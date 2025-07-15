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

    namespace fs = std::filesystem;
    using json = nlohmann::json;

    // -- always clear out any old light textures --
    light_textures.clear();

    // -- only generate lights if this asset actually has them --
    if (info->has_light_source) {
        // root cache: ./cache/<asset_name>/lights
        std::string cache_root = "cache/" + info->name + "/lights";
        fs::create_directories(cache_root);

        for (size_t i = 0; i < info->lights.size(); ++i) {
            const auto& ls = info->lights[i];
            std::string folder    = cache_root + "/" + std::to_string(i);
            std::string meta_file = folder + "/metadata.json";
            std::string img_file  = folder + "/light.png";

            bool use_cache = false;
            if (fs::exists(meta_file) && fs::exists(img_file)) {
                std::ifstream mfs(meta_file);
                if (mfs) {
                    json meta; mfs >> meta;
                    if (meta.value("radius",-1)==ls.radius
                     && meta.value("fall_off",-1)==ls.fall_off
                     && meta.value("intensity",-1)==ls.intensity
                     && meta["color"][0].get<int>()==ls.color.r
                     && meta["color"][1].get<int>()==ls.color.g
                     && meta["color"][2].get<int>()==ls.color.b)
                    {
                        use_cache = true;
                    }
                }
            }

            SDL_Texture* tex = nullptr;
            // load from cache
            if (use_cache) {
                if (SDL_Surface* surf = IMG_Load(img_file.c_str())) {
                    tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_FreeSurface(surf);
                }
            }
            // regenerate & cache if needed
            if (!tex) {
                fs::remove_all(folder);
                fs::create_directories(folder);

                GenerateLight gen(renderer);
                tex = gen.generate(this, ls);
                if (!tex) continue;

                // write metadata
                json meta;
                meta["radius"]    = ls.radius;
                meta["fall_off"]  = ls.fall_off;
                meta["intensity"] = ls.intensity;
                meta["color"]     = { ls.color.r, ls.color.g, ls.color.b };
                std::ofstream(folder + "/metadata.json") << meta.dump(4);

                // capture output into a PNG
                int w, h;
                SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
                SDL_Texture* prev_rt = SDL_GetRenderTarget(renderer);
                SDL_SetRenderTarget(renderer, tex);
                SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
                    0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
                SDL_RenderReadPixels(renderer,
                                     nullptr,
                                     SDL_PIXELFORMAT_RGBA32,
                                     surf->pixels,
                                     surf->pitch);
                IMG_SavePNG(surf, img_file.c_str());
                SDL_FreeSurface(surf);
                SDL_SetRenderTarget(renderer, prev_rt);
            }

            if (tex) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                light_textures.push_back(tex);
            }
        }
    }

    // -- always ensure we have a valid animation now that all textures are loaded --
    bool missing = current_animation.empty()
                || info->animations[current_animation].frames.empty();
    if (missing) {
        auto it = info->animations.find("start");
        if (it == info->animations.end()) it = info->animations.find("default");
        if (it == info->animations.end()) it = info->animations.begin();
        if (it != info->animations.end() && !it->second.frames.empty()) {
            change_animation(it->first);
        }
    }

    // -- spawn children regardless of light source --
    spawn_children(renderer);
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
    children.push_back(std::move(child));
}

void Asset::set_z_index() {
    if (info->type == "Player") {
        z_index = pos_Y + info->z_threshold;
    } else if (parent) {
        int base = pos_Y + info->z_threshold;
        z_index = std::clamp(base + z_offset,
                             parent->z_index - 1000,
                             parent->z_index + 1000);
    } else {
        z_index = pos_Y + info->z_threshold;
    }
}



void Asset::spawn_children(SDL_Renderer* renderer) {
    if (!info || !renderer) {
        std::cout << "[spawn_children] Missing asset info or renderer.\n";
        return;
    }

    std::mt19937 rng(std::random_device{}());

    for (const auto& ca : info->child_assets) {
        std::cout << "[spawn_children] Attempting to spawn child asset: " << ca.asset << "\n";

        std::shared_ptr<AssetInfo> child_info = std::make_shared<AssetInfo>(ca.asset);
        if (!child_info) {
            std::cout << "[spawn_children] Failed to construct AssetInfo for: " << ca.asset << "\n";
            continue;
        }

        std::uniform_int_distribution<int> dist(ca.min, ca.max);
        int count = dist(rng);
        std::cout << "[spawn_children] Will attempt to spawn " << count << " instances.\n";

        Area aligned_area = ca.area;

        // Align to bottom-center of parent sprite
        int anchor_x = pos_X;
        int anchor_y = pos_Y + static_cast<int>(info->original_canvas_height * info->scale_factor / 2.0f);
        aligned_area.align(anchor_x, anchor_y);

        const auto& valid_points = aligned_area.get_points();
        std::cout << "[spawn_children] Aligned area has " << valid_points.size() << " candidate points.\n";

        if (valid_points.empty()) {
            std::cout << "[spawn_children] No valid points to spawn within area.\n";
            continue;
        }

        std::uniform_int_distribution<size_t> point_picker(0, valid_points.size() - 1);

        for (int i = 0; i < count; ++i) {
            Area::Point pt = valid_points[point_picker(rng)];
            std::cout << "[spawn_children] Spawning at (" << pt.first << ", " << pt.second << ")\n";

            Asset child(child_info,
                        ca.z_offset,
                        aligned_area,
                        pt.first,
                        pt.second,
                        this);

            child_info->loadAnimations(renderer);
            child.finalize_setup(renderer);

            std::cout << "[spawn_children] Child asset created and finalized.\n";
            children.push_back(std::move(child));
        }
    }
}
