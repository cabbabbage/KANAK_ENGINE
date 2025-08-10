// === File: scene_renderer.hpp ===
#pragma once

#include <SDL.h>
#include <memory>
#include <string>

class Assets;
class RenderUtils;
class Asset;

#include "global_light_source.hpp"
#include "render_asset.hpp"
#include "light_map.hpp"

class SceneRenderer {
public:
    SceneRenderer(SDL_Renderer* renderer,
                  Assets* assets,
                  RenderUtils& util,
                  int screen_width,
                  int screen_height,
                  const std::string& map_path);

    ~SceneRenderer() = default;

    void render();

    bool debugging{false};

private:
    void update_shading_groups();
    bool shouldRegen(Asset* a);

private:
    std::string map_path_;
    SDL_Renderer* renderer_;
    Assets* assets_;
    RenderUtils& util_;
    int screen_width_;
    int screen_height_;

    Global_Light_Source main_light_source_;
    SDL_Texture* fullscreen_light_tex_{nullptr};
    std::unique_ptr<LightMap> z_light_pass_;

    RenderAsset render_asset_;

    int current_shading_group_{-1};
    int num_groups_{10};
};
