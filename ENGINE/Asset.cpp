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
      current_frame_index(0),
      static_frame(false),
      active(false),
      pos_X(start_pos_X),
      pos_Y(start_pos_Y),
      z_index(0),
      z_offset(0),
      player_speed_mult(10),
      is_lit(info->has_light_source),
      is_shaded(info->has_shading),
      alpha_percentage(1.0),
      has_base_shadow(false),
      spawn_area_local(spawn_area),
      depth(depth_)
{
    set_flip();
    set_z_index();
    player_speed_mult = 10;
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
        }
    }
}

void Asset::finalize_setup(SDL_Renderer* renderer) {
    if (!info || !renderer) return;

    if (current_animation.empty() ||
        info->animations[current_animation].frames.empty())
    {
        auto it = info->animations.find("start");
        if (it == info->animations.end())
            it = info->animations.find("default");
        if (it == info->animations.end())
            it = info->animations.begin();

        if (it != info->animations.end() && !it->second.frames.empty()) {
            current_animation = it->first;
            Animation &anim = it->second;
            static_frame = (anim.frames.size() == 1);
            anim.change(current_frame_index, static_frame);
            if (anim.randomize && anim.frames.size() > 1) {
                std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> dist(0, int(anim.frames.size()) - 1);
                current_frame_index = dist(rng);
            }
        }
    }

    for (auto& child : children) {
        child.finalize_setup(renderer);
    }

    if (!children.empty()) {
        std::cout << "[Asset] \"" << info->name
                  << "\" at (" << pos_X << ", " << pos_Y
                  << ") has " << children.size() << " child(ren):\n";
        for (auto& child : children) {
            if (child.info) {
                std::cout << "    - \"" << child.info->name
                          << "\" at (" << child.pos_X << ", "
                          << child.pos_Y << ")\n";
            }
        }
    }
    has_shading = info->has_shading;

}

// === Added missing set_position implementation ===
void Asset::set_position(int x, int y) {
    pos_X = x;
    pos_Y = y;
    set_z_index();
}

void Asset::update() {
    if (!info) return;
    if (dead) return;
    // Apply any queued animation change first
    if (!next_animation.empty()) {
        if (next_animation == "freeze_on_last") {
            auto itf = info->animations.find(current_animation);
            if (itf != info->animations.end()) {
                Animation &currAnim = itf->second;
                int lastIndex = static_cast<int>(currAnim.frames.size()) - 1;
                if (current_frame_index == lastIndex) {
                    static_frame = true;
                    next_animation.clear();
                }
            }
        }
        else {
            auto nit = info->animations.find(next_animation);
            if (nit != info->animations.end()) {
                current_animation = next_animation;
                Animation &anim = nit->second;
                static_frame = (static_cast<int>(anim.frames.size()) <= 1);
                current_frame_index = 0;
            }
            next_animation.clear();
        }
    }

    auto it = info->animations.find(current_animation);
    if (it == info->animations.end()) return;
    Animation &anim = it->second;

    // Advance frame if not marked static
    if (!static_frame) {
        std::string auto_transition;
        bool advanced = anim.advance(current_frame_index, auto_transition);
        if (!advanced && !auto_transition.empty() &&
            info->animations.count(auto_transition))
        {
            next_animation = auto_transition;
        }
    }

    // Recurse into children
    for (auto& c : children) {
        c.update();
    }
}






void Asset::change_animation(const std::string& name) {
    if (!info || name.empty()) return;
    if (name == current_animation) return;
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
    if (!info || !info->flipable) return;

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 1);
    flipped = (dist(rng) == 1);
}


void Asset::set_final_texture(SDL_Texture* tex) {
    if (final_texture) SDL_DestroyTexture(final_texture);
    final_texture = tex;
}

SDL_Texture* Asset::get_final_texture() const {
    return final_texture;
}

int Asset::get_shading_group() const {
    return shading_group;
}

bool Asset::is_shading_group_set() const {
    return shading_group_set;
}


void Asset::set_shading_group(int x){
    shading_group = x;
    shading_group_set = true;
}

// === Updated add_static_light_source using alpha calculation ===
void Asset::add_static_light_source(LightSource* light, int world_x, int world_y) {
    if (!light) return;

    StaticLight sl;
    sl.source = light;
    sl.offset_x = world_x - pos_X;
    sl.offset_y = world_y - pos_Y;
    sl.alpha_percentage = calculate_static_alpha_percentage(pos_Y, world_y);

    static_lights.push_back(sl);
}



void Asset::set_render_player_light(bool value) {
    render_player_light = value;
}

bool Asset::get_render_player_light() const {
    return render_player_light;
}

double Asset::calculate_static_alpha_percentage(int asset_y, int light_world_y) {
    constexpr int FADE_ABOVE = 180;
    constexpr int FADE_BELOW = -30;
    constexpr double MIN_OPACITY = 0.05;
    constexpr double MAX_OPACITY = 0.4;

    int delta_y = light_world_y - asset_y;
    double factor;

    if (delta_y <= -FADE_ABOVE) {
        factor = MIN_OPACITY;
    } else if (delta_y >= FADE_BELOW) {
        factor = MAX_OPACITY;
    } else {
        factor = double(delta_y + FADE_ABOVE) / double(FADE_ABOVE + FADE_BELOW);
        factor = MIN_OPACITY + (MAX_OPACITY - MIN_OPACITY) * factor;
    }

    return std::clamp(factor, MIN_OPACITY, MAX_OPACITY);
}




Area Asset::get_area(const std::string& name) const {
    // Start with an empty/fallback area named appropriately
    Area result(name);

    if (info) {
        if (name == "passability" && info->passability_area) {
            result = *info->passability_area;
        }
        else if (name == "spacing" && info->has_spacing_area && info->spacing_area) {
            result = *info->spacing_area;
        }
        else if (name == "collision" && info->has_collision_area && info->collision_area) {
            result = *info->collision_area;
        }
        else if (name == "interaction" && info->has_interaction_area && info->interaction_area) {
            result = *info->interaction_area;
        }
        else if (name == "attack" && info->has_attack_area && info->attack_area) {
            result = *info->attack_area;
        }
        // otherwise fall back to empty area of that name
    }

    // If the sprite is flipped, mirror the local-area horizontally
    if (flipped) {
        result.flip_horizontal();
    }

    // Finally, move the area into world space at the asset's position
    result.align(pos_X, pos_Y);

    return result;
}





// In Asset.cpp:
void Asset::deactivate() {
    if (final_texture) {
        SDL_DestroyTexture(final_texture);
        final_texture = nullptr;
    }
}
