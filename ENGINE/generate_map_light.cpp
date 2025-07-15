// generate_map_light.cpp

#include "generate_map_light.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>
#include <filesystem>
#include <SDL_image.h>

namespace fs = std::filesystem;

Generate_Map_Light::Generate_Map_Light(SDL_Renderer* renderer,
                                       int screen_center_x,
                                       int screen_center_y,
                                       int screen_width,
                                       SDL_Color base_color,
                                       Uint8 min_opacity,
                                       Uint8 max_opacity)
    : renderer_(renderer),
      texture_(nullptr),
      base_color_(base_color),
      current_color_(base_color),
      min_opacity_(min_opacity),
      max_opacity_(max_opacity),
      radius_(screen_width * 3),
      center_x_(screen_center_x),
      center_y_(screen_center_y + 200),
      angle_(0.0f),
      initialized_(false),
      pos_x_(0),
      pos_y_(0) {
    
    std::string cache_path = "cache/map_light.bmp";

    if (fs::exists(cache_path)) {
        SDL_Surface* surf = IMG_Load(cache_path.c_str());
        if (surf) {
            texture_ = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (texture_) {
                SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
                std::cout << "[MapLight] Loaded cached light texture.\n";
                return;
            }
        }
        std::cout << "[MapLight] Failed to load cached texture. Rebuilding...\n";
    }

    build_texture();
}

void Generate_Map_Light::update() {
    ++frame_counter_;
    if (frame_counter_ % update_interval_ != 0) return;

    if (!initialized_) {
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> angle_dist(0, 2 * M_PI);
        angle_ = angle_dist(rng);
        initialized_ = true;
    }

    angle_ -= 0.01f;
    if (angle_ < 0) angle_ += 2 * M_PI;

    pos_x_ = center_x_ + static_cast<int>(orbit_radius * std::cos(angle_));
    pos_y_ = center_y_ + static_cast<int>(orbit_radius * std::sin(angle_));

    float norm = std::fabs(std::cos(angle_));
    float fade = compute_opacity_from_horizon(norm);
    Uint8 alpha = static_cast<Uint8>(min_opacity_ + (max_opacity_ - min_opacity_) * fade);

    SDL_Color new_color = compute_color_from_horizon(norm);
    new_color.a = alpha;
    current_color_ = new_color;
}

std::pair<int, int> Generate_Map_Light::get_position() const {
    return { pos_x_, pos_y_ };
}

SDL_Texture* Generate_Map_Light::get_texture() const {
    return texture_;
}

void Generate_Map_Light::build_texture() {
    if (texture_) SDL_DestroyTexture(texture_);
    int size = radius_ * 2;
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    if (!texture_) return;

    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, texture_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - radius_ + 0.5f;
            float dy = y - radius_ + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius_) continue;

            float alpha_ratio = std::pow(1.0f - (dist / static_cast<float>(radius_)), 1.4f);
            alpha_ratio = std::clamp(alpha_ratio, 0.0f, 1.0f);
            Uint8 alpha = static_cast<Uint8>(std::min(255.0f, intensity_ * alpha_ratio));

            SDL_SetRenderDrawColor(renderer_, base_color_.r, base_color_.g, base_color_.b, alpha);
            SDL_RenderDrawPoint(renderer_, x, y);
        }
    }

    SDL_SetRenderTarget(renderer_, nullptr);

    // Save texture to BMP file
    int w, h;
    SDL_QueryTexture(texture_, nullptr, nullptr, &w, &h);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (surf) {
        SDL_SetRenderTarget(renderer_, texture_);
        SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA8888, surf->pixels, surf->pitch);
        SDL_SetRenderTarget(renderer_, nullptr);

        fs::create_directories("cache");
        if (SDL_SaveBMP(surf, "cache/map_light.bmp") == 0) {
            std::cout << "[MapLight] Cached light texture to disk.\n";
        } else {
            std::cerr << "[MapLight] Failed to save cache: " << SDL_GetError() << "\n";
        }

        SDL_FreeSurface(surf);
    }
}

float Generate_Map_Light::compute_opacity_from_horizon(float norm) const {
    float fade = std::pow(norm, 2.0f);
    return std::clamp(fade, 0.0f, 1.0f);
}

SDL_Color Generate_Map_Light::compute_color_from_horizon(float /*norm*/) const {
    float degrees = angle_ * (180.0f / M_PI);
    if (degrees < 0) degrees += 360.0f;
    if (degrees > 360.0f) degrees -= 360.0f;

    struct KeyColor {
        float degree;
        SDL_Color color;
    };

    double mult = 0.4;

    std::vector<KeyColor> keys = {
        {  0.0f,   { 255, 255, 255, static_cast<Uint8>(255) }},
        { 85.0f,   { 255, 255, 255, static_cast<Uint8>(200) }},
        { 95.0f,   { 120, 80,  50,  static_cast<Uint8>(60 * mult) }},
        { 105.0f,  { 90,  55,  90,  static_cast<Uint8>(50 * mult) }},
        { 120.0f,  { 60,  70, 150,  static_cast<Uint8>(20 * mult) }},
        { 150.0f,  { 0,   0,   0,   static_cast<Uint8>(0   * mult) }},
        { 210.0f,  { 0,   0,   0,   static_cast<Uint8>(0   * mult) }},
        { 240.0f,  { 60,  70, 150,  static_cast<Uint8>(20 * mult) }},
        { 255.0f,  { 90,  55,  90,  static_cast<Uint8>(50 * mult) }},
        { 265.0f,  { 120, 80,  50,  static_cast<Uint8>(60 * mult) }},
        { 275.0f,  { 255, 255, 255, static_cast<Uint8>(200) }},
        { 360.0f,  { 255, 255, 255, static_cast<Uint8>(255) }}
    };

    auto lerp = [](Uint8 a, Uint8 b, float t) -> Uint8 {
        return static_cast<Uint8>(a + t * (b - a));
    };

    for (size_t i = 0; i < keys.size() - 1; ++i) {
        const auto& k0 = keys[i];
        const auto& k1 = keys[i + 1];

        if (degrees >= k0.degree && degrees <= k1.degree) {
            float t = (degrees - k0.degree) / (k1.degree - k0.degree);
            SDL_Color result;
            result.r = lerp(k0.color.r, k1.color.r, t);
            result.g = lerp(k0.color.g, k1.color.g, t);
            result.b = lerp(k0.color.b, k1.color.b, t);
            result.a = lerp(k0.color.a, k1.color.a, t);
            return result;
        }
    }

    return keys.back().color;
}
