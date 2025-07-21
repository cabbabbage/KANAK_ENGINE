// === File: asset_info.hpp ===
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <SDL.h>
#include <nlohmann/json.hpp>
#include "Area.hpp"
#include "generate_light.hpp"

struct Animation {
    std::vector<SDL_Texture*> frames;
    std::string on_end;
    bool loop;
    bool randomize;
};

struct LightSource {
    int intensity;
    int radius;
    int fall_off;
    int jitter_min;
    int jitter_max;
    bool flicker;
    int offset_x;
    int offset_y;
    SDL_Color color;
};

struct ChildAsset {
    std::string asset;
    int point_x;
    int point_y;
    int radius;
    int z_offset;
    int min;
    int max;
    bool terminate_with_parent;
    Area area;
};

class AssetInfo {
public:
    // Constructor no longer takes SDL_Renderer; animations are loaded separately
    AssetInfo(const std::string& asset_folder_name);
    ~AssetInfo();

    // Call this after creating a renderer to load animation textures
    void loadAnimations(SDL_Renderer* renderer);

    bool has_tag(const std::string& tag) const;

    // Public properties
    std::string                   name;
    std::string                   type;
    int                           z_threshold;
    bool                          passable;
    int                           min_same_type_distance;
    bool                          can_invert;
    int                           max_child_depth;
    int                           min_child_depth;
    int                           child_depth;
    bool                          duplicatable;
    int                           duplication_interval_min;
    int                           duplication_interval_max;
    int                           duplication_interval;
    float                         scale_percentage;
    float                         variability_percentage;
    float                         scale_factor;
    int                           original_canvas_width;
    int                           original_canvas_height;

    std::vector<std::string>      tags;
    SDL_BlendMode                 blendmode;

    // Flags indicating presence of animation triggers
    bool                          interaction;
    bool                          hit;
    bool                          collision;

    // Lighting and shading
    std::vector<LightSource>      lights;
    bool                          has_light_source;
    bool                          has_shading;
    bool                          has_base_shadow;
    int                           base_shadow_intensity;
    bool                          has_gradient_shadow;
    int                           number_of_gradient_shadows;
    int                           gradient_shadow_intensity;
    bool                          has_casted_shadows;
    int                           number_of_casted_shadows;
    int                           cast_shadow_intensity;
    std::vector<SDL_Texture*> light_textures; 

    // Collision/interaction areas
    Area                          passability_area;
    bool                          has_passability_area;
    Area                          spacing_area;
    bool                          has_spacing_area;
    Area                          collision_area;
    bool                          has_collision_area;
    Area                          interaction_area;
    bool                          has_interaction_area;
    Area                          attack_area;
    bool                          has_attack_area;

    // Animations and child assets
    std::map<std::string, Animation> animations;
    std::vector<ChildAsset>          child_assets;

private:
    void load_base_properties(const nlohmann::json& data);
    void load_lighting_info    (const nlohmann::json& data);
    void generate_lights(SDL_Renderer* renderer);
    void load_shading_info     (const nlohmann::json& data);
    void load_collision_areas  (const nlohmann::json& data,
                                const std::string& dir_path,
                                int offset_x, int offset_y);
    void load_child_assets(const nlohmann::json& data,
                        const std::string& dir_path,
                        float scale_factor,
                        int offset_x,
                        int offset_y);

    void load_animations       (const nlohmann::json& anims_json,
                                const std::string& dir_path,
                                SDL_Renderer* renderer,
                                SDL_Texture*& base_sprite,
                                int& scaled_sprite_w,
                                int& scaled_sprite_h);
    void try_load_area         (const nlohmann::json& data,
                                const std::string& key,
                                const std::string& dir,
                                Area& area_ref,
                                bool& flag_ref,
                                float scale,
                                int offset_x,
                                int offset_y);

    // Stored so we can defer loading textures until we have a renderer
    nlohmann::json anims_json_;
    std::string     dir_path_;
};
