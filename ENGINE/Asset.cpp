// === File: Asset.cpp ===
#include "asset.hpp"
#include "generate_light.hpp"
#include <random>
#include <algorithm>
#include <SDL_image.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

// === Asset constructor ===
Asset::Asset(std::shared_ptr<AssetInfo> info_,
             const Area& spawn_area,
             int start_pos_X,
             int start_pos_Y,
             int depth_,
             Asset* parent_)
    : parent(parent_),
      info(std::move(info_)),
      current_animation(),
      pos_X(start_pos_X),
      pos_Y(start_pos_Y),
      z_index(0),
      z_offset(0),
      player_speed_mult(10),
      is_lit(info->has_light_source),
      is_shaded(info->has_shading),
      gradient_opacity(1.0),
      has_base_shadow(false),
      spawn_area_local(spawn_area),
      depth(depth_)
{
    set_z_index();
    // pick initial animation frame
    auto it = info->animations.find("start");
    if (it == info->animations.end())
        it = info->animations.find("default");

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
}

void Asset::finalize_setup(SDL_Renderer* renderer) {
    if (!info || !renderer) return;

    // If no animation chosen yet, pick a fallback
    if (current_animation.empty() ||
        info->animations[current_animation].frames.empty())
    {
        auto it = info->animations.find("start");
        if (it == info->animations.end()) it = info->animations.find("default");
        if (it == info->animations.end())
            it = info->animations.begin();

        if (it != info->animations.end() && !it->second.frames.empty()) {
            change_animation(it->first);
            if (info->animations[current_animation].randomize &&
                info->animations[current_animation].frames.size() > 1)
            {
                std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> dist(
                    0, int(info->animations[current_animation].frames.size()) - 1
                );
                current_frame_index = dist(rng);
            }
        }
    }

    for (auto& child : children) {
        child.finalize_setup(renderer);
    }

    if (!children.empty()) {
        std::cout << "[Asset] \"" << info->name
                  << "\" at (" << pos_X << ", " << pos_Y << ") has "
                  << children.size() << " child(ren):\n";
        for (auto& child : children) {
            if (child.info) {
                std::cout << "    - \"" << child.info->name
                          << "\" at (" << child.pos_X << ", " << child.pos_Y << ")\n";
            }
        }
    }
}

void Asset::set_position(int x, int y) {
    pos_X = x;
    pos_Y = y;
    set_z_index();
}

void Asset::update() {
    if (!info) return;

    auto it = info->animations.find(current_animation);
    if (it != info->animations.end() &&
        !it->second.frames.empty() &&
        !static_frame)
    {
        if (++current_frame_index >= int(it->second.frames.size())) {
            if (it->second.loop) {
                current_frame_index = 0;
            } else if (!it->second.on_end.empty() &&
                       info->animations.count(it->second.on_end))
            {
                change_animation(it->second.on_end);
            } else {
                current_frame_index = int(it->second.frames.size()) - 1;
            }
        }
    }

    // update own logic, then children
    for (auto& c : children) {
        c.update();
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
    auto itc = custom_frames.find(current_animation);
    if (itc != custom_frames.end() && !itc->second.empty())
        return itc->second[current_frame_index];

    auto iti = info->animations.find(current_animation);
    if (iti != info->animations.end() && !iti->second.frames.empty())
        return iti->second.frames[current_frame_index];

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
    if (!info || !info->has_collision_area || !info->collision_area)
        return nullptr;
    Area a = *info->collision_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_interaction_area() const {
    if (!info || !info->has_interaction_area || !info->interaction_area)
        return nullptr;
    Area a = *info->interaction_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_attack_area() const {
    if (!info || !info->has_attack_area || !info->attack_area)
        return nullptr;
    Area a = *info->attack_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_spacing_area() const {
    if (!info || !info->has_spacing_area || !info->spacing_area)
        return nullptr;
    Area a = *info->spacing_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_passability_area() const {
    if (!info || !info->has_passability_area || !info->passability_area)
        return nullptr;
    Area a = *info->passability_area;
    a.align(pos_X, pos_Y);
    return a;
}

void Asset::add_child(Asset child) {
    // look up this child's z_offset from our AssetInfo
    if (info) {
        for (auto& ci : info->children) {
            // match on the stem of the JSON path == the child's asset name
            if (std::filesystem::path(ci.json_path).stem().string() == child.info->name) {
                child.set_z_offset(ci.z_offset);
                break;
            }
        }
    }

    // assign parent and recalc final z_index
    child.parent = this;
    child.set_z_index();

    if (info && child.info) {
        std::cout 
            << "[Asset] " << info->name 
            << " adding child \"" << child.info->name 
            << "\" at (" << child.pos_X << ", " << child.pos_Y 
            << "), z_index=" << child.z_index << "\n";
    }

    children.push_back(std::move(child));
}

void Asset::set_z_index() {
    try {
        if (parent) {
            if (z_offset > 0) {
                z_index = parent->z_index + 1;
            } else if (z_offset < 0) {
                z_index = parent->z_index - 1;
            } else {
                z_index = pos_Y + info->z_threshold;
            }
        } else if (info) {
            z_index = pos_Y + info->z_threshold;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Asset::set_z_index] Exception: " << e.what() << "\n";
    }
}

void Asset::set_z_offset(int z){
    z_offset = z;
    set_z_index();
    std::cout << "Z offset set to " << z  << "for asset " << info->name << " \n";
}
