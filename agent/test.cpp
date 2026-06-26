#include <iostream>
#include <string>
#include "metrics_collector.hpp"
#include "ring_buffer.hpp"

int main() {
    MetricsCollector collector; 
    RingBuffer<std::string> rb(1024); 
    std::cout << "✅ headers compile and instantiate successfully!" << std::endl;
    return 0;
}
