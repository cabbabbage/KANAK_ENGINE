// === File: generate_light.cpp ===

#include "generate_light.hpp"
#include "cache_manager.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

GenerateLight::GenerateLight(SDL_Renderer* renderer)
    : renderer_(renderer) {}

SDL_Texture* GenerateLight::generate(SDL_Renderer* renderer,
                                     const std::string& asset_name,
                                     const LightSource& light,
                                     std::size_t light_index)
{
    if (!renderer) return nullptr;

    const std::string cache_root = "cache/" + asset_name + "/lights";
    const std::string folder     = cache_root + "/" + std::to_string(light_index);
    const std::string meta_file  = folder + "/metadata.json";
    const std::string img_file   = folder + "/light.png";

    // How aggressively to blur. If you change this, cache will invalidate.
    const int blur_passes = 15;

    json meta;
    if (CacheManager::load_metadata(meta_file, meta)) {
        bool meta_ok =
            meta.value("radius",   -1) == light.radius &&
            meta.value("fall_off", -1) == light.fall_off &&
            meta.value("intensity",-1) == light.intensity &&
            meta.value("flare",    -1) == light.flare &&
            meta.value("blur_passes", -1) == blur_passes &&
            meta.contains("color") &&
            meta["color"].is_array() && meta["color"].size() == 3 &&
            meta["color"][0].get<int>() == light.color.r &&
            meta["color"][1].get<int>() == light.color.g &&
            meta["color"][2].get<int>() == light.color.b;

        if (meta_ok) {
            if (SDL_Surface* surf = CacheManager::load_surface(img_file)) {
                SDL_Texture* tex = CacheManager::surface_to_texture(renderer, surf);
                SDL_FreeSurface(surf);
                if (tex) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    return tex;
                }
            }
        }
    }

    // Rebuild
    fs::remove_all(folder);
    fs::create_directories(folder);

    const int radius    = light.radius;
    const int falloff   = std::clamp(light.fall_off, 0, 100);
    const SDL_Color base = light.color;
    const int intensity = std::clamp(light.intensity, 0, 255);
    const int flare     = std::clamp(light.flare, 0, 100);

    const int drawRadius = radius;
    const int size = std::max(1, drawRadius * 2);
    const float effRadius = static_cast<float>(drawRadius);

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        std::cerr << "[GenerateLight] Failed to create surface: " << SDL_GetError() << "\n";
        return nullptr;
    }

    if (SDL_LockSurface(surf) != 0) {
        std::cerr << "[GenerateLight] Failed to lock surface: " << SDL_GetError() << "\n";
        SDL_FreeSurface(surf);
        return nullptr;
    }

    Uint32* pixels = static_cast<Uint32*>(surf->pixels);
    SDL_PixelFormat* fmt = surf->format;

    // === PASS 1: radial falloff (logistic curve) ===
    const float steepness = 10.0f + (falloff * 0.2f); // 10..30
    const float midpoint  = 0.75f;                    // where fade starts

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - drawRadius + 0.5f;
            float dy = y - drawRadius + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > effRadius) {
                pixels[y * size + x] = 0;
                continue;
            }
            float norm = dist / effRadius;
            float fade = 1.0f / (1.0f + std::exp((norm - midpoint) * steepness));
            Uint8 a = static_cast<Uint8>(intensity * std::clamp(fade, 0.0f, 1.0f));

            // Pre-multiplied-ish color so additive stack doesn’t blow out
            Uint8 r = static_cast<Uint8>(base.r * (a / 255.0f));
            Uint8 g = static_cast<Uint8>(base.g * (a / 255.0f));
            Uint8 b = static_cast<Uint8>(base.b * (a / 255.0f));

            pixels[y * size + x] = SDL_MapRGBA(fmt, r, g, b, a);
        }
    }

    // === PASS 2: thin lens flares ===
    if (flare > 0) {
        auto drawCircle = [&](int cx, int cy, int rad, Uint8 alpha) {
            if (rad <= 0) return;
            for (int dy = -rad; dy <= rad; ++dy) {
                int yy = cy + dy;
                if (yy < 0 || yy >= size) continue;
                int dx_limit = static_cast<int>(std::sqrt(std::max(0, rad * rad - dy * dy)));
                for (int dx = -dx_limit; dx <= dx_limit; ++dx) {
                    int xx = cx + dx;
                    if (xx < 0 || xx >= size) continue;
                    int idx = yy * size + xx;

                    Uint8 pr, pg, pb, pa;
                    SDL_GetRGBA(pixels[idx], fmt, &pr, &pg, &pb, &pa);
                    Uint8 na = static_cast<Uint8>(std::min(255, int(pa) + int(alpha)));
                    Uint8 nr = static_cast<Uint8>(base.r * (na / 255.0f));
                    Uint8 ng = static_cast<Uint8>(base.g * (na / 255.0f));
                    Uint8 nb = static_cast<Uint8>(base.b * (na / 255.0f));
                    pixels[idx] = SDL_MapRGBA(fmt, nr, ng, nb, na);
                }
            }
        };

        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> angDist(0.0f, 2.0f * float(M_PI));
        std::uniform_real_distribution<float> lenDist(radius * 0.8f, radius * 1.6f);
        std::uniform_real_distribution<float> widthDist(radius * 0.006f, radius * 0.022f);

        int streakCount = std::clamp(flare / 15, 3, 8);
        for (int s = 0; s < streakCount; ++s) {
            float angle  = angDist(rng);
            float length = lenDist(rng);
            float widthF = widthDist(rng);
            int steps    = std::max(1, static_cast<int>(length));
            for (int i = 0; i <= steps; ++i) {
                float t  = float(i) / steps;
                int cx   = static_cast<int>(drawRadius + std::cos(angle) * t * length);
                int cy   = static_cast<int>(drawRadius + std::sin(angle) * t * length);
                int rad  = static_cast<int>(widthF * (1.0f - t));
                Uint8 alphaVal = static_cast<Uint8>(flare * (1.0f - t) * 0.5f);
                drawCircle(cx, cy, rad, alphaVal);
            }
        }
    }

    // === PASS 3: heavy separable box-blur, many passes ===
    auto blur_surface = [&](SDL_Surface* surface, int passes) {
        if (!surface || passes <= 0) return;

        const int w = surface->w;
        const int h = surface->h;
        std::vector<Uint32> temp(w * h);

        auto get_pixel = [&](int x, int y) -> Uint32 {
            x = std::clamp(x, 0, w - 1);
            y = std::clamp(y, 0, h - 1);
            return static_cast<Uint32*>(surface->pixels)[y * w + x];
        };

        for (int pass = 0; pass < passes; ++pass) {
            // Horizontal blur (radius = 3)
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int r = 0, g = 0, b = 0, a = 0, count = 0;
                    for (int k = -3; k <= 3; ++k) {
                        Uint8 pr, pg, pb, pa;
                        SDL_GetRGBA(get_pixel(x + k, y), surface->format, &pr, &pg, &pb, &pa);
                        r += pr; g += pg; b += pb; a += pa; ++count;
                    }
                    temp[y * w + x] = SDL_MapRGBA(surface->format,
                                                  Uint8(r / count),
                                                  Uint8(g / count),
                                                  Uint8(b / count),
                                                  Uint8(a / count));
                }
            }

            // Vertical blur (radius = 3)
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int r = 0, g = 0, b = 0, a = 0, count = 0;
                    for (int k = -3; k <= 3; ++k) {
                        Uint8 pr, pg, pb, pa;
                        SDL_GetRGBA(temp[std::clamp(y + k, 0, h - 1) * w + x],
                                    surface->format, &pr, &pg, &pb, &pa);
                        r += pr; g += pg; b += pb; a += pa; ++count;
                    }
                    static_cast<Uint32*>(surface->pixels)[y * w + x] =
                        SDL_MapRGBA(surface->format,
                                    Uint8(r / count),
                                    Uint8(g / count),
                                    Uint8(b / count),
                                    Uint8(a / count));
                }
            }
        }
    };

    blur_surface(surf, blur_passes);

    SDL_UnlockSurface(surf);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        std::cerr << "[GenerateLight] Failed to create texture: " << SDL_GetError() << "\n";
        SDL_FreeSurface(surf);
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // Cache the blurred image for next run
    CacheManager::save_surface_as_png(surf, img_file);
    SDL_FreeSurface(surf);

    json new_meta;
    new_meta["radius"]      = light.radius;
    new_meta["fall_off"]    = light.fall_off;
    new_meta["intensity"]   = light.intensity;
    new_meta["flare"]       = flare;
    new_meta["blur_passes"] = blur_passes;
    new_meta["color"]       = { base.r, base.g, base.b };
    CacheManager::save_metadata(meta_file, new_meta);

    return tex;
}
