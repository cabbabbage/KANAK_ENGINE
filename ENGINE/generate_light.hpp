// generate_light.hpp
#pragma once

#include <SDL.h>
#include <cstddef>    // for std::size_t

struct LightSource;
class AssetInfo;

class GenerateLight {
public:
    explicit GenerateLight(SDL_Renderer* renderer);

    // now takes 3 args: pointer to AssetInfo, the LightSource, and the light index
    SDL_Texture* generate(const AssetInfo* asset_info,
                          const LightSource& light,
                          std::size_t light_index);

private:
    SDL_Renderer* renderer_;
};
