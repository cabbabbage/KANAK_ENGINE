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
    map_light = get_map_light();
    try {
        AssetLoader loader(map_path, renderer);

        // grab and tint the room/trail areas
        roomTrailAreas = loader.getAllRoomAndTrailAreas();
        for (auto& area : roomTrailAreas) {
            area.set_color(background_color.r, background_color.g, background_color.b);
        }

        // pull back all assets by value
        std::vector<Asset> assets = loader.extract_all_assets();

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


   // std::cout << "Generating Fade Textures\n ********************************************* \n";
   // {
        //FadeTextureGenerator fade_gen(renderer, background_color, 1);  // expand factor can be tweaked
       // static_faded_areas.clear();
     //   for (const auto& [tex, rect] : fade_gen.generate_all(roomTrailAreas)) {
         //   static_faded_areas[tex] = rect;
   //     }

 //   }

    std::cout << "Generating Base Shadows\n ********************************************* \n";
    GenerateBaseShadow(renderer, roomTrailAreas, game_assets);

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
    static float shake_speed = 0.05f;      // Very slow
    static float shake_timer = 0.0f;
    static int last_px = 0;
    static int last_py = 0;

    const float MIN_SHAKE_INTENSITY = 0.0f;
    const float MAX_SHAKE_INTENSITY = 0.0f;
    const float MIN_SHAKE_SPEED = 0.0f;
    const float MAX_SHAKE_SPEED = 0.0f;

    const int px = game_assets->player->pos_X;
    const int py = game_assets->player->pos_Y;

    // === Adjust shake based on movement ===
    if (px != last_px || py != last_py) {
        shake_intensity = std::max(MIN_SHAKE_INTENSITY, shake_intensity * 0.97f);
        shake_speed = std::max(MIN_SHAKE_SPEED, shake_speed * 0.9f);
    } else {
        shake_intensity = std::min(MAX_SHAKE_INTENSITY, shake_intensity * 1.03f);
        shake_speed = std::min(MAX_SHAKE_SPEED, shake_speed * 1.05f);
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

    float norm_y = std::clamp(y_offset / 1000.0f, -1.0f, 1.0f); // -1 = top, 1 = bottom
    float norm_x = std::clamp(x_offset / 1000.0f, -1.0f, 1.0f); // -1 = left, 1 = right

    // === Control points: (L, R, T, B) for key areas ===
    struct EdgeScales { float L, R, T, B; };


    //const EdgeScales bottom_left   = { 1.1f, 1.1f, 0.8f, 1.0f };
    //const EdgeScales top_left      = { 0.6f, 0.6f, 2.0f, 1.0f };
    
    
    const EdgeScales bottom_middle  = { 1.0f, 1.0f, .7f, 1.0f };
    const EdgeScales top_middle     = { .8f, .8f, 1.1f, .9f };
    

    const EdgeScales bottom_left    = bottom_middle; 
    const EdgeScales middle_left    = { .9f, .9f, 1.0f, .95f };
    const EdgeScales top_left       = top_middle;

    
    auto lerp = [](float a, float b, float t) {
        return a + (b - a) * t;
    };

    auto lerp_edges = [&](const EdgeScales& a, const EdgeScales& b, float t) -> EdgeScales {
        return {
            lerp(a.L, b.L, t),
            lerp(a.R, b.R, t),
            lerp(a.T, b.T, t),
            lerp(a.B, b.B, t)
        };
    };

    EdgeScales left_interp, middle_interp;

    if (norm_y < 0.0f) {
        float t = norm_y + 1.0f; // -1 to 0 → 0 to 1
        left_interp   = lerp_edges(top_left, middle_left, t);
        middle_interp = lerp_edges(top_middle, middle_left, t);
    } else {
        float t = norm_y; // 0 to 1
        left_interp   = lerp_edges(middle_left, bottom_left, t);
        middle_interp = lerp_edges(middle_left, bottom_middle, t);
    }

    float tx = (norm_x + 1.0f) * 0.5f; // -1 to 1 → 0 to 1
    EdgeScales s = lerp_edges(left_interp, middle_interp, tx);

    if (norm_x > 0.0f) std::swap(s.L, s.R); // flip if on right side

    float scale_x = (s.L + s.R) * 0.5f;
    float scale_y = (s.T + s.B) * 0.5f;

    return { scale_x, scale_y };
};



    auto apply_parallax = [&](int ax, int ay) -> SDL_Point {
        float y_offset = static_cast<float>(ay - py);
        float x_offset = static_cast<float>(ax - px);

        float strength_y = std::clamp(y_offset / 2000.0f, -1.0f, 1.0f);
        float strength_x = std::clamp(x_offset / 2000.0f, -1.0f, 1.0f);

        int screen_x = ax - px + cx + static_cast<int>(strength_x * 40.0f); // horizontal slide
        int screen_y = ay - py + cy + static_cast<int>(strength_y * 20.0f); // vertical slight float

        return {screen_x, screen_y};
    };


    SDL_SetRenderDrawColor(renderer, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer);

    for (const auto& [tex, dst] : static_faded_areas) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_Point p = apply_parallax(dst.x, dst.y);
        SDL_Rect shifted = {p.x, p.y - 40, dst.w, dst.h};
        SDL_RenderCopy(renderer, tex, nullptr, &shifted);
    }

    if (map_light->current_color_.a < dusk_thresh) {
        for (const auto* asset : game_assets->active_assets) {
            if (!asset || !asset->is_lit || asset->light_textures.empty() || asset->info->lights.empty())
                continue;

            const auto& lights = asset->info->lights;
            for (size_t i = 0; i < asset->light_textures.size() && i < lights.size(); ++i) {
                SDL_Texture* light = asset->light_textures[i];
                const auto& light_data = lights[i];

                int lw, lh;
                SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
                SDL_Point p = apply_parallax(asset->pos_X + light_data.offset_x,
                                            asset->pos_Y + light_data.offset_y);
                SDL_Rect highlight = {p.x - lw / 2, p.y - lh / 2, lw, lh};

                SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(light, 255, 235, 180);

                int flicker_alpha = light_data.flicker ? std::clamp(28 + (rand() % 12), 10, 48) : 32;
                float t = 1.0f - std::clamp(map_light->current_color_.a / dusk_thresh, 0.0f, 1.0f);
                SDL_SetTextureAlphaMod(light, static_cast<int>(flicker_alpha * t));

                render_light_distorted(renderer, light, highlight, SCREEN_WIDTH, SCREEN_HEIGHT);
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


        auto [scale_x, scale_y] = get_scale_factors(asset->pos_X, asset->pos_Y);

        // Calculate color darkness and opacity
        float curved_opacity = std::pow(asset->gradient_opacity, 1.2f);
        int darkness = static_cast<int>(255 * curved_opacity);
        if (asset->info->type == "Player" || asset->info->type == "player") {
            darkness = std::min(255, static_cast<int>(darkness * 3.0f));
        }

        SDL_Color mod_color = {
            static_cast<Uint8>(darkness),
            static_cast<Uint8>(darkness),
            static_cast<Uint8>(darkness),
            255
        };

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        // Use trapezoid rendering with locked bottom edge
        render_asset_with_trapezoid(renderer, tex, p.x, p.y, w, h, scale_x, scale_y, mod_color);


    }



    SDL_Texture* lightmap = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, lightmap);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
    SDL_RenderClear(renderer);

    for (const auto* asset : game_assets->active_assets) {
        if (!asset || !asset->is_lit || asset->light_textures.empty() || asset->info->lights.empty())
            continue;

        const auto& lights = asset->info->lights;
        for (size_t i = 0; i < asset->light_textures.size() && i < lights.size(); ++i) {
            SDL_Texture* light = asset->light_textures[i];
            const auto& light_data = lights[i];

            int lw, lh;
            SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
            SDL_Point p = apply_parallax(asset->pos_X + light_data.offset_x,
                                        asset->pos_Y + light_data.offset_y);
            SDL_Rect dest = {p.x - lw / 2, p.y - lh / 2, lw, lh};

            SDL_SetTextureBlendMode(light, SDL_BLENDMODE_ADD);
            int flicker_alpha = light_data.flicker ? 200 + (rand() % 56) : 255;
            SDL_SetTextureAlphaMod(light, flicker_alpha);

            if (asset->info->type == "Player" || asset->info->type == "player") {
                double angle = -0.05 * SDL_GetTicks() * 0.001;
                double degrees = angle * (180.0 / M_PI);
                SDL_RenderCopyEx(renderer, light, nullptr, &dest, degrees, nullptr, SDL_FLIP_NONE);
            } else {
                SDL_RenderCopy(renderer, light, nullptr, &dest);
            }
        }
    }


    map_light->update();
    if (map_light) {
        auto [lx, ly] = map_light->get_position();
        int size = SCREEN_WIDTH * 3 * 2;
        SDL_Rect dest = {lx - size / 2, ly - size / 2, size, size};
        SDL_Texture* tex = map_light->get_texture();
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(tex, map_light->current_color_.r, map_light->current_color_.g, map_light->current_color_.b);
            SDL_SetTextureAlphaMod(tex, map_light->current_color_.a);
            SDL_RenderCopy(renderer, tex, nullptr, &dest);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer, lightmap, nullptr, nullptr);
    SDL_DestroyTexture(lightmap);
    SDL_RenderPresent(renderer);
}
