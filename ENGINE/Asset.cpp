// asset.cpp
#include "asset.hpp"
#include <random>
#include <SDL_image.h>
#include <iostream>

Asset::Asset(int z_offset,
             const Area& spawn_area,
             SDL_Renderer* renderer,
             Asset* parent)
    : z_offset(z_offset),
      renderer(renderer),
      parent(parent),
      spawn_area_local(spawn_area)
{
    rebuild_world_areas();  // base_areas will be built in finalize_setup
}

void Asset::finalize_setup() {
    if (!info) return;

    auto it_default = info->animations.find("default");
    if (it_default != info->animations.end() && !it_default->second.frames.empty()) {
        current_animation = "default";
        static_frame = (it_default->second.frames.size() == 1);
        if (it_default->second.randomize && it_default->second.frames.size() > 1) {
            std::mt19937 gen{ std::random_device{}() };
            std::uniform_int_distribution<int> dist(0, int(it_default->second.frames.size()) - 1);
            current_frame_index = dist(gen);
        } else {
            current_frame_index = 0;
        }
    }

    base_areas.clear();

    {
        Area a = spawn_area_local;
        a.set_color(255, 0, 0);
        base_areas.push_back(a);
    }
    if (info->has_collision_area) {
        Area a = info->collision_area;
        a.set_color(0, 255, 0);
        base_areas.push_back(a);
    }
    if (info->has_interaction_area) {
        Area a = info->interaction_area;
        a.set_color(0, 0, 255);
        base_areas.push_back(a);
    }
    if (info->has_attack_area) {
        Area a = info->attack_area;
        a.set_color(255, 255, 0);
        base_areas.push_back(a);
    }
    if (info->has_spacing_area) {
        Area a = info->spacing_area;
        a.set_color(255, 0, 255);
        base_areas.push_back(a);
    }
    if (info->has_passability_area) {
        Area a = info->passability_area;
        a.set_color(0, 255, 255);
        base_areas.push_back(a);
    }
    for (const auto& child_def : info->child_assets) {
        Area a = child_def.area;
        a.set_color(192, 192, 192);
        base_areas.push_back(a);
    }

    set_z_index();
    rebuild_world_areas();
}

void Asset::rebuild_world_areas() {
    areas.clear();
    for (const Area& local : base_areas) {
        Area shifted = local;
        shifted.apply_offset(pos_X, pos_Y);
        areas.push_back(std::move(shifted));
    }
}

void Asset::set_position(int x, int y) {
    pos_X = x;
    pos_Y = y;
    set_z_index();
    rebuild_world_areas();
}

void Asset::set_z_index() {
    if (info && info->type == "Player") {
        z_index = pos_Y + info->z_threshold;
    } else if (parent) {
        z_index = parent->z_index + z_offset;
    } else if (info) {
        z_index = pos_Y + info->z_threshold;
    } else {
        z_index = 0;
    }
}

void Asset::update(const std::unordered_set<SDL_Keycode>& keys, const Area* /*compare_area*/) {
    if (!info) return;

    if (info->type == "Player") {
        int dx = 0, dy = 0;

        if (keys.count(SDLK_w)) {
            dy -= 5;
            if (current_animation != "backward") change_animation("backward");
        } else if (keys.count(SDLK_s)) {
            dy += 5;
            if (current_animation != "forward")  change_animation("forward");
        } else if (keys.count(SDLK_a)) {
            dx -= 5;
            if (current_animation != "left")     change_animation("left");
        } else if (keys.count(SDLK_d)) {
            dx += 5;
            if (current_animation != "right")    change_animation("right");
        } else {
            if (current_animation != "default")  change_animation("default");
        }

        if (dx || dy) {
            set_position(pos_X + dx, pos_Y + dy);
        }
    }

    auto it = info->animations.find(current_animation);
    if (it != info->animations.end() && !it->second.frames.empty() && !static_frame) {
        ++current_frame_index;
        if (current_frame_index >= static_cast<int>(it->second.frames.size())) {
            if (it->second.loop) {
                current_frame_index = 0;
            } else if (!it->second.on_end.empty() && info->animations.count(it->second.on_end)) {
                change_animation(it->second.on_end);
            } else {
                current_frame_index = static_cast<int>(it->second.frames.size()) - 1;
            }
        }
    }

    for (Asset& child : children) {
        child.update(keys);
    }
}

void Asset::randomize_frame() {
    auto it = info->animations.find(current_animation);
    if (it == info->animations.end() || it->second.frames.size() < 2) return;
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, int(it->second.frames.size()) - 1);
    current_frame_index = dist(rng);
}

void Asset::change_animation(const std::string& animation_name) {
    if (!info) return;
    auto it = info->animations.find(animation_name);
    if (it != info->animations.end() && !it->second.frames.empty()) {
        current_animation = animation_name;
        current_frame_index = 0;
        static_frame = (it->second.frames.size() == 1);
    }
}

SDL_Texture* Asset::get_current_frame() const {
    if (!info) return nullptr;
    auto it = info->animations.find(current_animation);
    if (it != info->animations.end() && !it->second.frames.empty()) {
        return it->second.frames[current_frame_index];
    }
    return nullptr;
}



SDL_Texture* Asset::get_image() const {
    return get_current_frame();
}

Area Asset::get_global_collision_area() const {
    if (!info || !info->has_collision_area) return Area();
    Area a = info->collision_area;
    a.apply_offset(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_interaction_area() const {
    if (!info || !info->has_interaction_area) return Area();
    Area a = info->interaction_area;
    a.apply_offset(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_attack_area() const {
    if (!info || !info->has_attack_area) return Area();
    Area a = info->attack_area;
    a.apply_offset(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_spacing_area() const {
    if (!info || !info->has_spacing_area) return Area();
    Area a = info->spacing_area;
    a.apply_offset(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_passability_area() const {
    if (!info || !info->has_passability_area) return Area();
    Area a = info->passability_area;
    a.apply_offset(pos_X, pos_Y);
    return a;
}

void Asset::add_child(const Asset& child) {
    std::cout << "[Asset] Parent: " << (info ? info->name : "Unknown")
              << " → Child: " << (child.info ? child.info->name : "Unknown") << "\n";
    children.push_back(child);
}
