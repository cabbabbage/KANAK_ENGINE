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
            // Queue and immediately apply fallback
            change_animation(it->first);
            current_animation = next_animation;
            info->animations[current_animation].change(current_frame_index, static_frame);
            next_animation.clear();

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
    set_flip();
}

void Asset::set_position(int x, int y) {
    pos_X = x;
    pos_Y = y;
    set_z_index();
}


void Asset::update() {
    if (!info) return;

    auto it = info->animations.find(current_animation);
    if (it == info->animations.end()) return;

    Animation& anim = it->second;

    // Immediate queued-change if not locked
    if (!next_animation.empty() && !anim.lock_until_done) {
        auto nit = info->animations.find(next_animation);
        if (nit != info->animations.end()) {
            current_animation = next_animation;
            Animation& newAnim = nit->second;
            newAnim.change(current_frame_index, static_frame);
        }
        next_animation.clear();
        it = info->animations.find(current_animation);
        anim = it->second;
    }

    // Advance frame or handle auto-transition
    if (!static_frame) {
        bool end_reached = false;
        std::string auto_transition;

        if (next_animation == "reverse") {
            --current_frame_index;
            if (current_frame_index <= 0) {
                end_reached = true;
                current_frame_index = 0;
            }
        } else {
            bool advanced = anim.advance(current_frame_index, auto_transition);
            end_reached = !advanced;
        }

        if (end_reached) {
            if (next_animation == "freeze_on_last") {
                anim.freeze();
                static_frame = true;
                next_animation.clear();
            } else if (next_animation == "terminate") {
                this->~Asset();
                return;
            } else if (next_animation == "reverse") {
                int last = static_cast<int>(anim.frames.size()) - 1;
                if (current_frame_index == 0) {
                    current_frame_index = std::min(last, 1);
                } else {
                    current_frame_index = std::max(0, last - 1);
                }
                // keep next_animation as "reverse" for continuous flip
            } else if (!next_animation.empty() && info->animations.count(next_animation)) {
                if (!anim.lock_until_done) {
                    auto & nxt = info->animations[next_animation];
                    current_animation = next_animation;
                    nxt.change(current_frame_index, static_frame);
                    next_animation.clear();
                }
            } else if (!auto_transition.empty() && info->animations.count(auto_transition)) {
                auto & at = info->animations[auto_transition];
                current_animation = auto_transition;
                at.change(current_frame_index, static_frame);
            }
        }
    }

    for (auto& c : children) {
        c.update();
    }
}

void Asset::change_animation(const std::string& name) {
    if (!info || name.empty()) return;
    if (name == current_animation && name != "reverse") return;
    next_animation = name;
}

SDL_Texture* Asset::get_current_frame() const {
    auto itc = custom_frames.find(current_animation);
    if (itc != custom_frames.end() && !itc->second.empty())
        return itc->second[current_frame_index];

    auto iti = info->animations.find(current_animation);
    if (iti != info->animations.end())
        return iti->second.get_frame(current_frame_index);

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

void Asset::add_child(Asset child) {
    if (info) {
        for (auto& ci : info->children) {
            if (std::filesystem::path(ci.json_path).stem().string() == child.info->name) {
                child.set_z_offset(ci.z_offset);
                break;
            }
        }
    }
    child.parent = this;
    child.set_z_index();
    if (info && child.info) {
        std::cout << "[Asset] " << info->name
                  << " adding child \"" << child.info->name
                  << "\" at (" << child.pos_X << ", " << child.pos_Y
                  << "), z_index=" << child.z_index << "\n";
    }
    children.push_back(std::move(child));
}

void Asset::set_z_index() {
    try {
        if (parent) {
            if (z_offset > 0) z_index = parent->z_index + 1;
            else if (z_offset < 0) z_index = parent->z_index - 1;
            else z_index = pos_Y + info->z_threshold;
        } else if (info) {
            z_index = pos_Y + info->z_threshold;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Asset::set_z_index] Exception: " << e.what() << "\n";
    }
}

void Asset::set_z_offset(int z) {
    z_offset = z;
    set_z_index();
    std::cout << "Z offset set to " << z << " for asset " << info->name << "\n";
}

void Asset::set_flip() {
    if (!info || !info->can_invert) return;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 1);
    flipped = (dist(rng) == 1);
}
