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
#include "asset_library.hpp"
#include "asset_spawner.hpp"
#include "asset_spawn_planner.hpp"

class AssetLibrary;

class Asset {
public:
    std::vector<SDL_Texture*> light_textures;

    Asset(std::shared_ptr<AssetInfo> info,
          const Area& spawn_area,
          int start_pos_X,
          int start_pos_Y,
          int depth,
          Asset* parent = nullptr);

    void finalize_setup(SDL_Renderer* renderer, AssetLibrary* asset_library);

    void set_position(int x, int y);
    void update();
    void change_animation(const std::string& name);

    SDL_Texture* get_current_frame() const;
    SDL_Texture* get_image() const;

    std::string get_current_animation() const;
    std::string get_type() const;

    Area get_global_collision_area() const;
    Area get_global_interaction_area() const;
    Area get_global_attack_area() const;
    Area get_global_spacing_area() const;
    Area get_global_passability_area() const;

    void add_child(Asset child);

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
    int depth = 0;

private:
    void spawn_children(AssetLibrary* asset_library);
    void set_z_index();

    int current_frame_index = 0;
    bool static_frame = true;
    std::unordered_map<std::string, std::vector<SDL_Texture*>> custom_frames;
};

#endif // ASSET_HPP
