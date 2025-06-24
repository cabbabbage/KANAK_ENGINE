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

namespace fs = std::filesystem;
static constexpr SDL_Color SLATE_COLOR = {11, 15, 20, 250};

Engine::Engine(const std::string& map_path)
    : map_path(map_path),
      window(nullptr),
      renderer(nullptr),
      game_assets(nullptr),
      SCREEN_WIDTH(0),
      SCREEN_HEIGHT(0),
      background_color({69, 101, 74, 155}),
      overlay_texture(nullptr) {}

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

void Engine::init() {
    srand(static_cast<unsigned int>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return;
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    if (TTF_Init() < 0) return;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return;

    window = SDL_CreateWindow("Game Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) return;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return;
    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    overlay_texture = IMG_LoadTexture(renderer, (map_path + "/over.png").c_str());
    if (overlay_texture) {
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(overlay_texture, static_cast<Uint8>(255 * 0.2));
    }

    try {
        AssetLoader loader(map_path, renderer);
        roomTrailAreas = loader.getAllRoomAndTrailAreas();
        for (auto& area : roomTrailAreas)
            area.set_color(background_color.r, background_color.g, background_color.b);

        std::vector<Asset> assets = loader.extract_all_assets();
        Asset* player_ptr = nullptr;
        for (auto& a : assets)
            if (a.info && a.info->type == "Player") { player_ptr = &a; break; }
        if (!player_ptr) throw std::runtime_error("No player asset found.");

        game_assets = new Assets(std::move(assets), player_ptr,
                                 SCREEN_WIDTH, SCREEN_HEIGHT,
                                 player_ptr->pos_X, player_ptr->pos_Y);


    } catch (const std::exception& e) {
        std::cerr << "[Engine] Error: " << e.what() << "\n";
        return;
    }
    GenerateBaseShadow(renderer, roomTrailAreas, game_assets);
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
    SDL_SetRenderDrawColor(renderer, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer);

    const int cx = SCREEN_WIDTH / 2;
    const int cy = SCREEN_HEIGHT / 2;
    const int px = game_assets->player->pos_X;
    const int py = game_assets->player->pos_Y;

    for (const auto& [tex, dst] : static_faded_areas) {
        SDL_Rect shifted = { dst.x - px + cx, dst.y - py + cy - 40, dst.w, dst.h };
        SDL_RenderCopy(renderer, tex, nullptr, &shifted);
    }

    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;

        SDL_Texture* base = asset->get_current_frame();
        if (!base) continue;

        int w, h;
        SDL_QueryTexture(base, nullptr, nullptr, &w, &h);
        SDL_Rect dest = {
            asset->pos_X - px + cx - w / 2,
            asset->pos_Y - py + cy - h,
            w, h
        };

        // Draw base frame
        SDL_SetTextureBlendMode(base, asset->info->blendmode);
        SDL_SetTextureColorMod(base, 255, 255, 255);
        SDL_SetTextureAlphaMod(base, 255);
        SDL_RenderCopy(renderer, base, nullptr, &dest);

        // Apply two gradient passes
        if (asset->gradient_opacity > 0) {
            SDL_Texture* final_texture = base;

            // First pass: fade from full black to transparent
            ShadowOverlay overlay1(renderer);
            overlay1.set_main_color({0, 0, 0, 0});
            overlay1.set_secondary_color({0, 0, 0, 255});
            overlay1.set_direction(270);
            overlay1.set_opacity(static_cast<Uint8>(asset->gradient_opacity * 255 / 100));
            overlay1.set_intensity(255);
            overlay1.set_blend_mode(SDL_BLENDMODE_BLEND);
            SDL_Texture* shaded1 = overlay1.apply(final_texture);
            if (final_texture != base) SDL_DestroyTexture(final_texture);
            final_texture = shaded1;

            // Second pass: fade from transparent to full black
            ShadowOverlay overlay2(renderer);
            overlay2.set_main_color({230, 0, 30, 200});
            overlay2.set_secondary_color({0, 0, 0, 240});
            overlay2.set_direction(90);
            overlay2.set_opacity(static_cast<Uint8>(asset->gradient_opacity * 255 / 75));
            overlay2.set_intensity(255);
            overlay2.set_blend_mode(SDL_BLENDMODE_BLEND);
            SDL_Texture* shaded2 = overlay2.apply(final_texture);
            SDL_DestroyTexture(final_texture);
            final_texture = shaded2;

            // Render combined texture
            SDL_SetTextureBlendMode(final_texture, asset->info->blendmode);
            SDL_SetTextureColorMod(final_texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(final_texture, 255);
            SDL_RenderCopy(renderer, final_texture, nullptr, &dest);

            if (final_texture != base) SDL_DestroyTexture(final_texture);
        }
    }

    // Optional overlay
    if (overlay_texture) {
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderCopy(renderer, overlay_texture, nullptr, &full);
    }

    SDL_RenderPresent(renderer);
}
