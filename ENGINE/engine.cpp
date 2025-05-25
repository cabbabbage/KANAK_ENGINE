// engine.cpp
#include "engine.hpp"
#include <iostream>
#include <algorithm>

Engine::Engine(const std::string& map_json)
    : map_json(map_json), window(nullptr), renderer(nullptr), game_assets("", nullptr) {}

Engine::~Engine() {
    for (auto& asset : game_assets.all) {
        if (asset.get_image()) SDL_DestroyTexture(asset.get_image());
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }

    window = SDL_CreateWindow("Basic Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!window || !renderer) {
        std::cerr << "Window or Renderer creation failed! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }

    game_assets = Assets(map_json, renderer);
    if (!game_assets.player) {
        std::cerr << "No Player asset found!\n";
        return;
    }

    game_loop();
}

void Engine::game_loop() {
    const int FRAME_DURATION_MS = 1000 / 24;
    bool quit = false;
    SDL_Event e;
    std::unordered_set<SDL_Keycode> keys_held;

    while (!quit) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                keys_held.insert(e.key.keysym.sym);
            } else if (e.type == SDL_KEYUP) {
                keys_held.erase(e.key.keysym.sym);
            }
        }

        game_assets.update(keys_held);
        render_visible();

        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DURATION_MS) {
            SDL_Delay(FRAME_DURATION_MS - frame_time);
        }
    }
}

void Engine::render_visible() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int player_x = game_assets.player->pos_X;
    int player_y = game_assets.player->pos_Y;
    int center_x = SCREEN_WIDTH / 2;
    int center_y = SCREEN_HEIGHT / 2;

    float max_dx = SCREEN_WIDTH * (0.5f + BORDER_RATIO);
    float max_dy = SCREEN_HEIGHT * (0.5f + BORDER_RATIO);

    for (const auto& asset : game_assets.all) {
        int rel_x = asset.pos_X - player_x + center_x;
        int rel_y = asset.pos_Y - player_y + center_y;

        if (rel_x < -SCREEN_WIDTH * BORDER_RATIO || rel_x > SCREEN_WIDTH * (1.0f + BORDER_RATIO) ||
            rel_y < -SCREEN_HEIGHT * BORDER_RATIO || rel_y > SCREEN_HEIGHT * (1.0f + BORDER_RATIO)) {
            continue;
        }

        SDL_Rect dest = { rel_x, rel_y, 64, 64 }; // Assumes 64x64 sprites
        SDL_RenderCopy(renderer, asset.get_image(), nullptr, &dest);
    }

    SDL_RenderPresent(renderer);
}