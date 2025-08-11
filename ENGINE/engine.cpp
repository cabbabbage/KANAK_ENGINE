// === File: engine.cpp ===

#include "engine.hpp"
#include "fade_textures.hpp"

#include "shadow_overlay.hpp"
#include <iostream>
#include <filesystem>
#include <random>
#include <ctime>

namespace fs = std::filesystem;

Engine::Engine(const std::string& map_path,
               SDL_Renderer* renderer,
               int screen_w,
               int screen_h)
    : map_path(map_path),
      renderer(renderer),
      SCREEN_WIDTH(screen_w),
      SCREEN_HEIGHT(screen_h),
      boundary_color({20, 33, 21, 150}),
      overlay_texture(nullptr),
      minimap_texture_(nullptr),
      game_assets(nullptr),
      util(renderer, screen_w, screen_h, nullptr, map_path),
      scene(nullptr)
{}

Engine::~Engine() {
    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    for (auto& [tex, _] : static_faded_areas)
        if (tex) SDL_DestroyTexture(tex);
    delete game_assets;
    delete scene;
}

void Engine::init() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    try {
        loader_ = std::make_unique<AssetLoader>(map_path, renderer);

        roomTrailAreas = loader_->getAllRoomAndTrailAreas();

        minimap_texture_ = loader_->createMinimap(200, 200);

        auto assets_uptr = loader_->createAssets(SCREEN_WIDTH, SCREEN_HEIGHT);
        game_assets = assets_uptr.release();
    }
    catch (const std::exception& e) {
        std::cerr << "[Engine] Error: " << e.what() << "\n";
        return;
    }

    util = RenderUtils(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, minimap_texture_, map_path);
    scene = new SceneRenderer(renderer, game_assets, util, SCREEN_WIDTH, SCREEN_HEIGHT, map_path);
    //DistantAssetOpt base(renderer, roomTrailAreas, game_assets);

    std::cout << "\n\nENTERING GAME LOOP\n\n";
    game_loop();
}

void Engine::game_loop() {
    const int FRAME_MS = 1000 / 30;
    bool quit = false;
    SDL_Event e;
    std::unordered_set<SDL_Keycode> keys;

    while (!quit) {
        Uint32 start = SDL_GetTicks();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_KEYDOWN) keys.insert(e.key.keysym.sym);
            else if (e.type == SDL_KEYUP)   keys.erase(e.key.keysym.sym);
        }

        int px = game_assets->player->pos_X;
        int py = game_assets->player->pos_Y;

        game_assets->update(keys, px, py);
        scene->render();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }
}
