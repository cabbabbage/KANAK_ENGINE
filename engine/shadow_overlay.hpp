// File: shadow_overlay.hpp

#ifndef SHADOW_OVERLAY_HPP
#define SHADOW_OVERLAY_HPP

#include <SDL.h>

class ShadowOverlay {
public:
    ShadowOverlay(SDL_Renderer* renderer);
    ShadowOverlay(const ShadowOverlay& other);

    SDL_Texture* apply(SDL_Texture* source_texture);

    void set_main_color(SDL_Color color);
    void set_secondary_color(SDL_Color color);
    void set_opacity(Uint8 opacity);
    void set_direction(float degrees);
    void set_intensity(int intensity);
    void set_blend_mode(SDL_BlendMode mode);

private:
    SDL_Renderer* renderer_;
    SDL_Color main_color_;
    SDL_Color secondary_color_;
    Uint8 opacity_;
    float direction_degrees_;
    int intensity_;
    SDL_BlendMode blend_mode_;
};

#endif // SHADOW_OVERLAY_HPP
