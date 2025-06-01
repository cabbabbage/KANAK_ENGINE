#include "asset_info.hpp"
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

AssetInfo::AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer) {
    name = asset_folder_name;
    std::string dir_path = "SRC/" + asset_folder_name;
    std::string info_path = dir_path + "/info.json";

    // 1) Open and parse info.json
    std::ifstream in(info_path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open asset info: " + info_path);
    }
    nlohmann::json data;
    in >> data;

    // 2) Read core metadata fields
    type                     = data.value("asset_type", "Object");
    z_threshold              = data.value("z_threshold", 0);
    is_passable              = data.value("is_passable", false);
    min_same_type_distance   = data.value("min_same_type_distance", 0);
    can_invert               = data.value("can_invert", true);

    max_child_depth           = data.value("max_child_depth", 0);
    min_child_depth           = data.value("min_child_depth", 0);
    duplicatable              = data.value("duplicatable", false);
    duplication_interval_min  = data.value("duplication_interval_min", 0);
    duplication_interval_max  = data.value("duplication_interval_max", 0);

    // 3) Randomize child_depth if requested
    {
        std::mt19937 rng{ std::random_device{}() };
        if (min_child_depth <= max_child_depth) {
            std::uniform_int_distribution<int> dist(min_child_depth, max_child_depth);
            child_depth = dist(rng);
        } else {
            child_depth = 0;
        }
    }
    // 4) Randomize duplication_interval
    {
        std::mt19937 rng{ std::random_device{}() };
        if (duplication_interval_min <= duplication_interval_max && duplicatable) {
            std::uniform_int_distribution<int> dist(duplication_interval_min, duplication_interval_max);
            duplication_interval = dist(rng);
        } else {
            duplication_interval = 0;
        }
    }

    // 5) Check which animations keys exist
    if (data.contains("animations")) {
        auto& anims = data["animations"];
        is_interactable = anims.contains("interaction") && !anims["interaction"].is_null();
        is_attackable   = anims.contains("hit")        && !anims["hit"].is_null();
        is_collidable   = anims.contains("collision")  && !anims["collision"].is_null();
    }

    // 6) Read size_settings (scale_percentage, variability)
    if (data.contains("size_settings")) {
        const auto& ss = data["size_settings"];
        scale_percentage      = ss.value("scale_percentage", 100.0f);
        variability_percentage = ss.value("variability_percentage", 0.0f);
    } else {
        scale_percentage = 100.0f;
        variability_percentage = 0.0f;
    }
    // For now, ignore variability. The main factor is:
    scale_factor = scale_percentage / 100.0f;

    std::cout << "[AssetInfo] Loaded '" << name
              << "'  type=" << type
              << "  scale=" << scale_percentage << "%\n";

    //
    // 7) Load every animation’s frames, *downscaling* by scale_factor
    //
    int orig_sprite_w = 0;
    int orig_sprite_h = 0;
    SDL_Texture* base_sprite = nullptr;

    if (data.contains("animations")) {
        for (auto it = data["animations"].begin(); it != data["animations"].end(); ++it) {
            const std::string& trigger = it.key();
            const auto& anim_json     = it.value();
            if (anim_json.is_null() || !anim_json.contains("frames_path")) continue;

            std::string folder = anim_json["frames_path"];
            std::string frame_path = dir_path + "/" + folder;

            Animation anim;
            anim.loop      = anim_json.value("loop", false);
            anim.on_end    = anim_json.value("on_end", "");
            anim.randomize = anim_json.value("randomize", false);

            std::vector<SDL_Surface*> loaded_surfaces;
            int frame_count = 0;

            // 7.1) Load each PNG into an SDL_Surface, record orig dims on the first frame
            for (int i = 0;; ++i) {
                std::string frame_file = frame_path + "/" + std::to_string(i) + ".png";
                if (!fs::exists(frame_file)) break;

                SDL_Surface* surface = IMG_Load(frame_file.c_str());
                if (!surface) {
                    std::cerr << "[AssetInfo] Failed to load: " << frame_file << "\n";
                    continue;
                }
                SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

                // Record original dimensions from the *first* frame
                if (frame_count == 0) {
                    orig_sprite_w = surface->w;
                    orig_sprite_h = surface->h;
                }

                // 7.2) Downscale this surface by scale_factor:
                int new_w = static_cast<int>(surface->w * scale_factor + 0.5f);
                int new_h = static_cast<int>(surface->h * scale_factor + 0.5f);
                SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(
                    0, new_w, new_h, 32, SDL_PIXELFORMAT_RGBA32
                );
                SDL_BlitScaled(surface, nullptr, scaled_surf, nullptr);
                SDL_FreeSurface(surface);

                loaded_surfaces.push_back(scaled_surf);
                ++frame_count;
            }

            // 7.3) Create an SDL_Texture from each downscaled SDL_Surface
            for (SDL_Surface* surface : loaded_surfaces) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                if (!tex) {
                    std::cerr << "[AssetInfo] Failed to create texture\n";
                    continue;
                }
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                anim.frames.push_back(tex);
            }

            // 7.4) If this is “default” and has at least one frame, record it
            if (trigger == "default" && !anim.frames.empty()) {
                base_sprite = anim.frames[0];
            }

            std::cout << "[AssetInfo] Loaded " << frame_count
                      << " frames for animation '" << trigger << "'\n";
            animations[trigger] = std::move(anim);
        }

        if (!animations.count("default") || animations["default"].frames.empty()) {
            std::cerr << "[AssetInfo] WARNING: no valid 'default' animation for '" << name << "'\n";
        }
    }

    // 8) We must have at least one “default” frame to know orig dimensions
    if (!base_sprite || orig_sprite_w <= 0 || orig_sprite_h <= 0) {
        throw std::runtime_error("[AssetInfo] No default sprite found to compute original dimensions.");
    }

    //
    // 9) Load each boundary JSON via: (json_path, orig_sprite_w, orig_sprite_h, scale_factor)
    //
    try_load_area(data, "impassable_area",  dir_path, passability_area,   has_passability_area,   orig_sprite_w, orig_sprite_h, scale_factor);
    try_load_area(data, "spacing_area",     dir_path, spacing_area,      has_spacing_area,      orig_sprite_w, orig_sprite_h, scale_factor);
    try_load_area(data, "collision_area",   dir_path, collision_area,    has_collision_area,    orig_sprite_w, orig_sprite_h, scale_factor);
    try_load_area(data, "interaction_area", dir_path, interaction_area,  has_interaction_area,  orig_sprite_w, orig_sprite_h, scale_factor);
    try_load_area(data, "hit_area",         dir_path, attack_area,       has_attack_area,       orig_sprite_w, orig_sprite_h, scale_factor);

    //
    // 10) Load any child‐asset areas in exactly the same way:
    //
    if (data.contains("child_assets")) {
        int idx = 0;
        for (const auto& c : data["child_assets"]) {
            try {
                std::string cname     = c.at("asset");
                std::string area_file = c.at("area_file");
                int z      = c.at("z_offset").get<int>();
                int mn     = c.at("min").get<int>();
                int mx     = c.at("max").get<int>();
                bool term  = c.at("terminate_with_parent").get<bool>();

                std::string area_path = dir_path + "/" + area_file;
                Area child_area(area_path, orig_sprite_w, orig_sprite_h, scale_factor);

                ChildAsset ca;
                ca.asset                 = std::move(cname);
                ca.area_file             = std::move(area_file);
                ca.z_offset              = z;
                ca.min                   = mn;
                ca.max                   = mx;
                ca.terminate_with_parent = term;
                ca.area                  = std::move(child_area);
                child_assets.push_back(std::move(ca));
            }
            catch(const std::exception& e) {
                std::cerr << "[AssetInfo] child_assets["<<idx<<"] load error: "<< e.what() <<"\n";
            }
            ++idx;
        }
    }
}

// ----------------------------------------------------------------------------------
// Updated try_load_area: now takes (orig_w, orig_h, user_scale).
// It constructs an Area(json_path, orig_w, orig_h, user_scale).
// ----------------------------------------------------------------------------------
void AssetInfo::try_load_area(const nlohmann::json& data,
                              const std::string&     key,
                              const std::string&     dir,
                              Area&                  area_ref,
                              bool&                  flag_ref,
                              int                    orig_w,
                              int                    orig_h,
                              float                  user_scale)
{
    if (data.contains(key) && data[key].is_string()) {
        try {
            std::string path = dir + "/" + data[key].get<std::string>();
            area_ref = Area(path, orig_w, orig_h, user_scale);
            flag_ref = true;
        }
        catch(const std::exception& e) {
            std::cerr << "[AssetInfo] warning: failed to load area '"
                      << key << "': " << e.what() << "\n";
        }
    }
}
