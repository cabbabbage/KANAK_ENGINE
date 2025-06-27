// === File: Asset.hpp ===
#ifndef ASSET_HPP
#define ASSET_HPP

#include <string>
#include <vector>
#include <memory>
#include <SDL.h>
#include "area.hpp"
#include "asset_info.hpp"
#include "generate_light.hpp"
#include "generate_light.hpp"


class Asset {
public:
    Asset(int z_offset,
          const Area& spawn_area,
          SDL_Renderer* renderer,
          Asset* parent = nullptr);

    std::unordered_map<std::string, std::vector<SDL_Texture*>> custom_frames;


    void rebuild_world_areas();

    /// Initializes animations, base_areas, computes parent-offsets, positions, and world-areas.
    void finalize_setup(int start_pos_X,
                        int start_pos_Y,
                        int parent_X = 0,
                        int parent_Y = 0);

    /// Moves the asset (and all its children) to a new world position.
    void set_position(int x, int y);

    /// Advances animation frames and updates children recursively.
    void update();

    /// Switches to a different named animation.
    void change_animation(const std::string& name);

    /// Returns the current animation frame texture.
    SDL_Texture* get_current_frame() const;
    SDL_Texture* get_image() const;

    std::string get_current_animation() const;
    std::string get_type() const;

    bool is_lit = false;
    bool is_shaded = false;


    SDL_Texture* base_shadow_texture = nullptr;
    SDL_Texture* light_texture = nullptr;
    std::vector<SDL_Texture*> gradient_textures;
    std::vector<SDL_Texture*> casted_shadows;
    
    Area get_global_collision_area() const;
    Area get_global_interaction_area() const;
    Area get_global_attack_area() const;
    Area get_global_spacing_area() const;
    Area get_global_passability_area() const;

    void add_child(Asset child);

    Asset* parent;
    std::shared_ptr<AssetInfo> info;
    std::string current_animation;
    int pos_X = 0;
    int pos_Y = 0;
    int z_index = 0;
    int z_offset = 0;
    int offset_from_parent_X = 0;
    int offset_from_parent_Y = 0;
    int player_speed_mult = 10;

    SDL_Renderer* renderer = nullptr;
    Area spawn_area_local;

    std::vector<Area> base_areas;
    std::vector<Area> areas;
    std::vector<Asset> children;
    mutable SDL_Texture* cached_lit_texture = nullptr;
    mutable int last_cached_opacity = -1;

    bool active = false;
    double gradient_opacity = 80;
    bool has_base_shadow = false;
    void set_z_index();
    int current_frame_index = 0;
private:

    bool static_frame = true;

};

#endif // ASSET_HPP