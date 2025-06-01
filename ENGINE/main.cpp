#include "main.hpp"
#include "engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    std::cout << "Starting" << std::endl;
    run();
    return 0;
}

void run() {
    const std::string map_path = "SRC/Map.json";

    std::ifstream file(map_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << map_path << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::cout << "[Map.json contents]\n" << buffer.str() << std::endl;

    Engine engine(map_path);
    engine.init();
}
