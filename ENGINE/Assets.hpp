// === File: assets.hpp ===
#pragma once
#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "asset.hpp"
#include "area.hpp"
#include <vector>
#include <unordered_set>
#include <SDL.h>
#include "active_assets_manager.hpp"

class Assets {
public:
    void set_shading_groups();
    Assets(std::vector<Asset>&& loaded,
           Asset* player_ptr,
           int screen_width,
           int screen_height,
           int screen_center_x,
           int screen_center_y);

    void update(const std::unordered_set<SDL_Keycode>& keys,
                int screen_center_x,
                int screen_center_y);
    void activate(Asset* asset);

    std::vector<Asset*> active_assets;
    Asset*              player            = nullptr;
    int                 visible_count     = 0;
    std::vector<Asset>  all;

    void                          set_static_sources();
    void                          set_player_light_render();
    std::vector<Asset*>           get_all_in_range(int cx, int cy, int radius) const;

private:
    int num_groups_ = 100;
    ActiveAssetsManager           activeManager;
    std::vector<Asset*>           closest_assets;
    int                           dx                 = 0;
    int                           dy                 = 0;
    int                           last_activat_update= 0;
    int                           update_interval    = 25;
    int                           screen_width;
    int                           screen_height;
};

#endif // ASSETS_HPP
