// === File: scene_renderer.hpp ===
#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "Assets.hpp"
#include "render_utils.hpp"
#include <SDL.h>

class SceneRenderer {
public:
    SceneRenderer(SDL_Renderer* renderer,
                  Assets* assets,
                  RenderUtils& util,
                  int screen_width,
                  int screen_height,
                  float dusk_threshold = 30.0f);

    void render();

private:
    SDL_Renderer* renderer_;
    Assets* assets_;
    RenderUtils& util_;
    int screen_width_;
    int screen_height_;
    float dusk_thresh_;
};

#endif // SCENE_RENDERER_HPP
