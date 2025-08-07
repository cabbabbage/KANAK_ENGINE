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

    json meta;
    if (CacheManager::load_metadata(meta_file, meta)) {
        if (meta.value("radius",   -1)    == light.radius &&
            meta.value("fall_off", -1)    == light.fall_off &&
            meta.value("intensity",-1)    == light.intensity &&
            meta.value("flare",    -1)    == light.flare &&
            meta["color"][0].get<int>()  == light.color.r &&
            meta["color"][1].get<int>()  == light.color.g &&
            meta["color"][2].get<int>()  == light.color.b)
        {
            if (SDL_Surface* surf = CacheManager::load_surface(img_file)) {
                SDL_Texture* tex = CacheManager::surface_to_texture(renderer, surf);
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                SDL_FreeSurface(surf);
                if (tex) return tex;
            }
        }
    }

    fs::remove_all(folder);
    fs::create_directories(folder);

    int radius    = light.radius;
    int falloff   = std::clamp(light.fall_off, 0, 100);
    SDL_Color base = light.color;
    int intensity = std::clamp(light.intensity, 0, 255);
    int flare     = std::clamp(light.flare, 0, 100);

    int drawRadius = radius;
    int size = drawRadius * 2;
    float effRadius = static_cast<float>(drawRadius);

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    Uint32* pixels = static_cast<Uint32*>(surf->pixels);
    SDL_PixelFormat* fmt = surf->format;

    // === PASS 1: Improved falloff fade ===
    float fFallNorm = falloff / 100.0f;
    float exponent = std::pow(0.0f, fFallNorm); // exponential curve: falloff=0 → 1, falloff=1 → 10


    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - drawRadius + 0.5f;
            float dy = y - drawRadius + 0.5f;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > effRadius) {
                pixels[y*size + x] = 0;
                continue;
            }
            float norm = dist / effRadius;
            float fade = std::pow(1.0f - norm, exponent);
            Uint8 a = static_cast<Uint8>(intensity * std::clamp(fade, 0.0f, 1.0f));
            Uint8 r = static_cast<Uint8>(base.r * (a / 255.0f));
            Uint8 g = static_cast<Uint8>(base.g * (a / 255.0f));
            Uint8 b = static_cast<Uint8>(base.b * (a / 255.0f));
            pixels[y*size + x] = SDL_MapRGBA(fmt, r, g, b, a);
        }
    }

    // === PASS 2: Lens flares (skinnier flares) ===
    if (flare > 0) {
        auto drawCircle = [&](int cx, int cy, int rad, Uint8 alpha) {
            for (int dy = -rad; dy <= rad; ++dy) {
                int yy = cy + dy;
                if (yy < 0 || yy >= size) continue;
                int dx_limit = static_cast<int>(std::sqrt(rad * rad - dy * dy));
                for (int dx = -dx_limit; dx <= dx_limit; ++dx) {
                    int xx = cx + dx;
                    if (xx < 0 || xx >= size) continue;
                    int idx = yy * size + xx;
                    Uint8 pr, pg, pb, pa;
                    SDL_GetRGBA(pixels[idx], fmt, &pr, &pg, &pb, &pa);
                    Uint8 na = static_cast<Uint8>(std::min(255, pa + alpha));
                    Uint8 nr = static_cast<Uint8>(base.r * (na / 255.0f));
                    Uint8 ng = static_cast<Uint8>(base.g * (na / 255.0f));
                    Uint8 nb = static_cast<Uint8>(base.b * (na / 255.0f));
                    pixels[idx] = SDL_MapRGBA(fmt, nr, ng, nb, na);
                }
            }
        };

        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> angDist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> lenDist(radius * 0.8f, radius * 1.6f);
        std::uniform_real_distribution<float> widthDist(radius * 0.006f, radius * 0.022f); // thinner

        int streakCount = std::clamp(flare / 15, 3, 8);
        for (int s = 0; s < streakCount; ++s) {
            float angle  = angDist(rng);
            float length = lenDist(rng);
            float widthF = widthDist(rng);
            int steps    = static_cast<int>(length);
            for (int i = 0; i <= steps; ++i) {
                float t = float(i) / steps;
                int cx = static_cast<int>(drawRadius + std::cos(angle) * t * length);
                int cy = static_cast<int>(drawRadius + std::sin(angle) * t * length);
                int rad = static_cast<int>(widthF * (1.0f - t));
                Uint8 alphaVal = static_cast<Uint8>(flare * (1.0f - t) * 0.5f);
                if (rad > 0) drawCircle(cx, cy, rad, alphaVal);
            }
        }
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    CacheManager::save_surface_as_png(surf, img_file);
    SDL_FreeSurface(surf);

    json new_meta;
    new_meta["radius"]    = light.radius;
    new_meta["fall_off"]  = light.fall_off;
    new_meta["intensity"] = light.intensity;
    new_meta["color"]     = { base.r, base.g, base.b };
    new_meta["flare"]     = flare;
    CacheManager::save_metadata(meta_file, new_meta);

    return tex;
}