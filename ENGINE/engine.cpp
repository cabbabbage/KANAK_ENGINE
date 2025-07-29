// === File: engine.cpp ===

#include "engine.hpp"
#include "asset_loader.hpp"
#include "fade_textures.hpp"
#include "generate_base_shadow.hpp"
#include "shadow_overlay.hpp"
#include "scene_renderer.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <random>
#include <ctime>
#include <cmath>
#include <utility>

namespace fs = std::filesystem;

Engine::Engine(const std::string& map_path, SDL_Renderer* renderer, int screen_w, int screen_h)
    : map_path(map_path),
      renderer(renderer),
      SCREEN_WIDTH(screen_w),
      SCREEN_HEIGHT(screen_h),
      background_color({20, 33, 21, 150}),
      overlay_texture(nullptr),
      minimap_texture_(nullptr),
      game_assets(nullptr),
      util(renderer, screen_w, screen_h, nullptr, map_path),
      scene(nullptr) {}

Engine::~Engine() {
    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    for (auto& [tex, _] : static_faded_areas)
        if (tex) SDL_DestroyTexture(tex);
    delete game_assets;
    delete scene;
}

void Engine::init() {
    srand(static_cast<unsigned int>(time(nullptr)));

    overlay_texture = IMG_LoadTexture(renderer, (map_path + "/over.png").c_str());
    if (overlay_texture) {
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(overlay_texture, static_cast<Uint8>(255 * 0.2));
    }

    try {
        AssetLoader loader(map_path, renderer);
        roomTrailAreas = loader.getAllRoomAndTrailAreas();

        std::vector<Asset> assets = loader.extract_all_assets();
        minimap_texture_ = loader.createMinimap(200, 200);

        std::unordered_map<std::string, int> type_counts;
        for (const auto& asset : assets) {
            if (asset.info) ++type_counts[asset.info->type];
        }
        for (const auto& [type, count] : type_counts) {
            std::cout << "  - " << type << ": " << count << "\n";
        }

        Asset* player_ptr = nullptr;
        for (auto& asset : assets) {
            if (asset.info && asset.info->type == "Player") {
                player_ptr = &asset;
                break;
            }
        }
        if (!player_ptr) {
            throw std::runtime_error("No player asset found.");
        }

        game_assets = new Assets(
            std::move(assets),
            player_ptr,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            player_ptr->pos_X,
            player_ptr->pos_Y
        );
    }
    catch (const std::exception& e) {
        std::cerr << "[Engine] Error: " << e.what() << "\n";
        return;
    }

    util = RenderUtils(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, minimap_texture_, map_path);
    util.createMapLight();
    scene = new SceneRenderer(renderer, game_assets, util, SCREEN_WIDTH, SCREEN_HEIGHT, map_path);
    GenerateBaseShadow base(renderer, roomTrailAreas, game_assets);
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
            else if (e.type == SDL_KEYUP) keys.erase(e.key.keysym.sym);
        }

        int px = game_assets->player->pos_X;
        int py = game_assets->player->pos_Y;

        game_assets->update(keys, px, py);
        scene->render();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }
}
