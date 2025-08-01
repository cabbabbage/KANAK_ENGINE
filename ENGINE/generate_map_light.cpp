// === File: generate_map_light.cpp ===
#include "generate_map_light.hpp"
#include "cache_manager.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <SDL_image.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

Generate_Map_Light::Generate_Map_Light(SDL_Renderer* renderer,
                                       int screen_center_x,
                                       int screen_center_y,
                                       int screen_width,
                                       SDL_Color fallback_base_color,
                                       const std::string& map_path)
    : renderer_(renderer),
      texture_(nullptr),
      current_color_(fallback_base_color),
      center_x_(screen_center_x),
      center_y_(screen_center_y + 200),
      angle_(0.0f),
      initialized_(false),
      pos_x_(0),
      pos_y_(0)
{
    // Defaults
    min_opacity_ = 50;
    max_opacity_ = 255;
    radius_ = screen_width * 3;
    intensity_ = 255;
    orbit_radius = 150;
    update_interval_ = 2;
    mult_ = 0.4;
    base_color_ = fallback_base_color;

    std::string config_file = map_path + "/map_light.json";
    std::ifstream file(config_file);
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            if (j.contains("min_opacity")) min_opacity_ = j["min_opacity"];
            if (j.contains("max_opacity")) max_opacity_ = j["max_opacity"];
            if (j.contains("radius")) radius_ = j["radius"];
            if (j.contains("intensity")) intensity_ = j["intensity"];
            if (j.contains("orbit_radius")) orbit_radius = j["orbit_radius"];
            if (j.contains("update_interval")) update_interval_ = j["update_interval"];
            if (j.contains("mult")) mult_ = j["mult"];
            if (j.contains("base_color") && j["base_color"].is_array() && j["base_color"].size() >= 3) {
                base_color_.r = j["base_color"][0];
                base_color_.g = j["base_color"][1];
                base_color_.b = j["base_color"][2];
                base_color_.a = (j["base_color"].size() > 3) ? j["base_color"][3] : 255;
            }
            if (j.contains("keys") && j["keys"].is_array()) {
                for (const auto& entry : j["keys"]) {
                    if (!entry.is_array() || entry.size() != 2) continue;
                    float deg = entry[0];
                    auto color = entry[1];
                    if (color.is_array() && color.size() == 4) {
                        SDL_Color c;
                        c.r = color[0];
                        c.g = color[1];
                        c.b = color[2];
                        c.a = color[3];
                        key_colors_.push_back({ deg, c });
                    }
                }
            }
        } catch (...) {
            std::cerr << "[MapLight] Error parsing map_light.json; using defaults.\n";
        }
    }

    if (key_colors_.empty()) {
        key_colors_ = {
            {  0.0f,   {255, 255, 255, 255}},
            { 85.0f,   {255, 255, 255, 200}},
            { 95.0f,   {120, 80,  50,  static_cast<Uint8>(60 * mult_)}},
            {105.0f,   {90,  55,  90,  static_cast<Uint8>(50 * mult_)}},
            {120.0f,   {60,  70, 150,  static_cast<Uint8>(20 * mult_)}},
            {150.0f,   {0,   0,   0,   static_cast<Uint8>(0   * mult_)}},
            {210.0f,   {0,   0,   0,   static_cast<Uint8>(0   * mult_)}},
            {240.0f,   {60,  70, 150,  static_cast<Uint8>(20 * mult_)}},
            {255.0f,   {90,  55,  90,  static_cast<Uint8>(50 * mult_)}},
            {265.0f,   {120, 80,  50,  static_cast<Uint8>(60 * mult_)}},
            {275.0f,   {255, 255, 255, 200}},
            {360.0f,   {255, 255, 255, 255}}
        };
    }

    std::string cache_path = "cache/map_light.bmp";
    CacheManager cache;

    if (fs::exists(cache_path)) {
        SDL_Surface* surf = cache.load_surface(cache_path);
        if (surf) {
            texture_ = cache.surface_to_texture(renderer_, surf);
            SDL_FreeSurface(surf);
            if (texture_) {
                std::cout << "[MapLight] Loaded cached light texture.\n";
                return;
            }
        }
        std::cout << "[MapLight] Failed to load cached texture. Rebuilding...\n";
    }

    build_texture();
}

float Generate_Map_Light::get_angle() const {
    return angle_;
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

    float height_ratio = 1.0f - ((std::sin(angle_) + 1.0f) * 0.5f);
    height_ratio = std::clamp(height_ratio, 0.0f, 1.0f);

    Uint8 alpha = static_cast<Uint8>(
        min_opacity_ + (max_opacity_ - min_opacity_) * height_ratio
    );

    SDL_Color new_color = compute_color_from_horizon();
    new_color.a = alpha;
    current_color_ = new_color;

    // Inverse interpolation for light_brightness
    if (current_color_.a >= light_source_off_at) {
        light_brightness = 0;
    } else if (current_color_.a <= min_opacity_) {
        light_brightness = 255;
    } else {
        float range = float(light_source_off_at - min_opacity_);
        float value = float(light_source_off_at - current_color_.a);
        float ratio = value / range;
        light_brightness = static_cast<int>(ratio * 255.0f);
    }
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

    int w, h;
    SDL_QueryTexture(texture_, nullptr, nullptr, &w, &h);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (surf) {
        SDL_SetRenderTarget(renderer_, texture_);
        SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA8888, surf->pixels, surf->pitch);
        SDL_SetRenderTarget(renderer_, nullptr);

        fs::create_directories("cache");
        CacheManager cache;
        if (cache.save_surface_as_png(surf, "cache/map_light.bmp")) {
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

SDL_Color Generate_Map_Light::compute_color_from_horizon() const {
    float degrees = angle_ * (180.0f / M_PI);
    if (degrees < 0) degrees += 360.0f;
    if (degrees > 360.0f) degrees -= 360.0f;

    auto lerp = [](Uint8 a, Uint8 b, float t) -> Uint8 {
        return static_cast<Uint8>(a + t * (b - a));
    };

    if (key_colors_.size() < 2)
        return key_colors_.empty() ? base_color_ : key_colors_.front().color;

    for (size_t i = 0; i + 1 < key_colors_.size(); ++i) {
        const auto& k0 = key_colors_[i];
        const auto& k1 = key_colors_[i + 1];
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

    const auto& k_last = key_colors_.back();
    const auto& k_first = key_colors_.front();
    float range = 360.0f - k_last.degree + k_first.degree;
    float t = (degrees < k_first.degree)
            ? (degrees + 360.0f - k_last.degree) / range
            : (degrees - k_last.degree) / range;

    SDL_Color result;
    result.r = lerp(k_last.color.r, k_first.color.r, t);
    result.g = lerp(k_last.color.g, k_first.color.g, t);
    result.b = lerp(k_last.color.b, k_first.color.b, t);
    result.a = lerp(k_last.color.a, k_first.color.a, t);
    return result;
}

int Generate_Map_Light::get_update_interval(){
    return update_interval_;
}

int Generate_Map_Light::get_update_index() {
    return frame_counter_ % update_interval_;
}
