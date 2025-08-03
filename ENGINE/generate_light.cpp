// === File: generate_light.cpp ===
#include "generate_light.hpp"
#include "asset_info.hpp"
#include "cache_manager.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>
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
    const std::string img_file   = folder + "/light.bmp";

    json meta;
    if (CacheManager::load_metadata(meta_file, meta)) {
        if (meta.value("radius", -1)    == light.radius &&
            meta.value("fall_off", -1)  == light.fall_off &&
            meta.value("intensity", -1) == light.intensity &&
            meta["color"][0].get<int>() == light.color.r &&
            meta["color"][1].get<int>() == light.color.g &&
            meta["color"][2].get<int>() == light.color.b &&
            meta.value("flare", -1)     == light.flare)
        {
            if (SDL_Surface* surf = CacheManager::load_surface(img_file)) {
                SDL_Texture* tex = CacheManager::surface_to_texture(renderer, surf);
                SDL_FreeSurface(surf);
                if (tex) return tex;
            }
        }
    }

    fs::remove_all(folder);
    fs::create_directories(folder);

    int radius    = light.radius;
    int falloff   = std::clamp(light.fall_off, 0, 100);
    SDL_Color color = light.color;
    int intensity = std::clamp(light.intensity, 0, 255);
    int flare     = std::clamp(light.flare, 0, 100);

    const int max_dim = 4096;
    float scale = 1.0f;

    while (radius * 2 > max_dim) {
        scale *= 0.5f;
        radius = static_cast<int>(light.radius * scale);
    }

    const int size = radius * 2;

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, size, size);
    if (!texture) return nullptr;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    float smoothness = std::clamp(falloff / 100.0f, 0.01f, 1.0f);  // 1.0 = softest fade
    float gamma = 2.5f * (1.0f - smoothness) + 1.0f; // lower gamma = softer falloff
    float white_core_ratio  = std::pow(smoothness, 2.5f);
    float white_core_radius = radius * white_core_ratio;

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<float> spread_dist(
        0.1f + flare / 300.0f, 0.4f + flare / 200.0f);

    int ray_count = 1 + int(flare * 0.1f);
    std::vector<std::pair<float, float>> rays;
    for (int i = 0; i < ray_count; ++i)
        rays.emplace_back(angle_dist(rng), spread_dist(rng));

    const float offsets[4][2] = {
        { -0.25f, -0.25f },
        {  0.25f, -0.25f },
        { -0.25f,  0.25f },
        {  0.25f,  0.25f }
    };

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float accum_r = 0, accum_g = 0, accum_b = 0, accum_a = 0;
            for (int s = 0; s < 4; ++s) {
                float dx = x - radius + 0.5f + offsets[s][0];
                float dy = y - radius + 0.5f + offsets[s][1];
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > radius) continue;

                float angle = std::atan2(dy, dx);
                float ray_boost = 1.0f;
                for (const auto& pr : rays) {
                    float diff = std::fabs(angle - pr.first);
                    diff = std::fmod(diff + 2 * M_PI, 2 * M_PI);
                    if (diff > M_PI) diff = 2 * M_PI - diff;
                    if (diff < pr.second)
                        ray_boost += (1.0f - diff / pr.second) * (0.04f + flare / 3000.0f);
                }
                ray_boost = std::clamp(ray_boost, 1.0f, 1.1f + flare / 500.0f);

                float fade_ratio = std::clamp(1.0f - dist / float(radius), 0.0f, 1.0f);
                float alpha_ratio = std::pow(fade_ratio, gamma);
                alpha_ratio = std::clamp(alpha_ratio * ray_boost, 0.0f, 1.0f);
                Uint8 alpha = static_cast<Uint8>(std::min(255.0f, intensity * alpha_ratio * 1.6f));

                SDL_Color fc;
                if (dist <= white_core_radius) {
                    fc.r = Uint8((255 + color.r) / 2);
                    fc.g = Uint8((255 + color.g) / 2);
                    fc.b = Uint8((255 + color.b) / 2);
                } else {
                    float t = (dist - white_core_radius) / (radius - white_core_radius);
                    fc.r = Uint8((1 - t) * ((255 + color.r) / 2) + t * color.r);
                    fc.g = Uint8((1 - t) * ((255 + color.g) / 2) + t * color.g);
                    fc.b = Uint8((1 - t) * ((255 + color.b) / 2) + t * color.b);
                }
                fc.a = alpha;

                accum_r += fc.r;
                accum_g += fc.g;
                accum_b += fc.b;
                accum_a += fc.a;
            }

            SDL_SetRenderDrawColor(renderer,
                Uint8(accum_r / 4.0f),
                Uint8(accum_g / 4.0f),
                Uint8(accum_b / 4.0f),
                Uint8(accum_a / 4.0f));
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);

    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (surf) {
        SDL_SetRenderTarget(renderer, texture);
        SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surf->pixels, surf->pitch);
        SDL_SetRenderTarget(renderer, nullptr);
        CacheManager::save_surface_as_png(surf, img_file);
        SDL_FreeSurface(surf);
    }

    json new_meta;
    new_meta["radius"]    = light.radius;
    new_meta["fall_off"]  = light.fall_off;
    new_meta["intensity"] = light.intensity;
    new_meta["color"]     = { color.r, color.g, color.b };
    new_meta["flare"]     = flare;
    new_meta["scale"]     = scale;
    CacheManager::save_metadata(meta_file, new_meta);

    return texture;
}
