#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "Assets.hpp"
#include "render_utils.hpp"
#include "light_map.hpp"
#include "debug_area.hpp"
#include "generate_map_light.hpp"
#include <SDL.h>
#include <vector>
#include <string>

class SceneRenderer {
public:
    SceneRenderer(SDL_Renderer* renderer,
                  Assets* assets,
                  RenderUtils& util,
                  int screen_width,
                  int screen_height,
                  std::string map_path_
                  );

    void render();
    void setTestAreas(const std::vector<std::string>& keys);

private:
    std::string map_path_; 
    SDL_Renderer* renderer_;
    Assets* assets_;
    RenderUtils& util_;
    int screen_width_;
    int screen_height_;

    LightMap lightmap_;
    Generate_Map_Light main_light_source_;
    AreaDebugRenderer debug_renderer_;
};

#endif // SCENE_RENDERER_HPP
