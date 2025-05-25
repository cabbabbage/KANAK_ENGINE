#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "assets.hpp"
#include <SDL.h>
#include <string>
#include <unordered_set>

class Engine {
public:
    Engine(const std::string& map_json);
    ~Engine();

    void init();

private:
    void game_loop();
    void render_visible();

    const std::string map_json;
    SDL_Window* window;
    SDL_Renderer* renderer;
    Assets game_assets;

    static constexpr int SCREEN_WIDTH = 1280;
    static constexpr int SCREEN_HEIGHT = 720;
    static constexpr float BORDER_RATIO = 0.2f;
};

#endif
