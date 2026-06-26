#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <chrono>

struct SystemMetrics {
    double cpu_usage_percent;
    double mem_usage_percent;
    long long timestamp_ms;
};

class MetricsCollector {
public:
    MetricsCollector() {
        read_cpu_stats(prev_idle_, prev_total_);
    }

    SystemMetrics collect() {
        SystemMetrics metrics;
        metrics.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
        
        metrics.cpu_usage_percent = calculate_cpu_usage();
        metrics.mem_usage_percent = calculate_mem_usage();
        
        return metrics;
    }

private:
    unsigned long long prev_idle_ = 0;
    unsigned long long prev_total_ = 0;

    void read_cpu_stats(unsigned long long& idle, unsigned long long& total) {
        std::ifstream file("/proc/stat");
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string cpu_label;
            unsigned long long user, nice, system, idle_time, iowait, irq, softirq, steal;
            
            // Expected format: cpu  user nice system idle iowait irq softirq steal guest guest_nice
            if (iss >> cpu_label >> user >> nice >> system >> idle_time >> iowait >> irq >> softirq >> steal) {
                idle = idle_time + iowait;
                total = user + nice + system + idle_time + iowait + irq + softirq + steal;
            }
        }
    }

    double calculate_cpu_usage() {
        unsigned long long idle = 0, total = 0;
        read_cpu_stats(idle, total);
        
        unsigned long long total_diff = total - prev_total_;
        unsigned long long idle_diff = idle - prev_idle_;
        
        prev_total_ = total;
        prev_idle_ = idle;
        
        if (total_diff == 0) return 0.0;
        
        return 100.0 * (static_cast<double>(total_diff - idle_diff) / total_diff);
    }

    double calculate_mem_usage() {
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
