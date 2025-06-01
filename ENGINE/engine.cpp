#include "engine.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <ctime>
#include <filesystem>
#include <SDL_ttf.h>
#include "asset_loader.hpp"

namespace fs = std::filesystem;

// Global flag to enable area drawing
bool testing = false;

Engine::Engine(const std::string& map_json)
    : map_json(map_json), window(nullptr), renderer(nullptr),
      game_assets(nullptr), SCREEN_WIDTH(0), SCREEN_HEIGHT(0),
      background_color({69, 101, 74, 255}), debug_font(nullptr) {}

Engine::~Engine() {
    if (debug_font) TTF_CloseFont(debug_font);
    if (game_assets) delete game_assets;
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
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
    std::cout << "[Engine] SDL initialized successfully.\n";

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[Engine] SDL_mixer init failed: " << Mix_GetError() << "\n";
    } else {
        std::cout << "[Engine] SDL_mixer initialized.\n";
    }

    if (TTF_Init() < 0) {
        std::cerr << "[Engine] SDL_ttf init failed: " << TTF_GetError() << "\n";
        return;
    }

    debug_font = TTF_OpenFont("assets/fonts/RobotoMono.ttf", 18);
    if (!debug_font) {
        std::cerr << "[Engine] Failed to load debug font: " << TTF_GetError() << "\n";
    }

    window = SDL_CreateWindow("Game Window",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cerr << "[Engine] Failed to create window: " << SDL_GetError() << "\n";
        return;
    }
    std::cout << "[Engine] Window created.\n";

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "[Engine] Failed to create renderer: " << SDL_GetError() << "\n";
        return;
    }
    std::cout << "[Engine] Renderer created.\n";

    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    std::cout << "[Engine] Renderer resolution: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << "\n";

    try {
        AssetLoader loader("engine/map.json", renderer);
        if (!loader.get_player()) throw std::runtime_error("No player asset found.");
        std::cout << "loading and spawning succsesfull\n";

        const auto& assets = loader.get_all_assets();

        std::cout << "[Engine] Loaded " << assets.size() << " assets:\n";
        for (const auto& a : assets) {
            if (!a || !a->info) {
                std::cout << "  - NULL or missing info\n";
                continue;
            }

            std::cout << "  - " << (a->info->name.empty() ? "Unnamed" : a->info->name)
                      << " @ (" << a->pos_X << "," << a->pos_Y << ")"
                      << " | type: " << a->info->type
                      << " | frames: "
                      << (a->info->animations.count("default") ? std::to_string(a->info->animations.at("default").frames.size()) : "none")
                      << "\n";
        }

        int px = loader.get_player()->pos_X;
        int py = loader.get_player()->pos_Y;


        game_assets = new Assets(
            std::move(const_cast<std::vector<std::unique_ptr<Asset>>&>(assets)),
            loader.get_player(),
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            px,
            py
        );


        std::cout << "[Engine] Player positioned at map center.\n";
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
            else if (e.type == SDL_KEYDOWN) {
                keys.insert(e.key.keysym.sym);
                std::cout << "[KeyDown] " << SDL_GetKeyName(e.key.keysym.sym) << "\n";
            }
            else if (e.type == SDL_KEYUP) {
                keys.erase(e.key.keysym.sym);
                std::cout << "[KeyUp] " << SDL_GetKeyName(e.key.keysym.sym) << "\n";
            }
        }

        int player_center_x = game_assets->player->pos_X;
        int player_center_y = game_assets->player->pos_Y;

        game_assets->update(keys, player_center_x, player_center_y);

        render_visible();

        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }
}

void Engine::render_visible() {
    SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g, background_color.b, 255);
    SDL_RenderClear(renderer);

    const int center_x = SCREEN_WIDTH / 2;
    const int center_y = SCREEN_HEIGHT / 2;
    const int player_x = game_assets->player->pos_X;
    const int player_y = game_assets->player->pos_Y;

    for (const auto* asset : game_assets->active_assets) {
        if (!asset) continue;

        // 1) Draw the sprite (bottom-center)
        SDL_Texture* tex = asset->get_image();
        if (tex) {
            int w_img, h_img;
            SDL_QueryTexture(tex, nullptr, nullptr, &w_img, &h_img);

            SDL_Rect dest_sprite = {
                asset->pos_X - player_x + center_x - (w_img / 2),
                asset->pos_Y - player_y + center_y - h_img,
                w_img,
                h_img
            };
            SDL_RenderCopy(renderer, tex, nullptr, &dest_sprite);
        }

        // 2) If testing, draw each Area in asset->areas
        if (testing) {
            for (const Area& area : asset->areas) {
                // area.points are already in world coordinates
                // get_image() returns a texture whose top-left corresponds to area.get_bounds().minx, miny
                SDL_Texture* area_tex = area.get_image(renderer);
                if (!area_tex) continue;

                auto [minx, miny, maxx, maxy] = area.get_bounds();
                int area_w = maxx - minx + 1;
                int area_h = maxy - miny + 1;

                SDL_Rect dest_area = {
                    minx - player_x + center_x,
                    miny - player_y + center_y,
                    area_w,
                    area_h
                };
                SDL_RenderCopy(renderer, area_tex, nullptr, &dest_area);
                SDL_DestroyTexture(area_tex);
            }
        }
    }

    render_debug_overlay();
    SDL_RenderPresent(renderer);
}


void Engine::render_debug_overlay() {
    if (!debug_font) return;

    int px = game_assets->player->pos_X;
    int py = game_assets->player->pos_Y;
    int active = static_cast<int>(game_assets->active_assets.size());

    std::string lines[] = {
        "DEBUG INFO",
        "Active assets: " + std::to_string(active),
        "Map pos: (" + std::to_string(px) + ", " + std::to_string(py) + ")",
        "Screen: " + std::to_string(SCREEN_WIDTH) + " x " + std::to_string(SCREEN_HEIGHT)
    };

    SDL_Color white = {255, 255, 255, 255};
    int y_offset = 10;

    for (const std::string& text : lines) {
        SDL_Surface* surface = TTF_RenderText_Blended(debug_font, text.c_str(), white);
        if (!surface) continue;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) continue;

        int tw, th;
        SDL_QueryTexture(texture, nullptr, nullptr, &tw, &th);
        SDL_Rect dest = {10, y_offset, tw, th};
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
        y_offset += th + 4;
    }
}
