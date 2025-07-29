// light_map.cpp
#include "light_map.hpp"

LightMap::LightMap(SDL_Renderer* renderer, int w, int h)
    : renderer_(renderer), width_(w), height_(h), texture_(nullptr), base_color_{0, 0, 0, 200} {
    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
}

LightMap::~LightMap() {
    if (texture_) SDL_DestroyTexture(texture_);
}

void LightMap::setBaseColor(SDL_Color color) {
    base_color_ = color;
}

void LightMap::setLights(const std::vector<LightInstance>& lights) {
    lights_ = lights;
}

void LightMap::update() {
    SDL_SetRenderTarget(renderer_, texture_);
    SDL_SetRenderDrawColor(renderer_, base_color_.r, base_color_.g, base_color_.b, base_color_.a);
    SDL_RenderClear(renderer_);

    for (const auto& light : lights_) {
        if (!light.texture) continue;
            SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);

        SDL_SetTextureAlphaMod(light.texture, light.alpha);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &light.dst_rect);
    }

    SDL_SetRenderTarget(renderer_, nullptr);
}

SDL_Texture* LightMap::getTexture() const {
    return texture_;
}
