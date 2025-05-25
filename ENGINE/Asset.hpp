#ifndef ASSET_HPP
#define ASSET_HPP

#include "asset_info.hpp"
#include "area.hpp"
#include <SDL.h>
#include <string>

class Asset {
public:
    int pos_X = 0;
    int pos_Y = 0;
    int z_index = 0;
    int z_offset = 0;
    Area spawn_area;
    AssetInfo* info = nullptr;

    std::string current_animation = "default";
    int current_frame_index = 0;

    Asset(int z_offset = 0, const Area& spawn_area = Area());

    void finalize_setup();  // ← add this declaration

    void update(bool player_is_moving);
    void change_animation(const std::string& animation_name);

    SDL_Texture* get_current_frame() const;
    SDL_Texture* get_image() const;

    void set_z_index(int z);
    void set_position(int x, int y);
    std::string get_type() const;

    Area get_global_collision_area() const;
    Area get_global_interaction_area() const;
    Area get_global_attack_area() const;
    Area get_global_spacing_area() const;
    Area get_global_passability_area() const;
};

#endif
