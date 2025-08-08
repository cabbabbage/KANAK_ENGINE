#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "Assets.hpp"
#include "render_utils.hpp"
#include "light_map.hpp"
#include "debug_area.hpp"
#include "generate_map_light.hpp"
#include <SDL.h>
#include <vector>
#include <tuple>
#include <string>

class SceneRenderer {
public:
    SceneRenderer(SDL_Renderer* renderer,
                  Assets* assets,
                  RenderUtils& util,
                  int screen_width,
                  int screen_height,
                  const std::string& map_path);

    void render();

private:
    SDL_Texture* fullscreen_light_tex_;
    SDL_Texture* generateMask(Asset* a, int bw, int bh);
    bool debugging = false;
    double player_shade_percent = 1.0;
    double calculate_static_alpha_percentage(int asset_y, int light_world_y);

    void render_asset_lights_z();

    void renderOwnedStaticLights(Asset* asset, const SDL_Rect& bounds, Uint8 alpha);
    void renderReceivedStaticLights(Asset* asset, const SDL_Rect& bounds, Uint8 alpha);
    void renderMovingLights(Asset* asset, const SDL_Rect& bounds, Uint8 alpha);
    void renderOrbitalLights(Asset* asset, const SDL_Rect& bounds, Uint8 alpha);

    int current_shading_group_ = 1;
    int num_groups_ = 10;
    void update_shading_groups();

    bool shouldRegen(Asset* asset);
    SDL_Texture* regenerateFinalTexture(Asset* asset);

    void renderMainLight(Asset* asset,
                         SDL_Texture* tex,
                         const SDL_Rect& main_rect,
                         const SDL_Rect& bounds,
                         Uint8 alpha);

    std::string map_path_;
    SDL_Renderer* renderer_;
    Assets* assets_;
    RenderUtils& util_;
    int screen_width_;
    int screen_height_;

    Generate_Map_Light main_light_source_;
};

#endif // SCENE_RENDERER_HPP
