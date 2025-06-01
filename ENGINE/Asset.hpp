#ifndef ASSET_HPP
#define ASSET_HPP

#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <SDL.h>
#include "area.hpp"
#include "asset_info.hpp"

class Asset {
public:
    Asset(int z_offset, const Area& spawn_area, SDL_Renderer* renderer, Asset* parent = nullptr);

    void finalize_setup();
    void update(const std::unordered_set<SDL_Keycode>& keys, const Area* compare_area = nullptr);
    SDL_Texture* get_image() const;
    void set_position(int x, int y);
    void set_z_index();
    std::string get_type() const;

    Area get_global_collision_area() const;
    Area get_global_interaction_area() const;
    Area get_global_attack_area() const;
    Area get_global_spacing_area() const;
    Area get_global_passability_area() const;

    void add_child(const Asset& child);

    bool active = false;
    int pos_X = 0;
    int pos_Y = 0;
    int z_index = 0;
    int z_offset = 0;

    std::shared_ptr<AssetInfo> info;
    std::vector<Area> areas;
    std::vector<Asset> children;

private:
    std::vector<Area> base_areas;
    SDL_Renderer* renderer;
    Asset* parent;

    Area spawn_area_local;

    std::string current_animation;
    int current_frame_index = 0;
    bool static_frame = true;

    void rebuild_world_areas();
    void randomize_frame();
    void change_animation(const std::string& animation_name);
    SDL_Texture* get_current_frame() const;
};

#endif // ASSET_HPP
