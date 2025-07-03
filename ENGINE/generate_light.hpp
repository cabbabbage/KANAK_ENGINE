#pragma once

#include <SDL.h>
#include "asset_info.hpp" // includes LightSource
class Asset;

class GenerateLight {
public:
    explicit GenerateLight(SDL_Renderer* renderer);
    SDL_Texture* generate(const Asset* asset, const LightSource& light);

private:
    SDL_Renderer* renderer_;
};
