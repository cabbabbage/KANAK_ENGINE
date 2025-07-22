// File: engine.cpp

#include "engine.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <random>
#include <ctime>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include "asset_loader.hpp"
#include "fade_textures.hpp"
#include "generate_base_shadow.hpp"
#include "shadow_overlay.hpp"
#include <cmath>
#include <vector>
#include <utility>

namespace fs = std::filesystem;
static constexpr SDL_Color SLATE_COLOR = {30, 50, 32, 150};





Engine::Engine(const std::string& map_path)
    : map_path(map_path),
      window(nullptr),
      renderer(nullptr),
      game_assets(nullptr),
      SCREEN_WIDTH(0),
      SCREEN_HEIGHT(0),
      background_color({30, 50, 32, 150}),
      overlay_texture(nullptr),
      util(nullptr, 0, 0, nullptr)    // <-- initialize RenderUtils here
{}


Engine::~Engine() {
    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    for (auto& [tex, _] : static_faded_areas) if (tex) SDL_DestroyTexture(tex);
    delete game_assets;
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}




// === engine.cpp ===

void Engine::init() {
    srand(static_cast<unsigned int>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "[Engine] SDL_Init failed: " << SDL_GetError() << "\n";
        return;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[Engine] Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        return;
    }
    if (TTF_Init() < 0) {
        std::cerr << "[Engine] TTF_Init failed\n";
        return;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "[Engine] IMG_Init failed\n";
        return;
    }

    window = SDL_CreateWindow("Game Window",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "[Engine] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "[Engine] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return;
    }
    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);

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

        if (assets.size() > 100000) {
            throw std::runtime_error("Asset vector too large, possible duplication or leak");
        }

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

    util = RenderUtils(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, minimap_texture_);
    util.createMapLight();
    GenerateBaseShadow base = GenerateBaseShadow(renderer, roomTrailAreas, game_assets);
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
        render_visible();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }
}


void Engine::render_visible() {
    int px = game_assets->player ? game_assets->player->pos_X : 0;
    int py = game_assets->player ? game_assets->player->pos_Y : 0;
    util.updateCameraShake(px, py);

    SDL_SetRenderDrawColor(renderer,
        SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer);

    auto* map_light = util.getMapLight();

    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;

        // --- draw light BEFORE asset, using z-order ---
        if (asset->is_lit && asset->info &&
            map_light && map_light->current_color_.a > dusk_thresh &&
            !asset->info->light_textures.empty() &&
            !asset->info->lights.empty())
        {
            const auto& textures = asset->info->light_textures;
            const auto& lights = asset->info->lights;
            size_t count = std::min(textures.size(), lights.size());

            for (size_t i = 0; i < count; ++i) {
                SDL_Texture* light = textures[i];
                if (!light) continue;
                const auto& ld = lights[i];

                int lw, lh;
                SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
                SDL_Point lp = util.applyParallax(asset->pos_X + ld.offset_x,
                                                  asset->pos_Y + ld.offset_y);
                SDL_Rect hl{ lp.x - lw / 2, lp.y - lh / 2, lw, lh };

                SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(light, 255, 235, 180);
                int flick = ld.flicker ? std::clamp(28 + (rand() % 12), 10, 48) : 32;
                float t = 1.0f - std::clamp(map_light->current_color_.a / dusk_thresh, 0.0f, 1.0f);
                SDL_SetTextureAlphaMod(light, static_cast<int>(flick * t));

                util.setLightDistortionRect(hl);
                util.renderLightDistorted(light);
            }
        }

        // --- draw asset ---
        util.setAssetTrapezoid(asset, px, py);
        util.renderAssetTrapezoid(asset->get_current_frame());
    }

    // === Lightmap Pass ===
    SDL_Texture* lightmap = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        SCREEN_WIDTH, SCREEN_HEIGHT
    );
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, lightmap);
    SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
    SDL_RenderClear(renderer);

    for (const auto* asset : game_assets->active_assets) {
        if (!asset || !asset->is_lit || !asset->info) continue;

        if (!map_light || map_light->current_color_.a <= dusk_thresh) continue;

        const auto& textures = asset->info->light_textures;
        const auto& lights = asset->info->lights;
        size_t count = std::min(textures.size(), lights.size());

        for (size_t i = 0; i < count; ++i) {
            SDL_Texture* light = textures[i];
            if (!light) continue;
            const auto& ld = lights[i];

            int lw, lh;
            SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
            SDL_Point lp = util.applyParallax(asset->pos_X + ld.offset_x,
                                              asset->pos_Y + ld.offset_y);
            SDL_Rect dst{ lp.x - lw / 2, lp.y - lh / 2, lw, lh };

            SDL_SetTextureBlendMode(light, SDL_BLENDMODE_ADD);
            int flick = ld.flicker ? 200 + (rand() % 56) : 255;
            SDL_SetTextureAlphaMod(light, flick);

            if (asset->info->type == "Player" || asset->info->type == "player") {
                double angle = -0.05 * SDL_GetTicks() * 0.001;
                SDL_RenderCopyEx(renderer, light, nullptr, &dst,
                                 angle * (180.0 / M_PI), nullptr, SDL_FLIP_NONE);
            } else {
                SDL_RenderCopy(renderer, light, nullptr, &dst);
            }
        }
    }

    if (map_light) {
        map_light->update();
        auto [lx, ly] = map_light->get_position();
        int size = SCREEN_WIDTH * 3 * 2;
        SDL_Rect dest{ lx - size / 2, ly - size / 2, size, size };
        SDL_Texture* tex = map_light->get_texture();
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(tex,
                map_light->current_color_.r,
                map_light->current_color_.g,
                map_light->current_color_.b);
            SDL_SetTextureAlphaMod(tex, map_light->current_color_.a);
            SDL_RenderCopy(renderer, tex, nullptr, &dest);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer, lightmap, nullptr, nullptr);
    SDL_DestroyTexture(lightmap);

    util.renderMinimap();
    SDL_RenderPresent(renderer);
}
