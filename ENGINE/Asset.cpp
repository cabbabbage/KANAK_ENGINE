// asset.cpp

#include "asset.hpp"
#include <random>

Asset::Asset(int z_offset, const Area& spawn_area)
    : z_offset(z_offset), spawn_area(spawn_area) {}

void Asset::finalize_setup() {
    if (!info) return;

    if (info->type == "Object" && current_animation == "default") {
        auto it = info->animations.find("default");
        if (it != info->animations.end() && !it->second.frames.empty()) {
            static std::mt19937 rng{ std::random_device{}() };
            std::uniform_int_distribution<int> dist(0, static_cast<int>(it->second.frames.size()) - 1);
            current_frame_index = dist(rng);
        }
    }
}

void Asset::update(bool player_is_moving) {
    if (!info) return;

    auto it = info->animations.find(current_animation);
    if (it == info->animations.end() || it->second.frames.empty()) return;

    if (info->type == "Player" && !player_is_moving) return;

    auto& anim = it->second;
    current_frame_index++;

    if (current_frame_index >= static_cast<int>(anim.frames.size())) {
        if (anim.loop) {
            current_frame_index = 0;
        } else if (!anim.on_end.empty() && info->animations.count(anim.on_end)) {
            current_animation = anim.on_end;
            current_frame_index = 0;
        } else {
            current_frame_index = static_cast<int>(anim.frames.size()) - 1;
        }
    }
}

void Asset::change_animation(const std::string& animation_name) {
    if (!info) return;

    auto it = info->animations.find(animation_name);
    if (it != info->animations.end()) {
        current_animation = animation_name;
        current_frame_index = 0;
    }
}

SDL_Texture* Asset::get_current_frame() const {
    if (!info) return nullptr;

    auto it = info->animations.find(current_animation);
    if (it == info->animations.end() || it->second.frames.empty()) return nullptr;

    return it->second.frames[current_frame_index];
}

SDL_Texture* Asset::get_image() const {
    return get_current_frame();
}

void Asset::set_z_index(int z) {
    z_index = z;
}

void Asset::set_position(int x, int y) {
    pos_X = x;
    pos_Y = y;
}

std::string Asset::get_type() const {
    return info ? info->type : "Unknown";
}

Area Asset::get_global_collision_area() const {
    if (!info || !info->has_collision_area) return Area();
    Area shifted = info->collision_area;
    shifted.apply_offset(pos_X, pos_Y);
    return shifted;
}

Area Asset::get_global_interaction_area() const {
    if (!info || !info->has_interaction_area) return Area();
    Area shifted = info->interaction_area;
    shifted.apply_offset(pos_X, pos_Y);
    return shifted;
}

Area Asset::get_global_attack_area() const {
    if (!info || !info->has_attack_area) return Area();
    Area shifted = info->attack_area;
    shifted.apply_offset(pos_X, pos_Y);
    return shifted;
}

Area Asset::get_global_spacing_area() const {
    if (!info || !info->has_spacing_area) return Area();
    Area shifted = info->spacing_area;
    shifted.apply_offset(pos_X, pos_Y);
    return shifted;
}

Area Asset::get_global_passability_area() const {
    if (!info || !info->has_passability_area) return Area();
    Area shifted = info->passability_area;
    shifted.apply_offset(pos_X, pos_Y);
    return shifted;
}
