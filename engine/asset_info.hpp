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
    bool randomize = false;
};

struct ChildAsset {
    std::string asset;
    std::string area_file;
    int z_offset;
    int min;
    int max;
    bool terminate_with_parent;
    Area area;
};

class AssetInfo {
public:
    std::string name;
    std::string type;
    int z_threshold = 0;
    bool is_passable = false;
    bool is_interactable = false;
    bool is_attackable = false;
    bool is_collidable = false;
    int min_same_type_distance = 0;
    int max_child_depth = 0;
    int min_child_depth = 0;
    int child_depth = 0; // random between min_child_depth and max_child_depth
    bool duplicatable = false;
    int duplication_interval_min = 0;
    int duplication_interval_max = 0;
    int duplication_interval = 0; // random between min and max

    bool can_invert = true;

    float scale_percentage = 100.0f;
    float variability_percentage = 0.0f;
    float scale_factor = 1.0f; // = scale_percentage/100

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

    AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer);

private:
    // Updated signature: pass orig_w, orig_h, and (float) scale_factor
    void try_load_area(const nlohmann::json& data,
                       const std::string& key,
                       const std::string& dir,
                       Area& area_ref,
                       bool& flag_ref,
                       int orig_w,
                       int orig_h,
                       float user_scale);
};

#endif // ASSET_INFO_HPP
