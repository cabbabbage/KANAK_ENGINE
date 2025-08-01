// === File: light_map.cpp ===

#include "light_map.hpp"

LightMap::LightMap(SDL_Renderer* renderer, int width, int height)
    : renderer_(renderer),
      texture_(nullptr),
      // start with full black so only lights brighten the map
      base_color_{0, 0, 0, 255},
      width_(width),
      height_(height)
{
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET, width_, height_);
    if (texture_) {
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    }
}

LightMap::~LightMap() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

void LightMap::setBaseColor(SDL_Color color) {
    base_color_ = color;
}

void LightMap::setLights(const std::vector<LightInstance>& lights) {
    lights_ = lights;
}

void LightMap::update() {
    if (!renderer_ || !texture_) return;

    // Render into the lightmap texture
    SDL_SetRenderTarget(renderer_, texture_);

    // 1) Fill with ambient/base color
    SDL_SetRenderDrawColor(renderer_,
                           base_color_.r,
                           base_color_.g,
                           base_color_.b,
                           base_color_.a);
    SDL_RenderClear(renderer_);

    // 2) Composite each light additively
    for (const LightInstance& li : lights_) {
        if (!li.texture) continue;

        SDL_SetTextureBlendMode(li.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(li.texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(li.texture, li.alpha);

        SDL_RenderCopy(renderer_, li.texture, nullptr, &li.dst_rect);
    }

    // Restore rendering to default target
    SDL_SetRenderTarget(renderer_, nullptr);
}

SDL_Texture* LightMap::getTexture() const {
    return texture_;
}
