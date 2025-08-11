// === File: engine.hpp ===
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <SDL.h>
#include "area.hpp"
#include "asset.hpp"
#include "active_assets_manager.hpp"
#include "render_utils.hpp"
#include "scene_renderer.hpp"
#include "asset_loader.hpp"

class Assets;
class RenderUtils;
class SceneRenderer;

class Engine {
public:
    Engine(const std::string& map_path,
           SDL_Renderer* renderer,
           int screen_w,
           int screen_h);
    ~Engine();

    void init();
    void game_loop();

private:
    std::string                              map_path;
    SDL_Renderer*                            renderer;
    const int                                SCREEN_WIDTH;
    const int                                SCREEN_HEIGHT;
    SDL_Color                                boundary_color;
    SDL_Texture*                             overlay_texture;
    SDL_Texture*                             minimap_texture_;
    std::unique_ptr<AssetLoader>             loader_;
    Assets*                                  game_assets;
    RenderUtils                              util;
    SceneRenderer*                           scene;
    std::vector<Area>                        roomTrailAreas;
    std::vector<std::pair<SDL_Texture*,Area>> static_faded_areas;
};
