#include "Engine.hpp"
#include <iostream>
#include <string>

namespace {
void printUsage(const char* executable) {
    std::cout << "Usage: " << executable << " [--agent-debug]\n"
              << "  --agent-debug  Run the line-oriented agent debug prompt.\n"
              << "  --help         Show this help.\n";
}
}

int main(int argc, char** argv) {
    bool agentDebugMode = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--agent-debug") {
            agentDebugMode = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 2;
        }
    }

    Engine engine;
    engine.setAgentDebugMode(agentDebugMode);
    
    if (!engine.initialize(1024, 768, "GTA-Style Game")) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }
    
    engine.run();
    
    return 0;
}
