// === File: generate_light.hpp ===

#pragma once

#include <SDL.h>
#include <string>
#include "light_source.hpp"

class GenerateLight {
public:
    explicit GenerateLight(SDL_Renderer* renderer);

    SDL_Texture* generate(SDL_Renderer* renderer,
                          const std::string& asset_name,
                          const LightSource& light,
                          std::size_t light_index);

private:
    SDL_Renderer* renderer_;
};
