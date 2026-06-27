#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include "httplib.h"
#include "metrics_collector.hpp"
#include "ring_buffer.hpp"

// Simple JSON builder for the batched metrics
std::string to_json(const SystemMetrics& m, const std::string& agent_id) {
    return "{\"agent_id\":\"" + agent_id + "\",\"cpu_usage_percent\":" + std::to_string(m.cpu_usage_percent) + 
           ",\"mem_usage_percent\":" + std::to_string(m.mem_usage_percent) + 
           ",\"timestamp_ms\":" + std::to_string(m.timestamp_ms) + "}";
}

int main() {
    const char* broker_url_env = std::getenv("BROKER_URL");
    std::string broker_url = broker_url_env ? broker_url_env : "http://localhost:8080";
    
    // Grab the container hostname to uniquely identify this agent in Prometheus/Grafana
    const char* hostname_env = std::getenv("HOSTNAME");
    std::string agent_id = hostname_env ? hostname_env : "agent-unknown";
    
    // Parse host and port for cpp-httplib
    std::string host = "localhost";
    int port = 8080;
    if (broker_url.find("http://") == 0) {
        auto colon_pos = broker_url.find(':', 7);
        if (colon_pos != std::string::npos) {
            host = broker_url.substr(7, colon_pos - 7);
            port = std::stoi(broker_url.substr(colon_pos + 1));
        } else {
            host = broker_url.substr(7);
            port = 80;
        }
    }

    RingBuffer<SystemMetrics> buffer(1000);
    MetricsCollector collector;

    // Thread 1: Producer Thread
    std::thread producer([&buffer, &collector]() {
        while (true) {
            SystemMetrics m = collector.collect();
            buffer.push(m);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // Thread 2: Consumer (Network) Thread
    std::thread consumer([&buffer, host, port, agent_id]() {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(2);
        cli.set_read_timeout(2);

        while (true) {
            std::vector<SystemMetrics> batch;
            // Drain the concurrent ring buffer
            while (auto item = buffer.pop()) {
                batch.push_back(*item);
            }

            if (!batch.empty()) {
                // Build JSON array manually to avoid extra dependencies
                std::string payload = "[";
                for (size_t i = 0; i < batch.size(); ++i) {
                    payload += to_json(batch[i], agent_id);
                    if (i < batch.size() - 1) payload += ",";
                }
                payload += "]";

                auto res = cli.Post("/metrics", payload, "application/json");
                if (res) {
                    if (res->status != 200) {
                        std::cerr << "[Agent] Failed to post metrics, Broker returned status: " << res->status << std::endl;
                    } else {
                        std::cout << "[Agent] Successfully sent batch of " << batch.size() << " metrics to Broker." << std::endl;
                    }
                } else {
                    std::cerr << "[Agent] Connection to Broker failed at " << host << ":" << port << std::endl;
                }
            }
            // Wait before batching and sending the next chunk
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    std::cout << "Telemetry Agent [" << agent_id << "] started. Targeting Broker: " << broker_url << std::endl;

    producer.join();
    consumer.join();
    return 0;
}
