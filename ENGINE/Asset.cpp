// === File: Asset.cpp ===
#include "asset.hpp"
#include <random>
#include <iostream>
#include <algorithm>
#include <SDL_image.h>

Asset::Asset(int z_offset,
             const Area& spawn_area,
             SDL_Renderer* renderer,
             Asset* parent)
  : z_offset(z_offset),
    renderer(renderer),
    parent(parent),
    spawn_area_local(spawn_area),
    gradient_opacity(100)
{
    player_speed_mult = 10;
}

void Asset::finalize_setup(int start_pos_X,
                           int start_pos_Y,
                           int /*parent_X*/,
                           int /*parent_Y*/)
{
    if (!info) return;

    // Set animation state
    auto it = info->animations.find("start");
    if (it == info->animations.end()) {
        it = info->animations.find("default");
    }

    if (it != info->animations.end() && !it->second.frames.empty()) {
        current_animation = it->first;
        static_frame = (it->second.frames.size() == 1);
        if (it->second.randomize && it->second.frames.size() > 1) {
            std::mt19937 g{std::random_device{}()};
            std::uniform_int_distribution<int> d(0, int(it->second.frames.size()) - 1);
            current_frame_index = d(g);
        } else {
            current_frame_index = 0;
        }
    }

    // Always position at the given start coordinates
    pos_X = start_pos_X;
    pos_Y = start_pos_Y;

    set_z_index();

    // === Lighting & Shading Setup ===
    is_lit = info->has_light_source;
    is_shaded = info->has_shading;

    if (is_lit && renderer) {
        GenerateLight light_gen(renderer);
        light_texture = light_gen.generate(this);
    }
}


void Asset::set_position(int x, int y) {
    // Ignore any offsets; set absolute position
    pos_X = x;
    pos_Y = y;
    set_z_index();
}

void Asset::update() {
    if (!info) return;
    auto it = info->animations.find(current_animation);
    if (it != info->animations.end() && !it->second.frames.empty() && !static_frame) {
        if (++current_frame_index >= int(it->second.frames.size())) {
            if (it->second.loop) {
                current_frame_index = 0;
            } else if (!it->second.on_end.empty() && info->animations.count(it->second.on_end)) {
                change_animation(it->second.on_end);
            } else {
                current_frame_index = int(it->second.frames.size()) - 1;
            }
        }
    }
    for (auto& c : children) {
        c.update();
    }
}

void Asset::set_z_index() {
    if (info && info->type == "Player") {
        z_index = pos_Y + info->z_threshold;
    } else if (parent) {
        int bz = pos_Y + info->z_threshold;
        z_index = std::clamp(bz + z_offset, parent->z_index - 1000, parent->z_index + 1000);
    } else if (info) {
        z_index = pos_Y + info->z_threshold;
    } else {
        z_index = 0;
    }
}

void Asset::change_animation(const std::string& name) {
    auto it = info->animations.find(name);
    if (it != info->animations.end() && !it->second.frames.empty()) {
        current_animation = name;
        current_frame_index = 0;
        static_frame = (it->second.frames.size() == 1);
    }
}

SDL_Texture* Asset::get_current_frame() const {
    // Check for per-instance custom shadowed frames
    auto it_custom = custom_frames.find(current_animation);
    if (it_custom != custom_frames.end() && !it_custom->second.empty())
        return it_custom->second[current_frame_index];

    // Fallback to shared asset info frames
    auto it_info = info->animations.find(current_animation);
    if (it_info != info->animations.end() && !it_info->second.frames.empty())
        return it_info->second.frames[current_frame_index];

    return nullptr;
}

SDL_Texture* Asset::get_image() const {
    return get_current_frame();
}


std::string Asset::get_current_animation() const {
    return current_animation;
}

std::string Asset::get_type() const {
    return info ? info->type : "";
}

Area Asset::get_global_collision_area() const {
    if (!info || !info->has_collision_area) return {};
    Area a = info->collision_area;
    a.align(pos_X, pos_Y);
    return a;
}
Area Asset::get_global_interaction_area() const {
    if (!info || !info->has_interaction_area) return {};
    Area a = info->interaction_area;
    a.align(pos_X, pos_Y);
    return a;
}
Area Asset::get_global_attack_area() const {
    if (!info || !info->has_attack_area) return {};
    Area a = info->attack_area;
    a.align(pos_X, pos_Y);
    return a;
}
Area Asset::get_global_spacing_area() const {
    if (!info || !info->has_spacing_area) return {};
    Area a = info->spacing_area;
    a.align(pos_X, pos_Y);
    return a;
}
Area Asset::get_global_passability_area() const {
    if (!info || !info->has_passability_area) return {};
    Area a = info->passability_area;
    a.align(pos_X, pos_Y);
    return a;
}

void Asset::add_child(Asset child) {
    children.push_back(child);
}
