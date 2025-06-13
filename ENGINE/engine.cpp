// File: engine.cpp
#include "engine.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <ctime>
#include <filesystem>
#include "asset_loader.hpp"
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

namespace fs = std::filesystem;

// Dark slate‐blue/gray color (RGB: 44, 62, 80)
static constexpr SDL_Color SLATE_COLOR = {11, 15, 20, 250};

Engine::Engine(const std::string& map_path)
    : map_path(map_path),
      window(nullptr),
      renderer(nullptr),
      game_assets(nullptr),
      SCREEN_WIDTH(0),
      SCREEN_HEIGHT(0),
      background_color({69, 101, 74, 155}),
      static_faded_areas_ready(false),
      overlay_texture(nullptr) {}

Engine::~Engine() {
    if (overlay_texture) {
        SDL_DestroyTexture(overlay_texture);
    }
    for (auto& [tex, _] : static_faded_areas) {
        if (tex) {
            SDL_DestroyTexture(tex);
        }
    }
    if (game_assets) {
        delete game_assets;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void Engine::init() {
    std::cout << "[Engine] Initializing engine...\n";
    srand(static_cast<unsigned int>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "[Engine] SDL_Init Error: " << SDL_GetError() << "\n";
        return;
    }
    std::cout << "[Engine] SDL initialized.\n";

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[Engine] SDL_mixer init failed: " << Mix_GetError() << "\n";
    } else {
        std::cout << "[Engine] SDL_mixer initialized.\n";
    }

    if (TTF_Init() < 0) {
        std::cerr << "[Engine] SDL_ttf init failed: " << TTF_GetError() << "\n";
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "[Engine] SDL_image PNG init failed: " << IMG_GetError() << "\n";
        return;
    }

    std::cout << "[Engine] Creating window...\n";
    window = SDL_CreateWindow("Game Window",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "[Engine] Failed to create window: " << SDL_GetError() << "\n";
        return;
    }
    std::cout << "[Engine] Window created.\n";

    std::cout << "[Engine] Creating renderer...\n";
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "[Engine] Failed to create renderer: " << SDL_GetError() << "\n";
        return;
    }
    std::cout << "[Engine] Renderer created.\n";

    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    std::cout << "[Engine] Resolution: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << "\n";

    std::cout << "[Engine] Loading overlay texture...\n";
    std::string overlay_path = map_path + "/over.png";
    overlay_texture = IMG_LoadTexture(renderer, overlay_path.c_str());
    if (!overlay_texture) {
        std::cerr << "[Engine] Overlay not found or failed to load.\n";
    } else {
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(overlay_texture, static_cast<Uint8>(255 * 0.6));
        std::cout << "[Engine] Overlay loaded.\n";
    }

    try {
        std::cout << "[Engine] Loading map assets...\n";
        AssetLoader loader(map_path, renderer);

        std::cout << "[Engine] Extracting room/trail areas...\n";
        roomTrailAreas = loader.getAllRoomAndTrailAreas();
        for (auto& area : roomTrailAreas) {
            area.set_color(background_color.r, background_color.g, background_color.b);
        }

        std::cout << "[Engine] Extracting asset list...\n";
        std::vector<Asset> assets = loader.extract_all_assets();
        std::cout << "[Engine] Total assets: " << assets.size() << "\n";

        std::cout << "[Engine] Computing background fade opacities...\n";
        for (auto& asset : assets) {
            if (!asset.info || asset.info->type != "Background") {
                asset.opacity = 100;
                continue;
            }

            double minDist = std::numeric_limits<double>::infinity();
            int x = asset.pos_X;
            int y = asset.pos_Y;

            for (const Area& zone : roomTrailAreas) {
                auto [minx, miny, maxx, maxy] = zone.get_bounds();
                double dx = 0.0, dy = 0.0;
                if (x < minx)       dx = static_cast<double>(minx - x);
                else if (x > maxx)  dx = static_cast<double>(x - maxx);
                if (y < miny)       dy = static_cast<double>(miny - y);
                else if (y > maxy)  dy = static_cast<double>(y - maxy);
                double dist = std::hypot(dx, dy);
                if (dist < minDist) minDist = dist;
            }

            if (minDist >= 200.0) {
                asset.opacity = 0;
            } else {
                asset.opacity = static_cast<int>(std::round(100.0 * (1.0 - (minDist / 150.0))));
                asset.opacity = std::clamp(asset.opacity, 0, 100);
            }
        }

        std::cout << "[Engine] Locating player asset...\n";
        Asset* player_ptr = nullptr;
        for (auto& a : assets) {
            if (a.info && a.info->type == "Player") {
                player_ptr = &a;
                break;
            }
        }
        if (!player_ptr) {
            throw std::runtime_error("No player asset found.");
        }

        int px = player_ptr->pos_X;
        int py = player_ptr->pos_Y;

        std::cout << "[Engine] Initializing Assets manager...\n";
        game_assets = new Assets(
            std::move(assets),
            player_ptr,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            px,
            py
        );

        std::cout << "[Engine] Finalizing background blend modes...\n";
        for (auto* asset : game_assets->active_assets) {
            if (asset && asset->info && asset->info->type == "Background") {
                if (SDL_Texture* tex = asset->get_image()) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                }
            }
        }

        std::cout << "[Engine] Player is positioned.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Engine] Asset loading error: " << e.what() << "\n";
        return;
    }

    generate_static_faded_areas();
    game_loop();
}

