#pragma once
#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "asset.hpp"
#include "area.hpp"
#include "controls_manager.hpp"  // ✅ Ensure this is included
#include "active_assets_manager.hpp"

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
           int screen_center_y, int map_radius);

    void update(const std::unordered_set<SDL_Keycode>& keys,
                int screen_center_x,
                int screen_center_y);

    void activate(Asset* asset);
    void set_shading_groups();
    void set_static_sources();
    void set_player_light_render();

    std::vector<Asset*> get_all_in_range(int cx, int cy, int radius) const;

    std::vector<Asset*> active_assets;
    std::vector<Asset>  all;
    std::vector<Asset*> closest_assets;
    Asset*              player = nullptr;
    int                 visible_count = 0;
    view& getView() { return window; }
private:
    view window;
    ControlsManager     controls;            // ✅ Now properly declared
    ActiveAssetsManager activeManager;

    int screen_width;
    int screen_height;

    int dx = 0;
    int dy = 0;
    int last_activat_update = 0;
    int update_interval = 25;
    int num_groups_ = 40;
};

#endif // ASSETS_HPP
