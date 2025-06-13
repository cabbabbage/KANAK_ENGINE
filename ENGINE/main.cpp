#include "main.hpp"
#include "engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    std::cout << "Starting" << std::endl;

    std::string map_path = "MAPS/FORREST";
    if (argc > 1 && argv[1]) {
        map_path = argv[1];
    }

    run(map_path);
    return 0;
}

void run(const std::string& map_path) {
    Engine engine(map_path);
    engine.init();
}
