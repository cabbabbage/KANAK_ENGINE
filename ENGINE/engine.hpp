#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <string>
#include <unordered_set>
#include "assets.hpp"

class Engine {
public:
    Engine(const std::string& map_json);
    ~Engine();

    void init();

private:
    std::string map_json;
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* debug_font;

    Assets* game_assets;
    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;
    SDL_Color background_color;

    void game_loop();
    void render_visible();
    void render_debug_overlay();
};

#endif
