// gradient.cpp
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
    : renderer_(renderer)
    , frames_(frames)
    , colors_(colors)
    , direction_(direction)
    , opacity_(opacity)
    , midpointPercent_(midpointPercent)
{
    if (!renderer_) throw std::runtime_error("Renderer is null");

    size_t n = frames_.size();
    cache_.assign(n, nullptr);
    cacheVersion_.assign(n, -1);

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
            renderer_, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET, w, h
        );
        if (!target)
            throw std::runtime_error("SDL_CreateTexture TARGET failed: " + std::string(SDL_GetError()));

        SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
        SDL_SetRenderTarget(renderer_, target);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, tex, nullptr, nullptr);

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        if (!surf)
            throw std::runtime_error("Failed to create mask surface");

        if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32, surf->pixels, surf->pitch) != 0) {
            SDL_FreeSurface(surf);
            throw std::runtime_error("SDL_RenderReadPixels failed: " + std::string(SDL_GetError()));
        }

        SDL_SetRenderTarget(renderer_, prev);

        maskTargets_.push_back(target);
        masks_.push_back(surf);
    }
}

Gradient::~Gradient() {
    for (auto t : maskTargets_) if (t) SDL_DestroyTexture(t);
    for (auto s : masks_)       if (s) SDL_FreeSurface(s);
    for (auto ct : cache_)      if (ct) SDL_DestroyTexture(ct);
}

void Gradient::updateParameters(const std::vector<SDL_Color>& colors,
                                int direction,
                                float opacity,
                                float midpointPercent)
{
    if (!active_) return;
    colors_          = colors;
    direction_       = direction;
    opacity_         = opacity;
    midpointPercent_ = midpointPercent;
    ++version_;
}

SDL_Texture* Gradient::getGradient(size_t index) const {
    if (!active_) return nullptr;
    if (index >= masks_.size()) return nullptr;
    SDL_Surface* maskSurf = masks_[index];
    if (!maskSurf) return nullptr;

    if (cacheVersion_[index] == version_ && cache_[index]) {
        return cache_[index];
    }

    SDL_Surface* gradSurf = buildGradientSurface(colors_, opacity_, midpointPercent_, direction_, maskSurf);
    if (!gradSurf) return nullptr;

    SDL_Texture* result = SDL_CreateTextureFromSurface(renderer_, gradSurf);
    SDL_FreeSurface(gradSurf);
    if (!result) throw std::runtime_error("Failed to create gradient texture");
    SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);

    if (cache_[index]) SDL_DestroyTexture(cache_[index]);
    cache_[index]       = result;
    cacheVersion_[index]= version_;
    return result;
}

SDL_Surface* Gradient::buildGradientSurface(const std::vector<SDL_Color>& colors,
                                            float opacity,
                                            float midpointPercent,
                                            int direction,
                                            SDL_Surface* mask) const
{
    int w = mask->w;
    int h = mask->h;
    int diag = static_cast<int>(std::ceil(std::sqrt(w * w + h * h)));

    SDL_Surface* temp = SDL_CreateRGBSurfaceWithFormat(0, diag, diag, 32, SDL_PIXELFORMAT_RGBA32);
    if (!temp) throw std::runtime_error("Failed to create temp gradient surface");

    SDL_FillRect(temp, nullptr, SDL_MapRGBA(temp->format, 0, 0, 0, 0));

    int segments = std::max(1, int(colors.size()) - 1);
    double midY = midpointPercent / 100.0 * diag;

    for (int y = 0; y < diag; ++y) {
        double rel = (y - midY + diag / 2.0) / diag;
        rel = std::clamp(rel, 0.0, 1.0);
        double pos = rel * segments;
        int idx = std::min(segments - 1, int(pos));
        double frac = pos - idx;

        SDL_Color c1 = colors[idx], c2 = colors[idx + 1];
        SDL_Color col {
            Uint8(c1.r + (c2.r - c1.r) * frac),
            Uint8(c1.g + (c2.g - c1.g) * frac),
            Uint8(c1.b + (c2.b - c1.b) * frac),
            Uint8(std::lround((c1.a + (c2.a - c1.a) * frac) * opacity))
        };


        Uint32 pixel = SDL_MapRGBA(temp->format, col.r, col.g, col.b, col.a);
        Uint32* row = static_cast<Uint32*>(temp->pixels) + y * (temp->pitch / 4);
        for (int x = 0; x < diag; ++x) row[x] = pixel;
    }

    double angle = 0.0;
    switch (direction % 360) {
        case 0:   angle = 0; break;
        case 90:  angle = -90; break;
        case 180: angle = 180; break;
        case 270: angle = 90; break;
        default:  angle = -double(direction % 360); break;
    }

    SDL_Surface* rotated = rotozoomSurface(temp, angle, 1.0, SMOOTHING_ON);
    SDL_FreeSurface(temp);
    if (!rotated) throw std::runtime_error("Failed to rotate gradient surface");

    SDL_Surface* final = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!final) {
        SDL_FreeSurface(rotated);
        throw std::runtime_error("Failed to create final gradient surface");
    }

    SDL_Rect src = {
        (rotated->w - w) / 2,
        (rotated->h - h) / 2,
        w,
        h
    };
    SDL_BlitSurface(rotated, &src, final, nullptr);
    SDL_FreeSurface(rotated);

    Uint32* fpx = static_cast<Uint32*>(final->pixels);
    Uint32* mpx = static_cast<Uint32*>(mask->pixels);
    int total = w * h;
    for (int i = 0; i < total; ++i) {
        Uint8 mr, mg, mb, ma;
        SDL_GetRGBA(mpx[i], mask->format, &mr, &mg, &mb, &ma);
        Uint8 r, g, b, a;
        SDL_GetRGBA(fpx[i], final->format, &r, &g, &b, &a);
        fpx[i] = SDL_MapRGBA(final->format, r, g, b, Uint8((a * ma) / 255));
    }

    return final;
}



void Gradient::setActive(bool value) {
    active_ = value;
}
