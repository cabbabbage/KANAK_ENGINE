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
#include "asset_spawn_planner.hpp"
#include "light_source.hpp"

struct StaticLight {
    LightSource* source = nullptr;
    int offset_x = 0;
    int offset_y = 0;
};


class Asset {
public:
    Asset(std::shared_ptr<AssetInfo> info,
          const Area& spawn_area,
          int start_pos_X,
          int start_pos_Y,
          int depth,
          Asset* parent = nullptr);

    void finalize_setup(SDL_Renderer* renderer);
    void set_position(int x, int y);
    void update();
    void change_animation(const std::string& name);

    SDL_Texture* get_current_frame() const;
    SDL_Texture* get_image() const;
    std::string get_current_animation() const;
    std::string get_type() const;
    void add_child(Asset child);

    void add_static_light_source(LightSource* light, int world_x, int world_y);

    void set_render_player_light(bool value);
    bool get_render_player_light() const;

    void set_z_offset(int z);
    void set_last_mask(SDL_Texture* tex);
    void set_shading_group(int x);
    bool is_shading_group_set();
    int get_shading_group();
    SDL_Texture* get_last_mask();

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
    bool has_base_shadow = false;
    bool active = false;
    bool flipped = false;
    bool render_player_light = false;

    double alpha_percentage = 1.0;

    Area spawn_area_local;
    std::vector<Area> base_areas;
    std::vector<Area> areas;
    std::vector<Asset> children;
    std::vector<StaticLight> static_lights;
    int gradient_shadow;
    int depth = 0;
    bool has_shading; 
private:
    void set_flip();
    void set_z_index();

    std::string next_animation;
    int current_frame_index = 0;
    bool static_frame = true;

    int shading_group = 0;
    bool shading_group_set = false;
    SDL_Texture* last_mask = nullptr;

    std::unordered_map<std::string, std::vector<SDL_Texture*>> custom_frames;
};

#endif // ASSET_HPP
