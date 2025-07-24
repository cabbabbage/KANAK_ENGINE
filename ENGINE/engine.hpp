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
#include "scene_renderer.hpp"

class Engine {
public:
    Engine(const std::string& map_path, SDL_Renderer* renderer, int screen_w, int screen_h);
    ~Engine();

    void init();

private:
    void render_asset_with_trapezoid(SDL_Renderer* renderer, SDL_Texture* tex, int screen_x, int screen_y, int w, int h, float top_scale_x, float top_scale_y, SDL_Color color);
    void generate_static_faded_areas();
    void game_loop();
    Generate_Map_Light* get_map_light();

    std::string map_path;
    SDL_Renderer* renderer;
    Assets* game_assets = nullptr;
    SDL_Texture* minimap_texture_ = nullptr;
    SDL_Texture* overlay_texture = nullptr;
    RenderUtils util;
    SceneRenderer* scene = nullptr;

    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;
    float dusk_thresh = 30.0f;
    int brightness_level = 0;
    SDL_Color background_color;
    std::vector<Area> roomTrailAreas;
    std::unordered_map<SDL_Texture*, SDL_Rect> static_faded_areas;
    Generate_Map_Light* map_light = nullptr;
};

#endif // ENGINE_HPP
