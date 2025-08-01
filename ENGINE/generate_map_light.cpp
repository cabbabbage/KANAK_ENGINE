// === File: generate_map_light.cpp ===
#include "generate_map_light.hpp"
#include "generate_light.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

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
    min_opacity_ = 50;
    max_opacity_ = 255;
    radius_ = screen_width * 3;
    intensity_ = 255;
    orbit_radius = 150;
    update_interval_ = 2;
    mult_ = 0.4;
    base_color_ = fallback_base_color;

    const std::string config_file = map_path + "/map_light.json";
    std::ifstream file(config_file);
    if (file) {
        try {
            json j;
            file >> j;
            min_opacity_ = j.value("min_opacity", min_opacity_);
            max_opacity_ = j.value("max_opacity", max_opacity_);
            radius_ = j.value("radius", radius_);
            intensity_ = j.value("intensity", intensity_);
            orbit_radius = j.value("orbit_radius", orbit_radius);
            update_interval_ = j.value("update_interval", update_interval_);
            mult_ = j.value("mult", mult_);

            if (j.contains("base_color") && j["base_color"].is_array() && j["base_color"].size() >= 3) {
                base_color_.r = j["base_color"][0].get<Uint8>();
                base_color_.g = j["base_color"][1].get<Uint8>();
                base_color_.b = j["base_color"][2].get<Uint8>();
                base_color_.a = j["base_color"].size() > 3 ? j["base_color"][3].get<Uint8>() : 255;
            }


            if (j.contains("keys")) {
                for (const auto& entry : j["keys"]) {
                    if (entry.is_array() && entry.size() == 2) {
                        float deg = entry[0];
                        const auto& color = entry[1];
                        if (color.is_array() && color.size() == 4) {
                            SDL_Color c{static_cast<Uint8>(color[0]), static_cast<Uint8>(color[1]),
                                        static_cast<Uint8>(color[2]), static_cast<Uint8>(color[3])};
                            key_colors_.emplace_back(deg, c);
                        }
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

    build_texture();
}

float Generate_Map_Light::get_angle() const {
    return angle_;
}

void Generate_Map_Light::update() {
    if (++frame_counter_ % update_interval_ != 0) return;

    if (!initialized_) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * static_cast<float>(M_PI));
        angle_ = angle_dist(rng);
        initialized_ = true;
    }

    angle_ = std::fmod(angle_ + 2.0f * static_cast<float>(M_PI) - 0.01f, 2.0f * static_cast<float>(M_PI));

    float cos_a = std::cos(angle_);
    float sin_a = std::sin(angle_);
    pos_x_ = center_x_ + static_cast<int>(orbit_radius * cos_a);
    pos_y_ = center_y_ + static_cast<int>(orbit_radius * sin_a);

    float height_ratio = std::clamp(1.0f - ((sin_a + 1.0f) * 0.5f), 0.0f, 1.0f);
    Uint8 alpha = static_cast<Uint8>(min_opacity_ + (max_opacity_ - min_opacity_) * height_ratio);

    SDL_Color new_color = compute_color_from_horizon();
    new_color.a = alpha;
    current_color_ = new_color;

    if (current_color_.a >= light_source_off_at) {
        light_brightness = 0;
    } else if (current_color_.a <= min_opacity_) {
        light_brightness = 255;
    } else {
        float range = static_cast<float>(light_source_off_at - min_opacity_);
        float value = static_cast<float>(light_source_off_at - current_color_.a);
        light_brightness = static_cast<int>((value / range) * 255.0f);
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

    LightSource light;
    light.radius    = radius_;
    light.intensity = intensity_;
    light.fall_off  = 60;
    light.flare     = 0;
    light.color     = base_color_;

    GenerateLight generator(renderer_);
    texture_ = generator.generate(renderer_, "map", light, 0);

    if (!texture_) {
        std::cerr << "[MapLight] Failed to generate light texture\n";
    } else {
        std::cout << "[MapLight] Generated global light texture\n";
    }
}

float Generate_Map_Light::compute_opacity_from_horizon(float norm) const {
    return std::clamp(norm * norm, 0.0f, 1.0f);
}

SDL_Color Generate_Map_Light::compute_color_from_horizon() const {
    float degrees = std::fmod(angle_ * (180.0f / static_cast<float>(M_PI)), 360.0f);
    if (degrees < 0) degrees += 360.0f;

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
            return {
                lerp(k0.color.r, k1.color.r, t),
                lerp(k0.color.g, k1.color.g, t),
                lerp(k0.color.b, k1.color.b, t),
                lerp(k0.color.a, k1.color.a, t)
            };
        }
    }

    const auto& k_last = key_colors_.back();
    const auto& k_first = key_colors_.front();
    float range = 360.0f - k_last.degree + k_first.degree;
    float t = (degrees < k_first.degree)
            ? (degrees + 360.0f - k_last.degree) / range
            : (degrees - k_last.degree) / range;

    return {
        lerp(k_last.color.r, k_first.color.r, t),
        lerp(k_last.color.g, k_first.color.g, t),
        lerp(k_last.color.b, k_first.color.b, t),
        lerp(k_last.color.a, k_first.color.a, t)
    };
}

int Generate_Map_Light::get_update_interval() {
    return update_interval_;
}

int Generate_Map_Light::get_update_index() {
    return frame_counter_ % update_interval_;
}
