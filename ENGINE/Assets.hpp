#pragma once

#include <vector>
#include <unordered_set>
#include "asset.hpp"
#include "active_assets_manager.hpp"

class Assets {
public:
    Assets(std::vector<Asset>&& loaded,
           Asset* player_ptr,
           int screen_width,
           int screen_height,
           int screen_center_x,
           int screen_center_y);

    void update(const std::unordered_set<SDL_Keycode>& keys,
                int screen_center_x,
                int screen_center_y);

    void set_static_lights();
    void set_static_sources();
    void set_player_light_render();

    std::vector<Asset*> get_all_in_range(int cx, int cy, int radius) const;

    Asset* player;
    std::vector<Asset*>& active_assets;
    std::vector<Asset*>& closest_assets;
    std::vector<Asset> all;

private:
    std::unordered_set<Asset*> prev_player_light_assets;

    void update_direction_movement(int offset_x, int offset_y);
    bool check_collision(const Area& a, const Area& b);

    ActiveAssetsManager activeManager;

    int screen_width;
    int screen_height;
    int dx;
    int dy;
    int last_activat_update;
    int update_interval;
};
