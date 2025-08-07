// === File: main.cpp ===

#include "main.hpp"
#include "engine.hpp"
#include "rebuild_assets.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Force use of dedicated GPU on systems with hybrid graphics
extern "C" {
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    __declspec(dllexport) int NvOptimusEnablement = 0x00000001;
}

void run(const std::string& map_path, SDL_Renderer* renderer, int screen_w, int screen_h);

int main(int argc, char* argv[]) {
    std::cout << "[Main] Starting game engine...\n";

    const std::string map_path = "MAPS/FORREST";
    bool rebuild_cache = (argc > 1 && argv[1] && std::string(argv[1]) == "-r");

    // === SDL Subsystem Initialization ===
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "[Main] SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[Main] Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "[Main] TTF_Init failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "[Main] IMG_Init failed: " << IMG_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    // === Create Fullscreen Window ===
    SDL_Window* window = SDL_CreateWindow("Game Window",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          0, 0,
                                          SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "[Main] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return 1;
    }

    // === Create GPU-accelerated Renderer ===
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "[Main] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        IMG_Quit(); TTF_Quit(); SDL_Quit();
        return 1;
    }

    // === Output renderer info ===
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    std::cout << "[Main] Renderer: " << (info.name ? info.name : "Unknown") << "\n";

    int screen_width = 0, screen_height = 0;
    SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);
    std::cout << "[Main] Screen resolution: " << screen_width << "x" << screen_height << "\n";


    // === Optional: Rebuild Asset Cache ===
    if (rebuild_cache) {
        std::cout << "[Main] Rebuilding asset cache...\n";
        RebuildAssets* rebuilder = new RebuildAssets(renderer, map_path);
        delete rebuilder;
        std::cout << "[Main] Asset cache rebuild complete.\n";
    }

    // === Run Main Engine ===
    run(map_path, renderer, screen_width, screen_height);

    // === Cleanup ===
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    std::cout << "[Main] Game exited cleanly.\n";
    return 0;
}

void run(const std::string& map_path, SDL_Renderer* renderer, int screen_w, int screen_h) {
    Engine engine(map_path, renderer, screen_w, screen_h);
    engine.init();
}
