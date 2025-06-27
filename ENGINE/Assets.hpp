// Assets.hpp
#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "asset.hpp"
#include "area.hpp"
#include <vector>
#include <unordered_set>
#include <SDL.h>

class Assets {
public:
    Assets(std::vector<Asset>&& loaded,
           Asset* player_ptr,
           int screen_width,
           int screen_height,
           int screen_center_x,
           int screen_center_y);

    void update(const std::unordered_set<SDL_Keycode>& keys, int screen_center_x, int screen_center_y);
    void sort_assets_by_distance_to_screen_center(int cx, int cy);
    void add_active(Asset* a);
    void remove_active(Asset* a);
    void resort_active_asset(Asset* a);
    void activate(Asset* asset);

    std::vector<Asset*> active_assets;
    Asset* player;
    int visible_count;
    std::vector<Asset> all;
private:

    std::vector<Asset*> closest_assets;
    int dx = 0, dy = 0;
    int last_activat_update = 0;
    int update_interval = 40;
    int screen_width;
    int screen_height;

    void update_closest();
    void update_direction_movement(int offset_x, int offset_y);
    bool check_collision(const Area& a, const Area& b);
};

#endif // ASSETS_HPP
