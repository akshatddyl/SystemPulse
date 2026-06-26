#include <iostream>
#include <string>
#include "metrics_collector.hpp"
#include "ring_buffer.hpp"

int main() {
    MetricsCollector collector; 
    
    // Tell C++ this buffer will hold strings (or whatever struct the AI made)
    RingBuffer<std::string> rb(1024); 
    
    std::cout << "✅ Phase 1 headers compile and instantiate successfully!" << std::endl;
    return 0;
}
