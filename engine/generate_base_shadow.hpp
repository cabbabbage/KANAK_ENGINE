#pragma once

#include <SDL.h>
#include "area.hpp"
#include "assets.hpp"
#include <vector>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

class GenerateBaseShadow {
public:
    GenerateBaseShadow(SDL_Renderer* renderer,
                       const std::vector<Area>& zones,
                       Assets* game_assets);

private:
    void apply_shadow_to_asset(Asset* asset, float overlay_strength);

    SDL_Renderer* renderer_;
    SDL_Color shadow_color_ = {11, 15, 20, 255};
};
