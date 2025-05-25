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
#include <iostream>
#include "area.hpp"

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

    Area collision_area;
    Area interaction_area;
    Area attack_area;
    Area spacing_area;
    Area passability_area;

    bool has_collision_area = false;
    bool has_interaction_area = false;
    bool has_attack_area = false;
    bool has_spacing_area = false;
    bool has_passability_area = false;

    AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer) {
        name = asset_folder_name;
        std::string dir_path = "SRC/" + asset_folder_name;
        std::string info_path = dir_path + "/info.json";

        std::ifstream in(info_path);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open asset info: " + info_path);
        }

        nlohmann::json data;
        try {
            in >> data;
        } catch (const std::exception& e) {
            std::cerr << "[AssetInfo] Failed to parse JSON in asset: " << name << "\n";
            std::cerr << "  Exception: " << e.what() << "\n";
            throw;
        }

        try {
            if (!data.contains("asset_type") || !data["asset_type"].is_string()) {
                throw std::runtime_error("Missing or invalid 'asset_type' in " + info_path);
            }
            type = data["asset_type"].get<std::string>();

            z_threshold = data.value("z_threshold", 0);
            is_passable = data.value("is_passable", false);
            is_interactable = data.value("is_interactable", false);
            is_attackable = data.value("is_attackable", false);
            is_collidable = data.value("is_collidable", false);
            child_only = data.value("child_only", false);
        } catch (const std::exception& e) {
            std::cerr << "[AssetInfo] Error parsing top-level fields in asset: " << name << "\n";
            std::cerr << "  Exception: " << e.what() << "\n";
            throw;
        }

        std::cout << "[AssetInfo] Loaded '" << name << "' with type: " << type << "\n";

        try_load_area(data, "collision_area", dir_path, collision_area, has_collision_area);
        try_load_area(data, "interaction_area", dir_path, interaction_area, has_interaction_area);
        try_load_area(data, "attack_area", dir_path, attack_area, has_attack_area);
        try_load_area(data, "spacing_area", dir_path, spacing_area, has_spacing_area);
        try_load_area(data, "passability_area", dir_path, passability_area, has_passability_area);

        if (data.contains("child_assets")) {
            int index = 0;
            for (const auto& c : data["child_assets"]) {
                try {
                    std::string cname = c.at("asset");
                    std::string area_file = c.at("area_file");

                    int z = c.at("z_offset").get<int>();
                    int min = c.at("min").get<int>();
                    int max = c.at("max").get<int>();
                    float skew = c.at("skew").get<float>();
                    bool terminate = c.at("terminate_with_parent").get<bool>();

                    child_assets.push_back({ cname, area_file, z, min, max, skew, terminate });
                } catch (const std::exception& e) {
                    std::cerr << "[AssetInfo] Error in asset '" << name << "' child_assets[" << index << "]\n";
                    std::cerr << "  Raw JSON: " << c.dump(2) << "\n";
                    std::cerr << "  Exception: " << e.what() << "\n";
                }
                ++index;
            }
        }

        if (data.contains("available_animations")) {
            for (const std::string& anim_type : data["available_animations"]) {
                std::string anim_key = anim_type + "_animation";
                if (data.contains(anim_key)) {
                    const auto& anim_json = data[anim_key];
                    if (!anim_json.contains("frames_path") || !anim_json["frames_path"].is_string())
                        continue;

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

private:
    void try_load_area(const nlohmann::json& data, const std::string& key, const std::string& dir,
                       Area& area_ref, bool& flag_ref) {
        if (data.contains(key) && data[key].is_string()) {
            try {
                area_ref = Area(dir + "/" + data[key].get<std::string>());
                flag_ref = true;
            } catch (...) {
                std::cerr << "[AssetInfo] Warning: failed to load area '" << key << "' from " << dir << "/" << data[key] << "\n";
            }
        }
    }
};

#endif
