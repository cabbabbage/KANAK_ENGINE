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
    const std::string map_path = "MAPS/FORREST";



    Engine engine(map_path);
    engine.init();
}
