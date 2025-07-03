#include "gradient.hpp"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL2_rotozoom.h>

Gradient::Gradient(SDL_Renderer* renderer,
                   const std::vector<SDL_Texture*>& frames,
                   const std::vector<SDL_Color>& colors,
                   int direction,
                   float opacity,
                   float midpointPercent)
    : renderer_(renderer),
      frames_(frames),
      colors_(colors),
      direction_(direction),
      last_direction_(-1),
      opacity_(opacity),
      midpointPercent_(midpointPercent) {
    if (!renderer_) throw std::runtime_error("Renderer is null");

    size_t n = frames_.size();
    cache_.assign(n, nullptr);
    cacheVersion_.assign(n, -1);
    last_images_.assign(n, nullptr);

    for (SDL_Texture* tex : frames_) {
        if (!tex) {
            maskTargets_.push_back(nullptr);
            masks_.push_back(nullptr);
            continue;
        }

        int w, h;
        if (SDL_QueryTexture(tex, nullptr, nullptr, &w, &h) != 0)
            throw std::runtime_error("SDL_QueryTexture failed: " + std::string(SDL_GetError()));

        SDL_Texture* target = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!target)
            throw std::runtime_error("SDL_CreateTexture TARGET failed");

        SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
        SDL_SetRenderTarget(renderer_, target);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, tex, nullptr, nullptr);

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        if (!surf)
            throw std::runtime_error("Failed to create mask surface");

        if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                                 surf->pixels, surf->pitch) != 0) {
            SDL_FreeSurface(surf);
            throw std::runtime_error("SDL_RenderReadPixels failed: " + std::string(SDL_GetError()));
        }

        SDL_SetRenderTarget(renderer_, prev);

        maskTargets_.push_back(target);
        masks_.push_back(surf);
    }

    raw_gradient_surface_ = buildGradientSurface(colors_, opacity_, midpointPercent_, 0, nullptr);
}

Gradient::~Gradient() {
    for (auto t : maskTargets_) if (t) SDL_DestroyTexture(t);
    for (auto s : masks_)       if (s) SDL_FreeSurface(s);
    for (auto ct : cache_)      if (ct) SDL_DestroyTexture(ct);
    for (auto li : last_images_) if (li) SDL_DestroyTexture(li);
    if (raw_gradient_surface_) SDL_FreeSurface(raw_gradient_surface_);
}

void Gradient::setDirection(int newDirection) {
    if (direction_ != newDirection) {
        last_direction_ = direction_;
        direction_ = newDirection;

    }
}

SDL_Texture* Gradient::getGradient(size_t index) const {
    if (!active_ || index >= frames_.size()) return nullptr;
    if (!raw_gradient_surface_) return nullptr;

    if (last_direction_ == direction_ && last_images_[index]) {
        return last_images_[index];
    }

    SDL_Surface* rotated = rotozoomSurface(raw_gradient_surface_, -double(direction_ % 360), 1.0, SMOOTHING_ON);
    if (!rotated) return nullptr;

    SDL_Surface* final = SDL_CreateRGBSurfaceWithFormat(0, masks_[index]->w, masks_[index]->h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!final) {
        SDL_FreeSurface(rotated);
        return nullptr;
    }

    SDL_Rect src = {
        (rotated->w - final->w) / 2,
        (rotated->h - final->h) / 2,
        final->w,
        final->h
    };
    SDL_BlitSurface(rotated, &src, final, nullptr);
    SDL_FreeSurface(rotated);

    Uint32* fpx = static_cast<Uint32*>(final->pixels);
    Uint32* mpx = static_cast<Uint32*>(masks_[index]->pixels);
    int total = final->w * final->h;
    for (int i = 0; i < total; ++i) {
        Uint8 mr, mg, mb, ma, r, g, b, a;
        SDL_GetRGBA(mpx[i], masks_[index]->format, &mr, &mg, &mb, &ma);
        SDL_GetRGBA(fpx[i], final->format, &r, &g, &b, &a);
        fpx[i] = SDL_MapRGBA(final->format, r, g, b, Uint8((a * ma) / 255));
    }

    SDL_Texture* result = SDL_CreateTextureFromSurface(renderer_, final);
    SDL_FreeSurface(final);
    if (!result) return nullptr;
    SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);

    if (last_images_[index]) SDL_DestroyTexture(last_images_[index]);
    last_images_[index] = result;
    last_direction_ = direction_;

    return result;
}

SDL_Surface* Gradient::buildGradientSurface(const std::vector<SDL_Color>& colors,
                                            float opacity,
                                            float midpointPercent,
                                            int direction,
                                            SDL_Surface* mask) const {
    int w = mask ? mask->w : 256;
    int h = mask ? mask->h : 256;
    int diag = static_cast<int>(std::ceil(std::sqrt(w * w + h * h)));

    SDL_Surface* temp = SDL_CreateRGBSurfaceWithFormat(0, diag, diag, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(temp, nullptr, SDL_MapRGBA(temp->format, 0, 0, 0, 0));

    double pct = midpointPercent / 100.0;
    int fadeBand = std::max(1, static_cast<int>(std::round(pct * h)));
    double cropOffsetY = (diag - h) / 2.0;
    double fadeStartY = cropOffsetY + (h - fadeBand);
    int segments = std::max(1, static_cast<int>(colors.size()) - 1);

    for (int y = 0; y < diag; ++y) {
        double rel = (y - fadeStartY) / double(fadeBand - 1);
        rel = std::clamp(rel, 0.0, 1.0);
        double pos = rel * segments;
        int idx = std::min(segments - 1, static_cast<int>(pos));
        double frac = pos - idx;

        SDL_Color c1 = colors[idx];
        SDL_Color c2 = colors[idx + 1];
        SDL_Color col {
            static_cast<Uint8>(std::lround(c1.r + (c2.r - c1.r) * frac)),
            static_cast<Uint8>(std::lround(c1.g + (c2.g - c1.g) * frac)),
            static_cast<Uint8>(std::lround(c1.b + (c2.b - c1.b) * frac)),
            static_cast<Uint8>(std::lround((c1.a + (c2.a - c1.a) * frac) * opacity))
        };

        Uint32 pixel = SDL_MapRGBA(temp->format, col.r, col.g, col.b, col.a);
        Uint32* row = static_cast<Uint32*>(temp->pixels) + y * (temp->pitch / 4);
        for (int x = 0; x < diag; ++x) {
            row[x] = pixel;
        }
    }

    return temp;
}

void Gradient::setActive(bool value) {
    active_ = value;
}
