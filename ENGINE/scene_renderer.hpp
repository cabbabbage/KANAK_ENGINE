#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "Assets.hpp"
#include "render_utils.hpp"
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
                  float dusk_threshold = 100.0f);

    void render();

private:
    void render_test_areas(const AssetInfo* info, int world_x, int world_y);
    std::vector<std::string> test_areas = {"interaction"};

    SDL_Renderer* renderer_;
    Assets* assets_;
    RenderUtils& util_;
    int screen_width_;
    int screen_height_;
    float dusk_thresh_;
};

#endif // SCENE_RENDERER_HPP
