// === File: engine.hpp ===
#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <vector>
#include <unordered_set>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "assets.hpp"
#include "asset.hpp"
#include "area.hpp"
#include <future>
#include <mutex>

class Engine {
public:
    explicit Engine(const std::string& map_path);
    ~Engine();

    void init();

private:
    void game_loop();
    void render_visible();
    void generate_static_faded_areas();

    std::string map_path;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Assets* game_assets = nullptr;

    int SCREEN_WIDTH = 0;
    int SCREEN_HEIGHT = 0;

    SDL_Color background_color;

    std::vector<Area> roomTrailAreas;
    std::vector<std::pair<SDL_Texture*, SDL_Rect>> static_faded_areas;
    bool static_faded_areas_ready = false;

    SDL_Texture* overlay_texture = nullptr;
};

#endif // ENGINE_HPP
