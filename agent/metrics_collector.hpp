#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <functional>
#include <algorithm>

struct SystemMetrics {
    double cpu_usage_percent;
    double mem_usage_percent;
    long long timestamp_ms;
};

class MetricsCollector {
public:
    MetricsCollector() {
        const char* hostname_env = std::getenv("HOSTNAME");
        agent_id_ = hostname_env ? hostname_env : "agent-unknown";
        
        num_cores_ = std::thread::hardware_concurrency();
        if (num_cores_ == 0) num_cores_ = 1;

        if (read_cgroup_cpu_usage(prev_cpu_usage_usec_)) {
            use_cgroups_ = true;
        } else {
            use_cgroups_ = false;
            read_proc_cpu_stats(prev_idle_, prev_total_);
        }
        
        prev_time_usec_ = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    SystemMetrics collect() {
        SystemMetrics metrics;
        metrics.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
        
        if (use_cgroups_) {
            metrics.cpu_usage_percent = calculate_cgroup_cpu();
            metrics.mem_usage_percent = calculate_cgroup_mem();
        } else {
            metrics.cpu_usage_percent = calculate_proc_cpu();
            metrics.mem_usage_percent = calculate_proc_mem();
            apply_jitter(metrics.cpu_usage_percent, metrics.mem_usage_percent);
        }
        
        return metrics;
    }

private:
    std::string agent_id_;
    bool use_cgroups_;
    unsigned int num_cores_;
    
    // cgroup state
    unsigned long long prev_cpu_usage_usec_ = 0;
    unsigned long long prev_time_usec_ = 0;
    
    // proc fallback state
    unsigned long long prev_idle_ = 0;
    unsigned long long prev_total_ = 0;

    // Pseudo-random deterministic jitter based on agent_id to separate lines in Grafana
    void apply_jitter(double& cpu, double& mem) {
        std::hash<std::string> hasher;
        size_t hash = hasher(agent_id_);
        
        // Use hash to deterministically offset by up to +/- 0.5%
        double cpu_jitter = ((hash % 1000) / 1000.0) - 0.5;
        double mem_jitter = (((hash / 13) % 1000) / 1000.0) - 0.5;
        
        cpu = std::max(0.0, cpu + cpu_jitter);
        mem = std::max(0.0, mem + mem_jitter);
    }

    bool read_cgroup_cpu_usage(unsigned long long& usage_usec) {
        std::ifstream file("/sys/fs/cgroup/cpu.stat");
        if (!file.is_open()) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.compare(0, 11, "usage_usec ") == 0) {
                std::istringstream iss(line.substr(11));
                if (iss >> usage_usec) return true;
            }
        }
        return false;
    }

    double calculate_cgroup_cpu() {
        unsigned long long current_usage_usec = 0;
        if (!read_cgroup_cpu_usage(current_usage_usec)) return 0.0;
        
        unsigned long long current_time_usec = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
            
        unsigned long long delta_usage = current_usage_usec - prev_cpu_usage_usec_;
        unsigned long long delta_time = current_time_usec - prev_time_usec_;
        
        prev_cpu_usage_usec_ = current_usage_usec;
        prev_time_usec_ = current_time_usec;
        
        if (delta_time == 0) return 0.0;
        
        double cpu_percent = (static_cast<double>(delta_usage) / static_cast<double>(delta_time * num_cores_)) * 100.0;
        return std::min(100.0, std::max(0.0, cpu_percent));
    }

    double calculate_cgroup_mem() {
        std::ifstream current_file("/sys/fs/cgroup/memory.current");
        std::ifstream max_file("/sys/fs/cgroup/memory.max");
        
        if (!current_file.is_open() || !max_file.is_open()) return 0.0;
        
        unsigned long long current_mem = 0;
        current_file >> current_mem;
        
        std::string max_str;
        max_file >> max_str;
        
        unsigned long long max_mem = 0;
        if (max_str == "max" || max_str.empty()) {
            max_mem = get_host_total_memory();
        } else {
            try {
                max_mem = std::stoull(max_str);
            } catch (...) {
                max_mem = get_host_total_memory();
            }
        }
        
        if (max_mem == 0) return 0.0;
        
        return (static_cast<double>(current_mem) / max_mem) * 100.0;
    }
    
    unsigned long long get_host_total_memory() {
        std::ifstream file("/proc/meminfo");
        std::string line;
        while (std::getline(file, line)) {
            if (line.compare(0, 9, "MemTotal:") == 0) {
                std::istringstream iss(line);
                std::string key, unit;
                unsigned long long mem_total_kb = 0;
                iss >> key >> mem_total_kb >> unit;
                return mem_total_kb * 1024; // convert to bytes
            }
        }
        return 0; // Fallback failed
    }

    //Legacy /proc fallbacks for native non-cgroup execution
    void read_proc_cpu_stats(unsigned long long& idle, unsigned long long& total) {
        std::ifstream file("/proc/stat");
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string cpu_label;
            unsigned long long user, nice, system, idle_time, iowait, irq, softirq, steal;
            if (iss >> cpu_label >> user >> nice >> system >> idle_time >> iowait >> irq >> softirq >> steal) {
                idle = idle_time + iowait;
                total = user + nice + system + idle_time + iowait + irq + softirq + steal;
            }
        }
    }

    double calculate_proc_cpu() {
        unsigned long long idle = 0, total = 0;
        read_proc_cpu_stats(idle, total);
        
        unsigned long long total_diff = total - prev_total_;
        unsigned long long idle_diff = idle - prev_idle_;
        
        prev_total_ = total;
        prev_idle_ = idle;
        
        if (total_diff == 0) return 0.0;
        
        return 100.0 * (static_cast<double>(total_diff - idle_diff) / total_diff);
    }

    double calculate_proc_mem() {
        std::ifstream file("/proc/meminfo");
        std::string line;
        unsigned long long mem_total = 0, mem_available = 0;
        
        while (std::getline(file, line)) {
            if (line.compare(0, 9, "MemTotal:") == 0) {
                std::istringstream iss(line);
                std::string key, unit;
                iss >> key >> mem_total >> unit;
            } else if (line.compare(0, 13, "MemAvailable:") == 0) {
                std::istringstream iss(line);
                std::string key, unit;
                iss >> key >> mem_available >> unit;
            }
            if (mem_total > 0 && mem_available > 0) break;
        }
        
        if (mem_total == 0) return 0.0;
        
        return 100.0 * (static_cast<double>(mem_total - mem_available) / mem_total);
    }
};

#endif // METRICS_COLLECTOR_HPP
