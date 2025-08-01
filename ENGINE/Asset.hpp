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
#include "asset_spawn_planner.hpp"

struct StaticLightSource {
    SDL_Texture* texture;
    int world_x;
    int world_y;
    int width;
    int height;
    int radius;
};

class Asset {
public:
    // existing members
    std::vector<SDL_Texture*> light_textures;

    // new static lights
    std::vector<StaticLightSource> static_light_sources;

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


    void add_static_light_source(SDL_Texture* texture,
                                 int world_x,
                                 int world_y,
                                 int width,
                                 int height,
                                 int radius);

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
    void set_z_offset(int z);
    bool flipped;
    void set_last_mask(SDL_Texture* tex);
    int get_shading_group();
    SDL_Texture* get_last_mask();
    void set_shading_group(int x);
    bool is_shading_group_set();
    SDL_Texture* get_current_mask() const; 
private:
    int shading_group;
    bool shading_group_set = false;
    void set_flip();
    std::string next_animation;
    void set_z_index();
    SDL_Texture* last_mask = nullptr;
    int current_frame_index = 0;
    bool static_frame = true;
    std::unordered_map<std::string, std::vector<SDL_Texture*>> custom_frames;
};




#endif // ASSET_HPP
