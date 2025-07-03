#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <SDL.h>
#include <nlohmann/json.hpp>
#include "area.hpp"

#include "gradient.hpp"

#include <memory>  // for std::unique_ptr


struct LightSource {
    SDL_Color color;
    int intensity = 0;
    int radius = 100;
    int fall_off = 0;
    int jitter_min = 0;
    int jitter_max = 0;
    bool flicker = false;
    int offset_x = 0;
    int offset_y = 0;
};


struct Animation {
    std::vector<SDL_Texture*> frames;
    // store pointers to Gradient so they own their own resources
    std::vector<std::unique_ptr<Gradient>> gradients;
    std::string on_end;
    bool randomize = false;
    bool loop = false;
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
    std::vector<LightSource> lights;

    void apply_base_gradient(SDL_Renderer* renderer);
    std::string name;
    std::string type;
    int z_threshold = 0;
    bool passable = false;

    bool interaction = false;
    bool hit = false;
    bool collision = false;

    int min_same_type_distance = 0;
    bool can_invert = true;
    int max_child_depth = 0;
    int min_child_depth = 0;
    int child_depth = 0;
    bool duplicatable = false;
    int duplication_interval_min = 0;
    int duplication_interval_max = 0;
    int duplication_interval = 0;

    float scale_percentage = 100.0f;
    float variability_percentage = 0.0f;
    float scale_factor = 1.0f;

    int original_canvas_width = 0;
    int original_canvas_height = 0;

    SDL_BlendMode blendmode = SDL_BLENDMODE_BLEND;

    std::vector<std::string> tags;
    std::unordered_map<std::string, Animation> animations;
    std::vector<ChildAsset> child_assets;

    // Lighting
    bool has_light_source = false;
    int light_intensity = 0;
    SDL_Color light_color = {0, 0, 0, 0};
    int radius = 100;
    int fall_off = 0;
    int jitter_min = 0;
    int jitter_max = 0;
    bool flicker = false;

    // Shading
    bool has_shading = false;
    bool has_base_shadow = false;
    int base_shadow_intensity = 0;
    bool has_gradient_shadow = false;
    int number_of_gradient_shadows = 0;
    int gradient_shadow_intensity = 0;
    bool has_casted_shadows = false;
    int number_of_casted_shadows = 0;
    int cast_shadow_intensity = 0;

    // Areas
    Area passability_area;
    Area spacing_area;
    Area collision_area;
    Area interaction_area;
    Area attack_area;

    bool has_passability_area = false;
    bool has_spacing_area = false;
    bool has_collision_area = false;
    bool has_interaction_area = false;
    bool has_attack_area = false;

public:
    AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer);
    ~AssetInfo();

    bool has_tag(const std::string& tag) const;

private:
    void load_base_properties(const nlohmann::json& data);
    void load_lighting_info(const nlohmann::json& data);
    void load_shading_info(const nlohmann::json& data);
    void load_collision_areas(const nlohmann::json& data,
                              const std::string& dir_path,
                              int offset_x,
                              int offset_y);
    void load_child_assets(const nlohmann::json& data,
                           float scale_factor,
                           int offset_x,
                           int offset_y);
    void load_animations(const nlohmann::json& anims_json,
                         const std::string& dir_path,
                         SDL_Renderer* renderer,
                         SDL_Texture*& base_sprite,
                         int& scaled_sprite_w,
                         int& scaled_sprite_h);
    void try_load_area(const nlohmann::json& data,
                       const std::string& key,
                       const std::string& dir,
                       Area& area_ref,
                       bool& flag_ref,
                       float scale,
                       int offset_x,
                       int offset_y);
};
