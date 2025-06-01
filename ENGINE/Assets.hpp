#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "asset.hpp"
#include "area.hpp"
#include <vector>
#include <unordered_set>
#include <memory>
#include <SDL.h>

class Assets {
public:
    Assets(std::vector<std::unique_ptr<Asset>>&& loaded,
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

private:
    std::vector<std::unique_ptr<Asset>> all; // ✅ FIXED: use unique_ptr here
    int screen_width;
    int screen_height;
};

#endif // ASSETS_HPP
