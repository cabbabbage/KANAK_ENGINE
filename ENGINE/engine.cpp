#include "engine.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

Engine::Engine(const std::string& map_json)
    : map_json(map_json), window(nullptr), renderer(nullptr),
      game_assets(nullptr), SCREEN_WIDTH(0), SCREEN_HEIGHT(0),
      overlay_texture(nullptr), overlay_opacity(40),
      gif_frame_index(0), gif_timer(0), gif_delay(150),
      background_color({144, 238, 144, 255}) {}

Engine::~Engine() {
    if (game_assets) delete game_assets;
    if (overlay_texture) SDL_DestroyTexture(overlay_texture);
    for (auto tex : gif_frames) SDL_DestroyTexture(tex);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

SDL_Color get_random_earth_tone() {
    std::vector<SDL_Color> earth_tones = {
        {107, 142, 35, 255},   // OliveDrab
        {47, 79, 79, 255},     // DarkSlateGray
        {112, 128, 144, 255},  // SlateGray
        {143, 151, 121, 255},  // Moss green
        {100, 120, 80, 255},   // Lichen
        {144, 160, 144, 255},  // Soft green-gray
        {90, 100, 70, 255},    // Dull forest

    };
    return earth_tones[rand() % earth_tones.size()];
}

SDL_BlendMode get_random_blend_mode() {
    SDL_BlendMode modes[] = {
        SDL_BLENDMODE_NONE,
        SDL_BLENDMODE_BLEND,
        SDL_BLENDMODE_ADD,
        SDL_BLENDMODE_MOD
    };
    return modes[rand() % 4];
}

SDL_Color get_random_overlay_tint() {
    // Slightly colored tints
    int r = 180 + rand() % 50;
    int g = 180 + rand() % 50;
    int b = 180 + rand() % 50;
    return SDL_Color{ Uint8(r), Uint8(g), Uint8(b), 255 };
}

void Engine::init() {
    srand(static_cast<unsigned int>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "[Engine] SDL_Init Error: " << SDL_GetError() << "\n";
        return;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[Engine] SDL_mixer init failed: " << Mix_GetError() << "\n";
    }

    window = SDL_CreateWindow("Game Window",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "[Engine] Failed to create window: " << SDL_GetError() << "\n";
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "[Engine] Failed to create renderer: " << SDL_GetError() << "\n";
        return;
    }

    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    background_color = get_random_earth_tone();

    // === Load static overlay (PNG) with random blend & tint ===
    SDL_Surface* surface = IMG_Load("SRC/overlay.png");
    if (surface) {
        overlay_texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (overlay_texture) {
            SDL_SetTextureBlendMode(overlay_texture, get_random_blend_mode());
            SDL_SetTextureAlphaMod(overlay_texture, overlay_opacity);

            SDL_Color tint = get_random_overlay_tint();
            SDL_SetTextureColorMod(overlay_texture, tint.r, tint.g, tint.b);
        }
    }

    // === Load center-aligned overlay ===
    SDL_Surface* center_surf = IMG_Load("SRC/overlay_center.png");
    if (center_surf) {
        overlay_center_texture = SDL_CreateTextureFromSurface(renderer, center_surf);
        SDL_FreeSurface(center_surf);

        if (overlay_center_texture) {
            SDL_SetTextureBlendMode(overlay_center_texture, SDL_BLENDMODE_ADD);  // Brighten effect
            SDL_SetTextureAlphaMod(overlay_center_texture, overlay_opacity - 20);
        }
    }

    // === Load animated grain overlay frames ===
    for (int i = 0;; ++i) {
        std::string path = "SRC/overlay_" + std::to_string(i) + ".png";
        if (!fs::exists(path)) break;

        SDL_Surface* frame_surf = IMG_Load(path.c_str());
        if (!frame_surf) continue;

        SDL_Texture* frame_tex = SDL_CreateTextureFromSurface(renderer, frame_surf);
        SDL_FreeSurface(frame_surf);
        if (!frame_tex) continue;

        SDL_SetTextureBlendMode(frame_tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(frame_tex, overlay_opacity);
        gif_frames.push_back(frame_tex);
    }

    if (!gif_frames.empty()) {
        gif_frame_index = rand() % gif_frames.size();
        gif_timer = SDL_GetTicks();
    }

    std::vector<std::string> audio_files;
    for (const auto& entry : fs::directory_iterator("SRC/audio")) {
        if (entry.path().extension() == ".wav") {
            audio_files.push_back(entry.path().string());
        }
    }

    if (!audio_files.empty()) {
        std::string selected = audio_files[rand() % audio_files.size()];
        background_music = Mix_LoadMUS(selected.c_str());
        if (background_music) {
            Mix_PlayMusic(background_music, -1);
            std::cout << "[Engine] Playing audio: " << selected << "\n";
        }
    }

    try {
        game_assets = new Assets(map_json, renderer);
        if (!game_assets->player) {
            throw std::runtime_error("[Engine] No player asset found.");
        }

        auto [min_x, min_y, max_x, max_y] = game_assets->map_area.get_bounds();
        int center_x = (min_x + max_x) / 2;
        int center_y = (min_y + max_y) / 2;
        game_assets->player->set_position(center_x, center_y);

    } catch (const std::exception& e) {
        std::cerr << "[Engine] Asset loading error: " << e.what() << "\n";
        return;
    }

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

        game_assets->update(keys);
        render_visible();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }
}

void Engine::render_visible() {
    SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g, background_color.b, 255);
    SDL_RenderClear(renderer);

    const float parallax_strength = 0.0015f;
    const float scale_boost = 0.4f;
    const float render_buffer_radius = 1500.0f;

    const int px = game_assets->player->pos_X;
    const int py = game_assets->player->pos_Y;

    auto [min_x, min_y, max_x, max_y] = game_assets->map_area.get_bounds();
    int map_width  = max_x - min_x;
    int map_height = max_y - min_y;

    float rel_x = float(px - min_x) / float(map_width);
    float rel_y = float(py - min_y) / float(map_height);

    int screen_anchor_x = static_cast<int>(rel_x * SCREEN_WIDTH);
    int screen_anchor_y = static_cast<int>(rel_y * SCREEN_HEIGHT);

    std::vector<const Asset*> visible;

    for (const auto& asset : game_assets->all) {
        int screen_x = asset.pos_X - px + screen_anchor_x;
        int screen_y = asset.pos_Y - py + screen_anchor_y;

        float dx = float(screen_x - SCREEN_WIDTH / 2);
        float dy = float(screen_y - SCREEN_HEIGHT / 2);
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= render_buffer_radius) {
            visible.push_back(&asset);
        }
    }

    std::sort(visible.begin(), visible.end(), [](const Asset* a, const Asset* b) {
        return a->z_index < b->z_index;
    });

    for (const Asset* asset : visible) {
        SDL_Texture* tex = asset->get_image();
        if (!tex) continue;

        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);

        int screen_x = asset->pos_X - px + screen_anchor_x;
        int screen_y = asset->pos_Y - py + screen_anchor_y;

        float x_offset = float(screen_x - SCREEN_WIDTH / 2);
        float y_factor = float(screen_y) / float(SCREEN_HEIGHT);
        y_factor = std::clamp(y_factor, 0.0f, 1.0f);

        float direction = x_offset >= 0 ? 1.0f : -1.0f;
        float outward_magnitude = std::pow(std::abs(x_offset), 1.5f);
        float parallax_shift = parallax_strength * outward_magnitude * y_factor * direction;

        float scale = 1.0f + scale_boost * y_factor;
        int scaled_w = static_cast<int>(w * scale);
        int scaled_h = static_cast<int>(h * scale);

        SDL_Rect dest = {
            static_cast<int>(screen_x - scaled_w / 2 + parallax_shift),
            static_cast<int>(screen_y - scaled_h),
            scaled_w,
            scaled_h
        };

        SDL_RenderCopy(renderer, tex, nullptr, &dest);
    }

    // === STATIC MULTIPLY OVERLAY ===
    if (overlay_texture) {
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_MOD);
        SDL_SetTextureAlphaMod(overlay_texture, overlay_opacity);
        SDL_Rect full = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderCopy(renderer, overlay_texture, nullptr, &full);
    }

    // === ANIMATED SCREEN-BLEND GRAIN ===
    if (!gif_frames.empty()) {
        Uint32 now = SDL_GetTicks();
        if (now - gif_timer >= gif_delay) {
            gif_timer = now;
            gif_frame_index = (gif_frame_index + 1) % gif_frames.size();
        }
        SDL_Texture* frame = gif_frames[gif_frame_index];
        SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(frame, overlay_opacity);
        SDL_Rect full = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderCopy(renderer, frame, nullptr, &full);
    }
    if (overlay_center_texture && game_assets->player) {
        int player_x = game_assets->player->pos_X;
        int player_y = game_assets->player->pos_Y;

        auto [min_x, min_y, max_x, max_y] = game_assets->map_area.get_bounds();
        int map_width  = max_x - min_x;
        int map_height = max_y - min_y;

        float rel_x = float(player_x - min_x) / float(map_width);
        float rel_y = float(player_y - min_y) / float(map_height);

        int screen_anchor_x = static_cast<int>(rel_x * SCREEN_WIDTH);
        int screen_anchor_y = static_cast<int>(rel_y * SCREEN_HEIGHT);

        int w, h;
        SDL_QueryTexture(overlay_center_texture, nullptr, nullptr, &w, &h);

        SDL_Rect center_dest = {
            screen_anchor_x - w / 2,
            screen_anchor_y - h + 220,
            w,
            h
        };

        SDL_SetTextureBlendMode(overlay_center_texture, SDL_BLENDMODE_ADD); // Brighten behind
        SDL_RenderCopy(renderer, overlay_center_texture, nullptr, &center_dest);
    }

    SDL_RenderPresent(renderer);
}
