// File: shadow_overlay.cpp

#include "shadow_overlay.hpp"
#include <cmath>

ShadowOverlay::ShadowOverlay(SDL_Renderer* renderer)
    : renderer_(renderer),
      main_color_({0, 0, 0, 255}),
      secondary_color_({0, 0, 0, 0}),
      opacity_(255),
      direction_degrees_(90.0f),
      intensity_(255),
      blend_mode_(SDL_BLENDMODE_BLEND) {}

ShadowOverlay::ShadowOverlay(const ShadowOverlay& other)
    : renderer_(other.renderer_),
      main_color_(other.main_color_),
      secondary_color_(other.secondary_color_),
      opacity_(other.opacity_),
      direction_degrees_(other.direction_degrees_),
      intensity_(other.intensity_),
      blend_mode_(other.blend_mode_) {}

SDL_Texture* ShadowOverlay::apply(SDL_Texture* source_texture) {
    if (!source_texture || !renderer_) return nullptr;

    int w, h;
    SDL_QueryTexture(source_texture, nullptr, nullptr, &w, &h);

    SDL_Texture* result = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET, w, h);
    if (!result) return nullptr;

    SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, result);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer_, source_texture, nullptr, nullptr);

    SDL_Texture* mask = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, w, h);
    if (!mask) {
        SDL_SetRenderTarget(renderer_, nullptr);
        return result;
    }

    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, mask);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, source_texture, nullptr, nullptr);

    SDL_SetRenderTarget(renderer_, result);

    float rad = direction_degrees_ * (M_PI / 180.0f);
    float dx = std::cos(rad);
    float dy = std::sin(rad);

    if (std::fabs(dy) >= std::fabs(dx)) {
        for (int row = 0; row < h; ++row) {
            float raw = float(row) / float(h - 1);
            float ratio = (dy >= 0) ? raw : 1.0f - raw;
            float inv = 1.0f - ratio;

            SDL_Color color;
            color.r = Uint8(main_color_.r * inv + secondary_color_.r * ratio);
            color.g = Uint8(main_color_.g * inv + secondary_color_.g * ratio);
            color.b = Uint8(main_color_.b * inv + secondary_color_.b * ratio);
            color.a = Uint8(opacity_ * (intensity_ / 255.0f) * ratio);

            SDL_SetTextureColorMod(mask, color.r, color.g, color.b);
            SDL_SetTextureAlphaMod(mask, color.a);

            SDL_Rect line = {0, row, w, 1};
            SDL_RenderCopy(renderer_, mask, &line, &line);
        }
    } else {
        for (int col = 0; col < w; ++col) {
            float raw = float(col) / float(w - 1);
            float ratio = (dx >= 0) ? raw : 1.0f - raw;
            float inv = 1.0f - ratio;

            SDL_Color color;
            color.r = Uint8(main_color_.r * inv + secondary_color_.r * ratio);
            color.g = Uint8(main_color_.g * inv + secondary_color_.g * ratio);
            color.b = Uint8(main_color_.b * inv + secondary_color_.b * ratio);
            color.a = Uint8(opacity_ * (intensity_ / 255.0f) * ratio);

            SDL_SetTextureColorMod(mask, color.r, color.g, color.b);
            SDL_SetTextureAlphaMod(mask, color.a);

            SDL_Rect line = {col, 0, 1, h};
            SDL_RenderCopy(renderer_, mask, &line, &line);
        }
    }

    SDL_DestroyTexture(mask);
    SDL_SetRenderTarget(renderer_, nullptr);
    return result;
}

void ShadowOverlay::set_main_color(SDL_Color color) {
    main_color_ = color;
}

void ShadowOverlay::set_secondary_color(SDL_Color color) {
    secondary_color_ = color;
}

void ShadowOverlay::set_opacity(Uint8 opacity) {
    opacity_ = opacity;
}

void ShadowOverlay::set_direction(float degrees) {
    direction_degrees_ = degrees;
}

void ShadowOverlay::set_intensity(int intensity) {
    intensity_ = intensity;
}

void ShadowOverlay::set_blend_mode(SDL_BlendMode mode) {
    blend_mode_ = mode;
}
