#include <iostream>
#include <string>
#include <thread>
#include "httplib.h"
#include "concurrent_queue.hpp"

int main() {
    // Max capacity of 10,000 items
    ConcurrentQueue<std::string> metrics_queue(10000);
    
    // cpp-httplib Server inherently uses a thread pool to handle concurrent requests
    httplib::Server svr;

    svr.Post("/metrics", [&metrics_queue](const httplib::Request& req, httplib::Response& res) {
        // Enqueue the received batched metrics
        metrics_queue.push(req.body);
        
        // Print the received metrics to stdout to verify ingestion
        std::cout << "[Broker] Received metrics block:\n" << req.body << std::endl;
        std::cout << "[Broker] Queue size: " << metrics_queue.size() << "\n" << std::endl;
        
        res.set_content("Metrics accepted", "text/plain");
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

    std::cout << "Starting Central Ingestion Broker on port 8080..." << std::endl;
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server. Port 8080 might be in use." << std::endl;
        return 1;
    }

    consumer_thread.join();
    return 0;
}
