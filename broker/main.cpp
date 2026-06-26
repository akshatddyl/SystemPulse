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

    // Create a consumer thread to simulate ML processing or forwarding
    std::thread consumer_thread([&metrics_queue]() {
        while (true) {
            auto item = metrics_queue.pop();
            if (item) {
                // Here we would forward the data to the Python ML Processor.
                // For now, we just dequeue it so the queue doesn't fill up infinitely.
                // If the ML processor is slower than ingestion, the drop-oldest mechanism 
                // in concurrent_queue.hpp will protect us.
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
