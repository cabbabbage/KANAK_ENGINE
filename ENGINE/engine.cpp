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

    map_light = get_map_light();

    try {
        AssetLoader loader(map_path, renderer);
        std::cout << "LOADING GOOD \n"; 
        // grab and tint the room/trail areas
        roomTrailAreas = loader.getAllRoomAndTrailAreas();

        std::cout << "AREAS GOOD \n"; 
        // pull back all assets by value
        std::vector<Asset> assets = loader.extract_all_assets();
        minimap_texture_ = loader.createMinimap(200, 200); 

        std::cout << "[Engine] Extracted " << assets.size() << " assets\n";

        if (assets.size() > 100000) {  // arbitrary large number threshold
            throw std::runtime_error("Asset vector too large, possible duplication or leak");
        }

        // Optional: Print counts by type for debugging
        std::unordered_map<std::string, int> type_counts;
        for (const auto& asset : assets) {
            if (asset.info) {
                ++type_counts[asset.info->type];
            }
        }
        for (const auto& [type, count] : type_counts) {
            std::cout << "  - " << type << ": " << count << "\n";
        }

        // find the player in that list
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

        // hand ownership of the assets (and the player pointer) off to the game
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

    std::cout << "Generating Base Shadows\n*********************************************\n";
    //GenerateBaseShadow(renderer, roomTrailAreas, game_assets);

    game_loop();
}


