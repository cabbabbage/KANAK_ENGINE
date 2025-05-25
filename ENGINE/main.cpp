#include "main.hpp"
#include "engine.hpp"

int main(int argc, char* argv[]) {
    run();
    return 0;
}

void run() {
    const std::string map_path = "C:\\Users\\cal_m\\OneDrive\\Documents\\GitHub\\tarot_game\\SRC\\Map.json";
    Engine engine(map_path);
    engine.init();
}
