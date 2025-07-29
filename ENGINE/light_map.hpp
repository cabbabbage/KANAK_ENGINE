// light_map.hpp
#pragma once

#include <SDL.h>
#include <vector>

// Represents a single light to apply to the light map
struct LightInstance {
    SDL_Texture* texture;  // Light texture (e.g., radial gradient)
    SDL_Rect     dst_rect; // Screen-space destination rect
    int          alpha;    // Opacity to apply to this light
};

class LightMap {
public:
    LightMap(SDL_Renderer* renderer, int width, int height);
    ~LightMap();

    // Set the background color of the lightmap (usually black with alpha)
    void setBaseColor(SDL_Color color);

    // Set the collection of lights to be rendered onto the lightmap
    void setLights(const std::vector<LightInstance>& lights);

    // Update the internal texture by blending the lights into the base
    void update();

    // Get the final SDL_Texture* to render
    SDL_Texture* getTexture() const;

private:
    SDL_Renderer* renderer_;
    SDL_Texture*  texture_;
    SDL_Color     base_color_;
    std::vector<LightInstance> lights_;
    int width_;
    int height_;
};
