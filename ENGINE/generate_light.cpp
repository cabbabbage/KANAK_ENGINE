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
        if (meta.value("radius", -1) == light.radius &&
            meta.value("fall_off", -1) == light.fall_off &&
            meta.value("intensity", -1) == light.intensity &&
            meta.value("flare", -1) == light.flare &&
            meta["color"][0].get<int>() == light.color.r &&
            meta["color"][1].get<int>() == light.color.g &&
            meta["color"][2].get<int>() == light.color.b)
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

    const int max_dim = 4096;
    float scale = 1.0f;
    while (radius * 2 > max_dim) {
        scale *= 0.5f;
        radius = static_cast<int>(light.radius * scale);
    }

    int drawRadius = radius;
    if (falloff == 0) {
        drawRadius = radius * 3;
    }
    float effRadius = static_cast<float>(drawRadius);
    int size = drawRadius * 2;

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    Uint32* pixels = static_cast<Uint32*>(surf->pixels);
    SDL_PixelFormat* fmt = surf->format;

    // PASS 1: Base colored glow
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
            float exponent = (falloff > 0)
                                ? ((falloff / 25.0f) + 1.5f)
                                : 1.0f;
            float fade = std::pow(1.0f - norm, exponent);
            Uint8 a = Uint8(intensity * fade);
            Uint8 r = Uint8(base.r * (a / 255.0f));
            Uint8 g = Uint8(base.g * (a / 255.0f));
            Uint8 b = Uint8(base.b * (a / 255.0f));
            pixels[y*size + x] = SDL_MapRGBA(fmt, r, g, b, a);
        }
    }

    // PASS 2: Realistic lens flares and halo (only if flare > 0)
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
                    Uint8 nr = Uint8(base.r * (na / 255.0f));
                    Uint8 ng = Uint8(base.g * (na / 255.0f));
                    Uint8 nb = Uint8(base.b * (na / 255.0f));
                    pixels[idx] = SDL_MapRGBA(fmt, nr, ng, nb, na);
                }
            }
        };

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> angDist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> lenDist(radius * 0.5f, radius * 1.2f);
        std::uniform_real_distribution<float> widthDist(radius * 0.002f, radius * 0.01f);

        int streakCount = std::clamp(flare / 20, 2, 6);
        for (int s = 0; s < streakCount; ++s) {
            float angle = angDist(rng);
            float length = lenDist(rng);
            float widthF = widthDist(rng);
            int steps = static_cast<int>(length);
            for (int i = 0; i <= steps; ++i) {
                float t = float(i) / steps;
                int cx = static_cast<int>(drawRadius + std::cos(angle) * t * length);
                int cy = static_cast<int>(drawRadius + std::sin(angle) * t * length);
                int rad = static_cast<int>(widthF * (1.0f - t));
                Uint8 alpha = static_cast<Uint8>(flare * (1.0f - t) * 0.3f);
                if (rad > 0) drawCircle(cx, cy, rad, alpha);
            }
        }

        int ghostCount = std::clamp(flare / 25, 2, 5);
        for (int g = 0; g < ghostCount; ++g) {
            float angle = angDist(rng);
            float distF = std::uniform_real_distribution<float>(0.3f, 0.9f)(rng);
            int gx = static_cast<int>(drawRadius + std::cos(angle) * drawRadius * distF);
            int gy = static_cast<int>(drawRadius + std::sin(angle) * drawRadius * distF);
            int gr = static_cast<int>(drawRadius * 0.05f);
            Uint8 alpha = static_cast<Uint8>(flare * 0.1f * (1.0f - distF));
            drawCircle(gx, gy, gr, alpha);
        }

        float ringR = effRadius * 0.9f;
        float ringW = effRadius * 0.1f;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float dx = x - drawRadius + 0.5f;
                float dy = y - drawRadius + 0.5f;
                float dist = std::sqrt(dx*dx + dy*dy);
                float diff = std::abs(dist - ringR);
                if (diff > ringW) continue;
                float fade = std::exp(-std::pow(diff / ringW, 2) * 2.0f);
                Uint8 add = Uint8(std::clamp(flare * 0.5f * fade, 0.0f, 255.0f));
                int idx = y * size + x;
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(pixels[idx], fmt, &pr, &pg, &pb, &pa);
                int nr = std::min<int>(255, pr + (base.r * add / 255));
                int ng = std::min<int>(255, pg + (base.g * add / 255));
                int nb = std::min<int>(255, pb + (base.b * add / 255));
                int na = std::min<int>(255, pa + add);
                pixels[idx] = SDL_MapRGBA(fmt, Uint8(nr), Uint8(ng), Uint8(nb), Uint8(na));
            }
        }
    }

    // PASS 3: Fast separable box blur
    {
        int blurRadius = std::clamp(drawRadius / 50, 2, 15);
        int winSize = blurRadius * 2 + 1;
        std::vector<Uint32> copyBuf(pixels, pixels + size * size);
        std::vector<Uint32> tempBuf(size * size);

        for (int y = 0; y < size; ++y) {
            int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            for (int dx = -blurRadius; dx <= blurRadius; ++dx) {
                int cx = std::clamp(dx, 0, size-1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(copyBuf[y*size + cx], fmt, &pr, &pg, &pb, &pa);
                sumR += pr; sumG += pg; sumB += pb; sumA += pa;
            }
            for (int x = 0; x < size; ++x) {
                Uint8 nr = Uint8(sumR / winSize);
                Uint8 ng = Uint8(sumG / winSize);
                Uint8 nb = Uint8(sumB / winSize);
                Uint8 na = Uint8(sumA / winSize);
                tempBuf[y*size + x] = SDL_MapRGBA(fmt, nr, ng, nb, na);
                int left = x - blurRadius;
                int right = x + blurRadius + 1;
                int leftCol = std::clamp(left, 0, size-1);
                int rightCol = std::clamp(right, 0, size-1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(copyBuf[y*size + leftCol], fmt, &pr, &pg, &pb, &pa);
                sumR -= pr; sumG -= pg; sumB -= pb; sumA -= pa;
                SDL_GetRGBA(copyBuf[y*size + rightCol], fmt, &pr, &pg, &pb, &pa);
                sumR += pr; sumG += pg; sumB += pb; sumA += pa;
            }
        }

        for (int x = 0; x < size; ++x) {
            int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            for (int dy = -blurRadius; dy <= blurRadius; ++dy) {
                int cy = std::clamp(dy, 0, size-1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(tempBuf[cy*size + x], fmt, &pr, &pg, &pb, &pa);
                sumR += pr; sumG += pg; sumB += pb; sumA += pa;
            }
            for (int y = 0; y < size; ++y) {
                Uint8 nr = Uint8(sumR / winSize);
                Uint8 ng = Uint8(sumG / winSize);
                Uint8 nb = Uint8(sumB / winSize);
                Uint8 na = Uint8(sumA / winSize);
                pixels[y*size + x] = SDL_MapRGBA(fmt, nr, ng, nb, na);
                int top = y - blurRadius;
                int bottom = y + blurRadius + 1;
                int topRow = std::clamp(top, 0, size-1);
                int bottomRow = std::clamp(bottom, 0, size-1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(tempBuf[topRow*size + x], fmt, &pr, &pg, &pb, &pa);
                sumR -= pr; sumG -= pg; sumB -= pb; sumA -= pa;
                SDL_GetRGBA(tempBuf[bottomRow*size + x], fmt, &pr, &pg, &pb, &pa);
                sumR += pr; sumG += pg; sumB += pb; sumA += pa;
            }
        }
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    CacheManager::save_surface_as_png(surf, img_file);
    SDL_FreeSurface(surf);

    json new_meta;
    new_meta["radius"]    = light.radius;
    new_meta["fall_off"]  = light.fall_off;
    new_meta["intensity"] = light.intensity;
    new_meta["color"]     = { base.r, base.g, base.b };
    new_meta["flare"]     = flare;
    new_meta["scale"]     = scale;
    CacheManager::save_metadata(meta_file, new_meta);

    return tex;
}
