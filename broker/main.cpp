#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include "httplib.h"
#include "concurrent_queue.hpp"

// Global metric counters for Prometheus
std::atomic<uint64_t> messages_received_total{0};

int main() {
    ConcurrentQueue<std::string> metrics_queue(10000);
    httplib::Server svr;

    // POST /metrics: Receives data from Telemetry Agents
    svr.Post("/metrics", [&metrics_queue](const httplib::Request& req, httplib::Response& res) {
        messages_received_total.fetch_add(1, std::memory_order_relaxed);
        metrics_queue.push(req.body);
        res.set_content("Metrics accepted", "text/plain");
    });

    // GET /metrics: Prometheus scraper endpoint returning Exposition Format
    svr.Get("/metrics", [&metrics_queue](const httplib::Request& req, httplib::Response& res) {
        std::string prom_metrics = 
            "# HELP messages_received_total Total telemetry batches received\n"
            "# TYPE messages_received_total counter\n"
            "messages_received_total " + std::to_string(messages_received_total.load()) + "\n"
            "# HELP queue_size Current size of the concurrent queue\n"
            "# TYPE queue_size gauge\n"
            "queue_size " + std::to_string(metrics_queue.size()) + "\n"
            "# HELP messages_dropped_total Total telemetry batches dropped due to backpressure\n"
            "# TYPE messages_dropped_total counter\n"
            "messages_dropped_total " + std::to_string(metrics_queue.dropped_count()) + "\n";
        
        res.set_content(prom_metrics, "text/plain");
    });

    // Create a consumer thread to forward metrics to the Python ML Processor
    std::thread consumer_thread([&metrics_queue]() {
        const char* ml_url_env = std::getenv("ML_PROCESSOR_URL");
        std::string ml_url = ml_url_env ? ml_url_env : "http://localhost:5000";
        
        std::string host = "localhost";
        int port = 5000;
        if (ml_url.find("http://") == 0) {
            auto colon_pos = ml_url.find(':', 7);
            if (colon_pos != std::string::npos) {
                host = ml_url.substr(7, colon_pos - 7);
                port = std::stoi(ml_url.substr(colon_pos + 1));
            } else {
                host = ml_url.substr(7);
                port = 80;
            }
        }
        
        httplib::Client cli(host, port);
        cli.set_connection_timeout(2);
        cli.set_read_timeout(2);

        while (true) {
            auto item = metrics_queue.pop();
            if (item) {
                auto res = cli.Post("/analyze", *item, "application/json");
                if (!res || res->status != 200) {
                    std::cerr << "[Broker] Failed to forward to ML Processor at " << host << ":" << port << std::endl;
                }
            }
        }
    });

    std::cout << "Starting Central Ingestion Broker on port 8080 (Prometheus at GET /metrics)..." << std::endl;
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server. Port 8080 might be in use." << std::endl;
        return 1;
    }

    consumer_thread.join();
    return 0;
}
