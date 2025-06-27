#pragma once

#include <SDL.h>

// Forward declaration to avoid circular include
class Asset;

class GenerateLight {
public:
    GenerateLight(SDL_Renderer* renderer);
    SDL_Texture* generate(const Asset* asset);

private:
    SDL_Renderer* renderer_;
};
