// === File: Asset.hpp ===
#ifndef ASSET_HPP
#define ASSET_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <SDL.h>
#include "area.hpp"
#include "asset_info.hpp"

class Asset {
public:
    // Textures generated for lighting
    std::vector<SDL_Texture*> light_textures;

    // Constructor does everything except rendering-dependent lighting
    Asset(std::shared_ptr<AssetInfo> info,
          int z_offset,
          const Area& spawn_area,
          int start_pos_X,
          int start_pos_Y,
          Asset* parent = nullptr);

    // Call this when you have an SDL_Renderer to generate lighting textures
    void finalize_setup(SDL_Renderer* renderer);

    // Move the asset and recompute its z-index
    void set_position(int x, int y);

    // Advance its animation and update children
    void update();

    // Switch to a named animation
    void change_animation(const std::string& name);

    // Get the current frame texture
    SDL_Texture* get_current_frame() const;
    SDL_Texture* get_image() const;

    std::string get_current_animation() const;
    std::string get_type() const;

    // Query aligned collision/interaction/etc areas
    Area get_global_collision_area() const;
    Area get_global_interaction_area() const;
    Area get_global_attack_area() const;
    Area get_global_spacing_area() const;
    Area get_global_passability_area() const;

    // Add a child asset
    void add_child(Asset child);

    // Public state
    Asset* parent = nullptr;
    std::shared_ptr<AssetInfo> info;
    std::string current_animation;
    int pos_X = 0;
    int pos_Y = 0;
    int z_index = 0;
    int z_offset = 0;
    int player_speed_mult = 10;
    bool is_lit = false;
    bool is_shaded = false;
    double gradient_opacity = 1.0;
    bool has_base_shadow = false;
    bool active = false;
    Area spawn_area_local;
    std::vector<Area> base_areas;
    std::vector<Area> areas;
    std::vector<Asset> children;
    int depth = 0;  // Tracks nesting level for limiting child spawning

private:

    void set_z_index();
    int current_frame_index = 0;
    bool static_frame = true;
    std::unordered_map<std::string, std::vector<SDL_Texture*>> custom_frames;
};

#endif // ASSET_HPP
