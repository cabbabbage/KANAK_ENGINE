// === File: light_source.hpp ===

#pragma once

#include <SDL.h>
#include <vector>

struct LightSource {
    int intensity = 255;
    int radius = 64;
    int fall_off = 50;
    int flare = 0;
    bool flicker = false;
    int offset_x = 0;
    int offset_y = 0;
    int orbit_radius = 0;
    SDL_Color color = {255, 255, 255, 255};
    SDL_Texture* texture = nullptr;  // Generated light texture
};
