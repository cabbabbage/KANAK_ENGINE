// engine.hpp

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <SDL.h>
#include <SDL_image.h>
#include "area.hpp"
#include "Assets.hpp"
#include "fade_textures.hpp"
#include "generate_map_light.hpp"
#include "render_utils.hpp"

class Engine {
public:
    Engine(const std::string& map_path);
    ~Engine();

    void init();

private:
    void render_asset_with_trapezoid(SDL_Renderer* renderer, SDL_Texture* tex, int screen_x, int screen_y, int w, int h, float top_scale_x, float top_scale_y, SDL_Color color);
    float dusk_thresh = 80.0f;
    int brightness_level;
    std::string map_path;
    SDL_Window* window;
    SDL_Renderer* renderer;
    Assets* game_assets;
    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;
    SDL_Texture* minimap_texture_ = nullptr;
    SDL_Color background_color;
    SDL_Texture* overlay_texture;
    std::vector<Area> roomTrailAreas;
    RenderUtils util;
    std::unordered_map<SDL_Texture*, SDL_Rect> static_faded_areas;
    Generate_Map_Light* map_light;  // now a pointer

    Generate_Map_Light* get_map_light();  // declaration only
    void generate_static_faded_areas();
    void game_loop();
    void render_visible();

};

#endif // ENGINE_HPP
