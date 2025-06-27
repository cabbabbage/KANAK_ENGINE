// File: engine.hpp

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <SDL.h>
#include <SDL_image.h>          // ← for IMG_Animation
#include "area.hpp"
#include "Assets.hpp"
#include "fade_textures.hpp"

class Engine {
public:
    Engine(const std::string& map_path);
    ~Engine();

    void init();

private:
    std::string map_path;
    SDL_Window* window;
    SDL_Renderer* renderer;
    Assets* game_assets;
    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;

    SDL_Color background_color;
    SDL_Texture* overlay_texture;
    std::vector<Area> roomTrailAreas;

    std::unordered_map<SDL_Texture*, SDL_Rect> static_faded_areas;

    void generate_static_faded_areas();
    void game_loop();
    void render_visible();
    void render_asset_gradient(const Asset* asset, const SDL_Rect& dest);
};

#endif // ENGINE_HPP
