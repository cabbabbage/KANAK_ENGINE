// generate_light.cpp
#include "generate_light.hpp"
#include "asset_info.hpp"    // now gives you AssetInfo and LightSource defs

#include <SDL_image.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

GenerateLight::GenerateLight(SDL_Renderer* renderer)
    : renderer_(renderer) {}

SDL_Texture* GenerateLight::generate(const AssetInfo* asset_info,
                                     const LightSource& light,
                                     std::size_t light_index)
{
    if (!renderer_ || !asset_info)
        return nullptr;

    namespace fs = std::filesystem;
    using json = nlohmann::json;

    // prepare cache paths
    const std::string cache_root = "cache/" + asset_info->name + "/lights";
    const std::string folder     = cache_root + "/" + std::to_string(light_index);
    const std::string meta_file  = folder + "/metadata.json";
    const std::string img_file   = folder + "/light.png";

    // try to load from cache
    if (fs::exists(meta_file) && fs::exists(img_file)) {
        std::ifstream mfs(meta_file);
        if (mfs) {
            json meta; mfs >> meta;
            if (meta.value("radius", -1)    == light.radius
             && meta.value("fall_off", -1)  == light.fall_off
             && meta.value("intensity", -1) == light.intensity
             && meta["color"][0].get<int>() == light.color.r
             && meta["color"][1].get<int>() == light.color.g
             && meta["color"][2].get<int>() == light.color.b)
            {
                if (SDL_Surface* surf = IMG_Load(img_file.c_str())) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
                    SDL_FreeSurface(surf);
                    if (tex) {
                        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                        return tex;
                    }
                }
            }
        }
    }

    // not cached or cache invalid: regenerate
    // clear & recreate cache folder
    fs::remove_all(folder);
    fs::create_directories(folder);

    // create render‐target texture
    const int radius    = light.radius;
    const int falloff   = std::clamp(light.fall_off, 0, 100);
    const SDL_Color color = light.color;
    const int intensity = std::clamp(light.intensity, 0, 255);
    const int size      = radius * 2;

    SDL_Texture* texture = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        size, size
    );
    if (!texture) return nullptr;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, texture);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    // compute white core
    float white_core_ratio  = std::pow(1.0f - falloff / 100.0f, 2.0f);
    float white_core_radius = radius * white_core_ratio;

    // setup ray flicker
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<float> spread_dist(0.2f, 0.6f);
    std::uniform_int_distribution<int> ray_count_dist(4, 7);
    int ray_count = ray_count_dist(rng);

    std::vector<std::pair<float, float>> rays;
    for (int i = 0; i < ray_count; ++i)
        rays.emplace_back(angle_dist(rng), spread_dist(rng));

    const float offsets[4][2] = {
        { -0.25f, -0.25f },
        {  0.25f, -0.25f },
        { -0.25f,  0.25f },
        {  0.25f,  0.25f }
    };

    // draw each pixel
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float accum_r=0, accum_g=0, accum_b=0, accum_a=0;
            for (int s = 0; s < 4; ++s) {
                float dx = x - radius + 0.5f + offsets[s][0];
                float dy = y - radius + 0.5f + offsets[s][1];
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist > radius) continue;

                // angle & ray boost
                float angle = std::atan2(dy, dx);
                float ray_boost = 1.0f;
                for (auto& pr : rays) {
                    float diff = std::fabs(angle - pr.first);
                    diff = std::fmod(diff + 2*M_PI, 2*M_PI);
                    if (diff > M_PI) diff = 2*M_PI - diff;
                    if (diff < pr.second)
                        ray_boost += (1.0f - diff/pr.second)*0.05f;
                }
                ray_boost = std::clamp(ray_boost, 1.0f, 1.1f);

                // alpha falloff
                float alpha_ratio = std::pow(1.0f - (dist/static_cast<float>(radius)), 1.4f);
                alpha_ratio = std::clamp(alpha_ratio*ray_boost, 0.0f, 1.0f);
                Uint8 alpha = static_cast<Uint8>(std::min(255.0f, intensity*alpha_ratio*1.6f));

                // interpolate color
                SDL_Color fc;
                if (dist <= white_core_radius) {
                    fc.r = Uint8((255 + color.r)/2);
                    fc.g = Uint8((255 + color.g)/2);
                    fc.b = Uint8((255 + color.b)/2);
                } else {
                    float t = (dist - white_core_radius)/(radius - white_core_radius);
                    fc.r = Uint8((1-t)*((255+color.r)/2) + t*color.r);
                    fc.g = Uint8((1-t)*((255+color.g)/2) + t*color.g);
                    fc.b = Uint8((1-t)*((255+color.b)/2) + t*color.b);
                }
                fc.a = alpha;

                accum_r += fc.r;
                accum_g += fc.g;
                accum_b += fc.b;
                accum_a += fc.a;
            }

            SDL_SetRenderDrawColor(renderer_,
                Uint8(accum_r/4.0f),
                Uint8(accum_g/4.0f),
                Uint8(accum_b/4.0f),
                Uint8(accum_a/4.0f)
            );
            SDL_RenderDrawPoint(renderer_, x, y);
        }
    }

    // reset target
    SDL_SetRenderTarget(renderer_, nullptr);

    // write metadata.json
    {
        json meta;
        meta["radius"]    = light.radius;
        meta["fall_off"]  = light.fall_off;
        meta["intensity"] = light.intensity;
        meta["color"]     = { light.color.r,
                              light.color.g,
                              light.color.b };
        std::ofstream(folder + "/metadata.json") << meta.dump(4);
    }

    // capture to PNG
    {
        int w, h;
        SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
        SDL_Texture* prev_rt = SDL_GetRenderTarget(renderer_);
        SDL_SetRenderTarget(renderer_, texture);
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
            0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_RenderReadPixels(renderer_,
                             nullptr,
                             SDL_PIXELFORMAT_RGBA32,
                             surf->pixels,
                             surf->pitch);
        IMG_SavePNG(surf, img_file.c_str());
        SDL_FreeSurface(surf);
        SDL_SetRenderTarget(renderer_, prev_rt);
    }

    return texture;
}
