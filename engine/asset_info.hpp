#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <SDL.h>
#include <nlohmann/json.hpp>
#include "area.hpp"

struct Animation {
    std::vector<SDL_Texture*> frames;
    bool loop = false;
    bool randomize = false;
    std::string on_end;
};

struct ChildAsset {
    std::string asset;
    int point_x = 0;
    int point_y = 0;
    int radius = 0;
    int z_offset = 0;
    int min = 0;
    int max = 0;
    bool terminate_with_parent = false;
    Area area;
};

class AssetInfo {
public:
    explicit AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer);
    ~AssetInfo();

    void try_load_area(const nlohmann::json& data,
                       const std::string& key,
                       const std::string& dir,
                       Area& area_ref,
                       bool& flag_ref,
                       float scale,
                       int offset_x,
                       int offset_y);

    std::string name;
    std::string type;
    bool has_tag(const std::string& tag) const;

    int z_threshold = 0;
    bool passable = false;
    bool can_invert = true;
    bool interaction = false;
    bool hit = false;
    bool collision = false;

    bool duplicatable = false;
    int duplication_interval_min = 0;
    int duplication_interval_max = 0;
    int duplication_interval = 0;

    int min_same_type_distance = 0;

    int min_child_depth = 0;
    int max_child_depth = 0;
    int child_depth = 0;

    float scale_percentage = 100.0f;
    float variability_percentage = 0.0f;
    float scale_factor = 1.0f;

    int original_canvas_width = 0;
    int original_canvas_height = 0;

    std::vector<std::string> tags;

    bool has_passability_area = false;
    bool has_spacing_area = false;
    bool has_collision_area = false;
    bool has_interaction_area = false;
    bool has_attack_area = false;

    Area passability_area;
    Area spacing_area;
    Area collision_area;
    Area interaction_area;
    Area attack_area;

    std::unordered_map<std::string, Animation> animations;
    std::vector<ChildAsset> child_assets;

    SDL_BlendMode blendmode = SDL_BLENDMODE_BLEND;
};
