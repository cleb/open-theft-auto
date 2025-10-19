#include "Engine.hpp"
#include <iostream>

int main() {
    Engine engine;
    
    if (!engine.initialize(1024, 768, "GTA-Style Game")) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }
    
    engine.run();
    
    return 0;
}