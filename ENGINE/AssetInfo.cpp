#ifndef ASSET_INFO_HPP
#define ASSET_INFO_HPP

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <SDL_image.h>

namespace fs = std::filesystem;

struct Animation {
    std::vector<SDL_Texture*> frames;
    bool loop = false;
    std::string on_end;
};

struct ChildAsset {
    std::string asset;
    std::string area_file;
    int z_offset;
    int min;
    int max;
    float skew;
    bool terminate_with_parent;
};

class AssetInfo {
public:
    std::string name;
    std::string type;
    int z_threshold;
    bool is_passable;
    bool is_interactable;
    bool is_attackable;
    bool is_collidable;
    bool child_only;

    std::unordered_map<std::string, Animation> animations;
    std::vector<ChildAsset> child_assets;

    // Path references for boundaries (single-purpose geometry files)
    std::string collision_area_path;
    std::string interaction_area_path;
    std::string attack_area_path;
    std::string spacing_area_path;

    AssetInfo(const std::string& asset_type, SDL_Renderer* renderer) {
        name = asset_type;
        std::string dir_path = "../SRC/" + asset_type;
        std::string info_path = dir_path + "/info.json";

        std::ifstream file(info_path);
        if (!file.is_open()) throw std::runtime_error("Missing info.json for asset: " + asset_type);

        nlohmann::json data;
        file >> data;

        type = data.value("asset_type", "Unknown");
        z_threshold = data.value("z_threshold", 0);
        is_passable = data.value("is_passable", false);
        is_interactable = data.value("is_interactable", false);
        is_attackable = data.value("is_attackable", false);
        is_collidable = data.value("is_collidable", false);
        child_only = data.value("child_only", false);

        if (data.contains("collision_area")) collision_area_path = dir_path + "/" + data["collision_area"].get<std::string>();
        if (data.contains("interaction_area")) interaction_area_path = dir_path + "/" + data["interaction_area"].get<std::string>();
        if (data.contains("attack_area")) attack_area_path = dir_path + "/" + data["attack_area"].get<std::string>();
        if (data.contains("spacing_area")) spacing_area_path = dir_path + "/" + data["spacing_area"].get<std::string>();

        if (data.contains("child_assets")) {
            for (const auto& c : data["child_assets"]) {
                child_assets.push_back({
                    c["asset"],
                    c["area_file"],
                    c.value("z_offset", 0),
                    c.value("min", 1),
                    c.value("max", 1),
                    c.value("skew", 0.0f),
                    c.value("terminate_with_parent", false)
                });
            }
        }

        if (data.contains("available_animations")) {
            for (const std::string& anim_type : data["available_animations"]) {
                std::string anim_key = anim_type + "_animation";
                if (data.contains(anim_key)) {
                    const auto& anim_json = data[anim_key];
                    std::string frame_path = dir_path + "/" + anim_json["frames_path"].get<std::string>();
                    Animation anim;
                    anim.loop = anim_json.value("loop", false);
                    anim.on_end = anim_json.value("on_end", "");

                    for (int i = 0;; ++i) {
                        std::string frame_file = frame_path + "/" + std::to_string(i) + ".png";
                        if (!fs::exists(frame_file)) break;
                        SDL_Surface* surface = IMG_Load(frame_file.c_str());
                        if (!surface) break;
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
                        SDL_FreeSurface(surface);
                        if (!tex) break;
                        anim.frames.push_back(tex);
                    }

                    animations[anim_type] = anim;
                }
            }
        }
    }
};

#endif
