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
static constexpr SDL_Color SLATE_COLOR = {69, 101, 74, 120};





Engine::Engine(const std::string& map_path)
    : map_path(map_path),
      window(nullptr),
      renderer(nullptr),
      game_assets(nullptr),
      SCREEN_WIDTH(0),
      SCREEN_HEIGHT(0),
      background_color({69, 101, 74, 120}),
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

    window = SDL_CreateWindow("Game Window",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

    std::cout << "Generating Fade Textures\n ********************************************* \n";
    {
        FadeTextureGenerator fade_gen(renderer, background_color, 1);  // expand factor can be tweaked
        static_faded_areas.clear();
        for (const auto& [tex, rect] : fade_gen.generate_all(roomTrailAreas)) {
            static_faded_areas[tex] = rect;
        }

    }

    std::cout << "Generating Base Shadows\n ********************************************* \n";
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



void render_light_distorted(SDL_Renderer* renderer, SDL_Texture* tex, SDL_Rect center_rect, int screen_w, int screen_h) {
    // Compute how close to the horizontal and vertical edges (normalized 0–1)
    float norm_x = std::abs(center_rect.x + center_rect.w / 2 - screen_w / 2) / float(screen_w / 2);
    float norm_y = std::abs(center_rect.y + center_rect.h / 2 - screen_h / 2) / float(screen_h / 2);

    // Overall distance-to-edge factor
    float edge_factor = std::max(norm_x, norm_y);

    // Apply mirrored distortion effect
    float scale = 1.0f + edge_factor * 0.25f;           // up to 1.25x bigger near corners
    float rotation = std::sin(SDL_GetTicks() * 0.002f) * edge_factor * 15.0f;  // up to ±15 degrees

    SDL_Rect scaled = center_rect;
    scaled.w = int(center_rect.w * scale);
    scaled.h = int(center_rect.h * scale);
    scaled.x = center_rect.x - (scaled.w - center_rect.w) / 2;
    scaled.y = center_rect.y - (scaled.h - center_rect.h) / 2;

    SDL_RenderCopyEx(renderer, tex, nullptr, &scaled, rotation, nullptr, SDL_FLIP_NONE);
}



void Engine::render_visible() {
    const int cx = SCREEN_WIDTH / 2;
    const int cy = SCREEN_HEIGHT / 2;
    const int px = game_assets->player->pos_X;
    const int py = game_assets->player->pos_Y;

    // === [1] Clear background to ambient dark color ===
    SDL_SetRenderDrawColor(renderer, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer);
   
    for (const auto& [tex, dst] : static_faded_areas) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);  // Ensure alpha blending
        SDL_Rect shifted = { dst.x - px + cx, dst.y - py + cy - 40, dst.w, dst.h };
        SDL_RenderCopy(renderer, tex, nullptr, &shifted);
    }



    // === [2] Draw soft light highlights *behind* each lit asset ===
    for (const auto* asset : game_assets->active_assets) {
        if (!asset || !asset->is_lit || !asset->light_texture) continue;

        SDL_Texture* light = asset->light_texture;
        int lw, lh;
        SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
        SDL_Rect highlight = {
            asset->pos_X - px + cx - lw / 2,
            asset->pos_Y - py + cy - lh / 2,
            lw, lh
        };

        SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);

        // Slight saturation boost: warm tint toward candlelight (orange-yellow)
        SDL_SetTextureColorMod(light, 255, 235, 180);

        // Flicker alpha with a slightly brighter base
        int flicker_alpha = 32;  // base alpha boost
        if (asset->info->flicker)
            flicker_alpha = std::clamp(28 + (rand() % 12), 10, 48);  // 28–48 range

        SDL_SetTextureAlphaMod(light, flicker_alpha);
        render_light_distorted(renderer, light, highlight, SCREEN_WIDTH, SCREEN_HEIGHT);
    }


    // === [3] Draw all assets with gradient-based darkness modulation ===
    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;

        SDL_Texture* tex = asset->get_current_frame();
        if (!tex) continue;

        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        SDL_Rect dest = {
            asset->pos_X - px + cx - w / 2,
            asset->pos_Y - py + cy - h,
            w, h
        };

        // Steepen falloff using power curve for stronger shadows
        float curved_opacity = std::pow(asset->gradient_opacity, 1.5f);
        int darkness = static_cast<int>(255 * curved_opacity);

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(tex, darkness, darkness, darkness);
        SDL_SetTextureAlphaMod(tex, 255);
        SDL_RenderCopy(renderer, tex, nullptr, &dest);
    }

    // === [4] Draw soft light highlights *in front* of each lit asset ===
    for (const auto* asset : game_assets->active_assets) {
        if (!asset || !asset->is_lit || !asset->light_texture) continue;

        SDL_Texture* light = asset->light_texture;
        int lw, lh;
        SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
        SDL_Rect highlight = {
            asset->pos_X - px + cx - lw / 2,
            asset->pos_Y - py + cy - lh / 2,
            lw, lh
        };

        SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(light, 255, 255, 255);

        // Flicker effect (optional)
        int flicker_alpha = 20;
        if (asset->info->flicker)
            flicker_alpha = std::max(10, 20 + (rand() % 10));

        SDL_SetTextureAlphaMod(light, flicker_alpha);
        render_light_distorted(renderer, light, highlight, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // === [5] Create full-screen lightmap ===
    SDL_Texture* lightmap = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, lightmap);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

for (const auto* asset : game_assets->active_assets) {
    if (!asset || !asset->is_lit || !asset->light_texture) continue;

    SDL_Texture* light = asset->light_texture;
    int lw, lh;
    SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
    SDL_Rect dest = {
        asset->pos_X - px + cx - lw / 2,
        asset->pos_Y - py + cy - lh / 2,
        lw, lh
    };

    SDL_SetTextureBlendMode(light, SDL_BLENDMODE_ADD);

    int flicker_alpha = 255;
    if (asset->info->flicker)
        flicker_alpha = 200 + (rand() % 56);  // 200–255

    if (asset->info->type == "Player" || asset->info->type == "player") {
        SDL_SetTextureAlphaMod(light, flicker_alpha);            // ← missing
        double angle = -0.05 * SDL_GetTicks() * 0.001;
        double degrees = angle * (180.0 / M_PI);
        SDL_RenderCopyEx(renderer, light, nullptr, &dest, degrees, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_SetTextureAlphaMod(light, flicker_alpha);
        SDL_RenderCopy(renderer, light, nullptr, &dest);
    }
}


    // === [7] Blend lightmap back over the scene using MOD ===
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer, lightmap, nullptr, nullptr);
    SDL_DestroyTexture(lightmap);

    // === [8] Optional overlay ===
    if (overlay_texture) {
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderCopy(renderer, overlay_texture, nullptr, &full);
    }

 

    SDL_RenderPresent(renderer);
}
