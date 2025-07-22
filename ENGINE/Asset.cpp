// === File: Asset.cpp ===
#include "asset.hpp"
#include "generate_light.hpp"
#include <random>
#include <algorithm>
#include <SDL_image.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <SDL_image.h>
#include "generate_light.hpp"
#include "asset.hpp"
#include <iostream>

// Constructor: initialize animation state, position, z-index, shading flags
Asset::Asset(std::shared_ptr<AssetInfo> info_,
             int z_offset_,
             const Area& spawn_area,
             int start_pos_X,
             int start_pos_Y,
             Asset* parent_)
    : parent(parent_)
    , info(std::move(info_))
    , current_animation()
    , pos_X(start_pos_X)
    , pos_Y(start_pos_Y)
    , z_index(0)
    , z_offset(z_offset_)
    , player_speed_mult(10)
    , is_lit(info->has_light_source)
    , is_shaded(info->has_shading)
    , gradient_opacity(1.0)
    , has_base_shadow(false)
    , spawn_area_local(spawn_area)
{
    // compute initial z_index
    set_z_index();

    // Try "start" → "default"
    auto it = info->animations.find("start");
    if (it == info->animations.end())
        it = info->animations.find("default");

    // If we found one of those, use it
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
    // **FALLBACK**: if still empty, pick **any** available animation
    else if (!info->animations.empty()) {
        auto fallback = info->animations.begin();
        if (!fallback->second.frames.empty()) {
            current_animation = fallback->first;
            static_frame = (fallback->second.frames.size() == 1);
            current_frame_index = 0;
        }
    }
}

void Asset::finalize_setup(SDL_Renderer* renderer) {
    if (!info || !renderer) return;
    //spawn_children(renderer);

    // ensure we have a valid animation now that all textures are loaded
    bool missing = current_animation.empty()
                || info->animations[current_animation].frames.empty();
    if (missing) {
        auto it = info->animations.find("start");
        if (it == info->animations.end()) it = info->animations.find("default");
        if (it == info->animations.end()) it = info->animations.begin();
        if (it != info->animations.end() && !it->second.frames.empty()) {
            change_animation(it->first);

            if (info->animations[current_animation].randomize &&
                info->animations[current_animation].frames.size() > 1)
            {
                std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> dist(0, int(info->animations[current_animation].frames.size()) - 1);
                current_frame_index = dist(rng);
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
    if (!info || !info->has_collision_area || !info->collision_area) return nullptr;
    Area a = *info->collision_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_interaction_area() const {
    if (!info || !info->has_interaction_area || !info->interaction_area) return nullptr;
    Area a = *info->interaction_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_attack_area() const {
    if (!info || !info->has_attack_area || !info->attack_area) return nullptr;
    Area a = *info->attack_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_spacing_area() const {
    if (!info || !info->has_spacing_area || !info->spacing_area) return nullptr;
    Area a = *info->spacing_area;
    a.align(pos_X, pos_Y);
    return a;
}

Area Asset::get_global_passability_area() const {
    if (!info || !info->has_passability_area || !info->passability_area) return nullptr;
    Area a = *info->passability_area;
    a.align(pos_X, pos_Y);
    return a;
}







void Asset::add_child(Asset child) {
    children.push_back(std::move(child));
}

void Asset::set_z_index() {
    try {
        if (parent) {
            z_index = parent->z_index + z_offset;
        } else if (info) {
            z_index = pos_Y + info->z_threshold;
        }
    } catch (const std::exception& e) {
        std::cerr << "[set_z_index] Exception: " << e.what() << "\n";
    }
}





void Asset::spawn_children(SDL_Renderer* renderer) {
    std::cout << "[spawn_children] Beginning spawn...\n";
    if (!info || !renderer) {
        std::cerr << "[spawn_children] Missing asset info or renderer.\n";
        return;
    }

    std::mt19937 rng(std::random_device{}());

    for (const auto& ca : info->child_assets) {
        std::cout << "[spawn_children] Attempting to spawn child asset: " << ca->asset << "\n";

        if (ca->asset.empty()) {
            std::cerr << "[spawn_children] Empty asset name, skipping.\n";
            continue;
        }

        if (ca->min > ca->max) {
            std::cerr << "[spawn_children] Invalid range for '" << ca->asset << "' (min > max), skipping.\n";
            continue;
        }

        int count = std::uniform_int_distribution<int>(ca->min, ca->max)(rng);
        if (count < 1) {
            std::cout << "[spawn_children] Count < 1, skipping.\n";
            continue;
        }

        std::shared_ptr<AssetInfo> child_info;
        try {
            child_info = std::make_shared<AssetInfo>(ca->asset);
        } catch (const std::exception& e) {
            std::cerr << "[spawn_children] Failed to load child AssetInfo: " << e.what() << "\n";
            continue;
        }

        std::cout << "[spawn_children] Spawning " << count << " instance(s) of '" << ca->asset << "'\n";

        Area aligned_area = *ca->area;
        try {
            int anchor_x = pos_X;
            int anchor_y = pos_Y + static_cast<int>(info->original_canvas_height * info->scale_factor / 2.0f);
            aligned_area.align(anchor_x, anchor_y);
        } catch (const std::exception& e) {
            std::cerr << "[spawn_children] Failed to align spawn area: " << e.what() << "\n";
            continue;
        }

        const auto& points = aligned_area.get_points();
        if (points.empty()) {
            std::cerr << "[spawn_children] No points in aligned area.\n";
            continue;
        }

        std::uniform_int_distribution<size_t> dist_point(0, points.size() - 1);

        for (int i = 0; i < count; ++i) {
            const auto& pt = points[dist_point(rng)];

            Area spacing_test("spacing_test");
            try {
                if (child_info->has_spacing_area) {
                    spacing_test = *child_info->spacing_area;
                    spacing_test.align(pt.first, pt.second);
                } else {
                    spacing_test = Area("dummy", pt.first, pt.second, 1, 1, "Square", 0,
                                         std::numeric_limits<int>::max(),
                                         std::numeric_limits<int>::max());
                }
            } catch (const std::exception& e) {
                std::cerr << "[spawn_children] Error creating spacing test area: " << e.what() << "\n";
                continue;
            }

            bool overlaps = false;
            for (const auto& existing : children) {
                try {
                    Area other_area("other");
                    if (existing.info && existing.info->has_spacing_area) {
                        other_area = existing.get_global_spacing_area();
                    } else {
                        other_area = Area("dummy", existing.pos_X, existing.pos_Y, 1, 1, "Square", 0,
                                          std::numeric_limits<int>::max(),
                                          std::numeric_limits<int>::max());
                    }

                    if (spacing_test.intersects(other_area)) {
                        std::cout << "[spawn_children] Overlap detected with '" << existing.info->name << "'\n";
                        overlaps = true;
                        break;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[spawn_children] Failed spacing check: " << e.what() << "\n";
                }
            }

            if (overlaps) {
                continue;
            }

            try {
                Asset child(child_info, ca->z_offset, aligned_area, pt.first, pt.second, this);
                child_info->loadAnimations(renderer);
                child.finalize_setup(renderer);
                children.push_back(std::move(child));
                std::cout << "[spawn_children] Child '" << ca->asset << "' added.\n";
            } catch (const std::exception& e) {
                std::cerr << "[spawn_children] Exception while spawning child: " << e.what() << "\n";
            }
        }
    }
}