Generate_Map_Light* Engine::get_map_light() {
    SDL_Color base_color = {255, 255, 255, 255};
    return new Generate_Map_Light(
        renderer,
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT / 2,
        SCREEN_WIDTH,
        base_color,
        50,   // min opacity
        255   // max opacity
    );
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

void Engine::render_asset_with_trapezoid(SDL_Renderer* renderer, SDL_Texture* tex, int screen_x, int screen_y, int w, int h, float top_scale_x, float top_scale_y, SDL_Color color) {
    if (!tex) return;

    SDL_Vertex verts[4];
    int half_base_w = w / 2;

    int top_w = static_cast<int>(w * top_scale_x);
    int top_h = static_cast<int>(h * top_scale_y);
    int half_top_w = top_w / 2;

    // Lock bottom of the sprite (y aligned)
    int bottom_y = screen_y;
    int top_y = screen_y - top_h;

    verts[0].position = { static_cast<float>(screen_x - half_top_w), static_cast<float>(top_y) };     // Top Left
    verts[0].tex_coord = { 0.0f, 0.0f };

    verts[1].position = { static_cast<float>(screen_x + half_top_w), static_cast<float>(top_y) };     // Top Right
    verts[1].tex_coord = { 1.0f, 0.0f };

    verts[2].position = { static_cast<float>(screen_x + half_base_w), static_cast<float>(bottom_y) }; // Bottom Right
    verts[2].tex_coord = { 1.0f, 1.0f };

    verts[3].position = { static_cast<float>(screen_x - half_base_w), static_cast<float>(bottom_y) }; // Bottom Left
    verts[3].tex_coord = { 0.0f, 1.0f };

    for (int i = 0; i < 4; ++i) {
        verts[i].color = color;
    }

    int indices[] = { 0, 1, 2, 2, 3, 0 };
    SDL_RenderGeometry(renderer, tex, verts, 4, indices, 6);
}

void Engine::render_visible() {
    // === Camera Shake Parameters ===
    static float shake_intensity = 0.5f;   // Final motion capped by logic below
    static float shake_speed     = 0.05f;  // Very slow
    static float shake_timer     = 0.0f;
    static int   last_px         = 0;
    static int   last_py         = 0;

    const float MIN_SHAKE_INTENSITY = 0.0f;
    const float MAX_SHAKE_INTENSITY = 0.0f;
    const float MIN_SHAKE_SPEED     = 0.0f;
    const float MAX_SHAKE_SPEED     = 0.0f;

    const int px = game_assets->player->pos_X;
    const int py = game_assets->player->pos_Y;

    // === Adjust shake based on movement ===
    if (px != last_px || py != last_py) {
        shake_intensity = std::max(MIN_SHAKE_INTENSITY, shake_intensity * 0.97f);
        shake_speed     = std::max(MIN_SHAKE_SPEED,     shake_speed * 0.9f);
    } else {
        shake_intensity = std::min(MAX_SHAKE_INTENSITY, shake_intensity * 1.03f);
        shake_speed     = std::min(MAX_SHAKE_SPEED,     shake_speed * 1.05f);
    }

    last_px = px;
    last_py = py;

    // === Smooth, capped shake output ===
    shake_timer += shake_speed;
    float raw_x = std::sin(shake_timer * 0.7f) * shake_intensity;
    float raw_y = std::sin(shake_timer * 1.05f + 2.0f) * shake_intensity;

    // Clamp to no more than 1 pixel of movement
    int shake_x = static_cast<int>(std::clamp(raw_x, -1.0f, 1.0f));
    int shake_y = static_cast<int>(std::clamp(raw_y, -1.0f, 1.0f));

    const int cx = SCREEN_WIDTH / 2 + shake_x;
    const int cy = SCREEN_HEIGHT / 2 + shake_y;

    auto get_scale_factors = [&](int ax, int ay) -> std::pair<float, float> {
        float y_offset = static_cast<float>(ay - py);
        float x_offset = static_cast<float>(ax - px);

        float norm_y = std::clamp(y_offset / 1000.0f, -1.0f, 1.0f);
        float norm_x = std::clamp(x_offset / 1000.0f, -1.0f, 1.0f);

        struct EdgeScales { float L, R, T, B; };
        const EdgeScales bottom_middle = { 0.98f, 0.98f, 0.82f, 0.98f };
        const EdgeScales top_middle    = { 0.88f, 0.88f, 1.05f, 0.92f };
        const EdgeScales middle_left   = { 0.94f, 0.94f, 0.96f, 0.97f };
        const EdgeScales bottom_left   = bottom_middle;
        const EdgeScales top_left      = top_middle;
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        auto lerp_edges = [&](const EdgeScales& A, const EdgeScales& B, float t) -> EdgeScales {
            return {
                lerp(A.L, B.L, t),
                lerp(A.R, B.R, t),
                lerp(A.T, B.T, t),
                lerp(A.B, B.B, t)
            };
        };
        auto ease = [](float t) { return t * t * (3.0f - 2.0f * t); };

        EdgeScales left_interp, mid_interp;
        if (norm_y < 0.0f) {
            float t  = norm_y + 1.0f;          // -1→0 → 0→1
            float et = ease(t);
            left_interp = lerp_edges(top_left,  middle_left, et);
            mid_interp  = lerp_edges(top_middle, middle_left, et);
        } else {
            float t  = norm_y;                // 0→1
            float et = ease(t);
            left_interp = lerp_edges(middle_left, bottom_left, et);
            mid_interp  = lerp_edges(middle_left, bottom_middle, et);
        }

        float tx  = (norm_x + 1.0f) * 0.5f;    // -1→1 → 0→1
        float ex  = ease(tx);
        EdgeScales S = lerp_edges(left_interp, mid_interp, ex);

        if (norm_x > 0.0f) std::swap(S.L, S.R);

        return { (S.L + S.R) * 0.5f, (S.T + S.B) * 0.5f };
    };


    auto apply_parallax = [&](int ax, int ay) -> SDL_Point {
        // normalized offsets from center
        float dx = std::clamp((ax - px) / (SCREEN_WIDTH  * 0.5f), -1.0f, 1.0f);
        float dy = std::clamp((ay - py) / (SCREEN_HEIGHT * 0.5f), -1.0f, 1.0f);

        // horizontal shift scales with vertical position and left/right offset
        float hx = dx * dy;  
        float hy = dy;

        int maxX = 10;
        int maxY = 5;
        int offX = static_cast<int>(hx * maxX);
        int offY = static_cast<int>(hy * maxY);

        int screen_x = ax - px + cx + offX;
        int screen_y = ay - py + cy + offY;
        return { screen_x, screen_y };
    };


    SDL_SetRenderDrawColor(renderer, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer);

    for (const auto& [tex, dst] : static_faded_areas) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_Point p = apply_parallax(dst.x, dst.y);
        SDL_Rect shifted = { p.x, p.y - 40, dst.w, dst.h };
        SDL_RenderCopy(renderer, tex, nullptr, &shifted);
    }

    // only render per-asset lights when map_light alpha is above dusk_thresh
    if (map_light->current_color_.a > dusk_thresh) {
        for (const auto* asset : game_assets->active_assets) {
            if (!asset || !asset->is_lit || asset->light_textures.empty() || asset->info->lights.empty())
                continue;

            const auto& lights = asset->info->lights;
            for (size_t i = 0; i < asset->light_textures.size() && i < lights.size(); ++i) {
                SDL_Texture* light = asset->light_textures[i];
                const auto& ld = lights[i];

                int lw, lh;
                SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);

                SDL_Point lp = apply_parallax(asset->pos_X + ld.offset_x,
                                              asset->pos_Y + ld.offset_y);
                SDL_Rect hl = { lp.x - lw/2, lp.y - lh/2, lw, lh };

                SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(light, 255, 235, 180);
                int flick = ld.flicker ? std::clamp(28 + (rand() % 12), 10, 48) : 32;
                float t = 1.0f - std::clamp(map_light->current_color_.a / dusk_thresh, 0.0f, 1.0f);
                SDL_SetTextureAlphaMod(light, static_cast<int>(flick * t));

                render_light_distorted(renderer, light, hl, SCREEN_WIDTH, SCREEN_HEIGHT);
            }
        }
    }

    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;
        SDL_Texture* tex = asset->get_current_frame();
        if (!tex) continue;

        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        SDL_Point p = apply_parallax(asset->pos_X, asset->pos_Y);

        auto [sx, sy] = get_scale_factors(asset->pos_X, asset->pos_Y);

        float curved_opacity = std::pow(asset->gradient_opacity, 1.2f);
        int darkness = static_cast<int>(255 * curved_opacity);
        if (asset->info->type == "Player" || asset->info->type == "player") {
            darkness = std::min(255, darkness * 3);
        }

        SDL_Color mod_color = {
            static_cast<Uint8>(darkness),
            static_cast<Uint8>(darkness),
            static_cast<Uint8>(darkness),
            255
        };

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        render_asset_with_trapezoid(renderer, tex, p.x, p.y, w, h, sx, sy, mod_color);
    }

    SDL_Texture* lightmap = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, lightmap);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
    SDL_RenderClear(renderer);

    // only add per-asset lights into the lightmap when past dusk
    if (map_light->current_color_.a > dusk_thresh) {
        for (const auto* asset : game_assets->active_assets) {
            if (!asset || !asset->is_lit || asset->light_textures.empty() || asset->info->lights.empty())
                continue;

            const auto& lights = asset->info->lights;
            for (size_t i = 0; i < asset->light_textures.size() && i < lights.size(); ++i) {
                SDL_Texture* light = asset->light_textures[i];
                const auto& ld = lights[i];

                int lw, lh;
                SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);

                SDL_Point lp = apply_parallax(asset->pos_X + ld.offset_x,
                                              asset->pos_Y + ld.offset_y);
                SDL_Rect dst = { lp.x - lw/2, lp.y - lh/2, lw, lh };

                SDL_SetTextureBlendMode(light, SDL_BLENDMODE_ADD);
                int flick = ld.flicker ? 200 + (rand() % 56) : 255;
                SDL_SetTextureAlphaMod(light, flick);

                if (asset->info->type == "Player" || asset->info->type == "player") {
                    double angle = -0.05 * SDL_GetTicks() * 0.001;
                    double deg   = angle * (180.0 / M_PI);
                    SDL_RenderCopyEx(renderer, light, nullptr, &dst, deg, nullptr, SDL_FLIP_NONE);
                } else {
                    SDL_RenderCopy(renderer, light, nullptr, &dst);
                }
            }
        }
    }

    map_light->update();
    if (map_light) {
        auto [lx, ly] = map_light->get_position();
        int size = SCREEN_WIDTH * 3 * 2;
        SDL_Rect dest = { lx - size/2, ly - size/2, size, size };
        SDL_Texture* tex = map_light->get_texture();
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(tex,
                map_light->current_color_.r,
                map_light->current_color_.g,
                map_light->current_color_.b
            );
            SDL_SetTextureAlphaMod(tex, map_light->current_color_.a);
            SDL_RenderCopy(renderer, tex, nullptr, &dest);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer, lightmap, nullptr, nullptr);
    SDL_DestroyTexture(lightmap);

    // — Draw minimap in bottom-right —
    if (minimap_texture_) {
        int mw, mh;
        SDL_QueryTexture(minimap_texture_, nullptr, nullptr, &mw, &mh);
        SDL_Rect miniDst = {
            SCREEN_WIDTH - mw - 10,
            SCREEN_HEIGHT - mh - 10,
            mw, mh
        };
        SDL_SetTextureBlendMode(minimap_texture_, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(renderer, minimap_texture_, nullptr, &miniDst);
    }
    SDL_RenderPresent(renderer);
}