void Engine::generate_static_faded_areas() {
    if (static_faded_areas_ready) {
        std::cout << "[generate_static_faded_areas] Already generated; skipping.\n";
        return;
    }

    std::cout << "[generate_static_faded_areas] Starting generation for "
              << roomTrailAreas.size() << " areas...\n";

    const int expand = 1; // match the expand parameter passed below

    size_t index = 0;
    for (const Area& area : roomTrailAreas) {
        std::cout << "  [Area " << index << "] Computing fade texture...\n";

        SDL_Texture* tex = area.get_fade_texture(renderer, background_color, expand);
        if (!tex) {
            std::cout << "    [Area " << index << "] Failed to create texture; skipping.\n";
            ++index;
            continue;
        }

        auto [ominx, ominy, omaxx, omaxy] = area.get_bounds();
        int ow = (omaxx - ominx + 1);
        int oh = (omaxy - ominy + 1);

        float base_fade = 0.2f * static_cast<float>(std::min(ow, oh));
        if (base_fade < 1.0f) {
            base_fade = 1.0f;
        }

        float fade_width = base_fade * expand;
        int fw = static_cast<int>(std::ceil(fade_width));

        int expanded_minx = ominx - fw;
        int expanded_miny = ominy - fw;
        int expanded_maxx = omaxx + fw;
        int expanded_maxy = omaxy + fw;

        int tex_w = expanded_maxx - expanded_minx + 1;
        int tex_h = expanded_maxy - expanded_miny + 1;

        std::cout << "    [Area " << index 
                  << "] Expanded bounds = (" << expanded_minx << ", " << expanded_miny 
                  << ") to (" << expanded_maxx << ", " << expanded_maxy 
                  << "), size = " << tex_w << "×" << tex_h << "\n";

        SDL_Rect dst = { expanded_minx, expanded_miny, tex_w, tex_h };
        static_faded_areas.emplace_back(tex, dst);

        std::cout << "    [Area " << index << "] Texture stored.\n";
        ++index;
    }

    static_faded_areas_ready = true;
    std::cout << "[generate_static_faded_areas] Completed generation of "
              << static_faded_areas.size() << " textures.\n";
}

void Engine::game_loop() {
    const int FRAME_MS = 1000 / 30;
    bool quit = false;
    SDL_Event e;
    std::unordered_set<SDL_Keycode> keys;

    while (!quit) {
        Uint32 start = SDL_GetTicks();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                keys.insert(e.key.keysym.sym);
            } else if (e.type == SDL_KEYUP) {
                keys.erase(e.key.keysym.sym);
            }
        }

        int player_center_x = game_assets->player->pos_X;
        int player_center_y = game_assets->player->pos_Y;

        game_assets->update(keys, player_center_x, player_center_y);

        render_visible();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) {
            SDL_Delay(FRAME_MS - elapsed);
        }
    }
}

void Engine::render_visible() {
    // Clear screen with slate color instead of black
    SDL_SetRenderDrawColor(renderer,
                           SLATE_COLOR.r,
                           SLATE_COLOR.g,
                           SLATE_COLOR.b,
                           SLATE_COLOR.a);
    SDL_RenderClear(renderer);

    const int center_x = SCREEN_WIDTH / 2;
    const int center_y = SCREEN_HEIGHT / 2;
    const int player_x = game_assets->player->pos_X;
    const int player_y = game_assets->player->pos_Y;

    // Move faded textures up by 40px
    for (const auto& [tex, dst] : static_faded_areas) {
        SDL_Rect shifted = {
            dst.x - player_x + center_x,
            dst.y - player_y + center_y - 40,
            dst.w,
            dst.h
        };
        SDL_RenderCopy(renderer, tex, nullptr, &shifted);
    }

    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;

        SDL_Texture* tex = asset->get_image();
        if (!tex) continue;

        // Apply the asset's blend mode before drawing
        SDL_SetTextureBlendMode(tex, asset->info->blendmode);

        int w_img, h_img;
        SDL_QueryTexture(tex, nullptr, nullptr, &w_img, &h_img);

        SDL_Rect dest_sprite = {
            asset->pos_X - player_x + center_x - (w_img / 2),
            asset->pos_Y - player_y + center_y - h_img,
            w_img,
            h_img
        };

        // Draw the sprite normally
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
        SDL_RenderCopy(renderer, tex, nullptr, &dest_sprite);

        // Compute overlay strength: 0–1 based on inverted opacity
        float overlay_strength = (100.0f - asset->opacity) / 100.0f;
        if (overlay_strength <= 0.0f) {
            continue; // no gradient overlay needed
        }

        // Draw a slate‐to‐transparent gradient only over the sprite's opaque pixels
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        for (int row = 0; row < h_img; ++row) {
            float row_factor = static_cast<float>(row) / static_cast<float>(h_img - 1);
            Uint8 row_alpha = static_cast<Uint8>(
                std::round(overlay_strength * row_factor * 255.0f)
            );
            if (row_alpha == 0) continue;

            // Tint texture to slate color instead of black
            SDL_SetTextureColorMod(tex, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b);
            SDL_SetTextureAlphaMod(tex, row_alpha);

            SDL_Rect src_row = { 0, row, w_img, 1 };
            SDL_Rect dst_row = { dest_sprite.x, dest_sprite.y + row, w_img, 1 };
            SDL_RenderCopy(renderer, tex, &src_row, &dst_row);
        }

        // Reset texture color/alpha modulation for next use
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    }

    // Draw full-screen overlay if loaded
    if (overlay_texture) {
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderCopy(renderer, overlay_texture, nullptr, &full);
    }

    SDL_RenderPresent(renderer);
}
