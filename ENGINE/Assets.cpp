// === File: assets.cpp ===
#include "assets.hpp"
#include "controls_manager.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

Assets::Assets(std::vector<Asset>&& loaded,
               Asset* player_ptr,
               int screen_width,
               int screen_height,
               int screen_center_x,
               int screen_center_y)
    : player(nullptr),
      activeManager(screen_width, screen_height),
      screen_width(screen_width),
      screen_height(screen_height),
      dx(0),
      dy(0),
      last_activat_update(0),
      update_interval(20)
{
    std::cout << "[Assets] Initializing Assets manager...\n";

    // move loaded into all
    while (!loaded.empty()) {
        Asset a = std::move(loaded.back());
        loaded.pop_back();
        if (!a.info) {
            std::cerr << "[Assets] Skipping asset: info is null\n";
            continue;
        }
        auto it = a.info->animations.find("default");
        if (it == a.info->animations.end() || it->second.frames.empty()) {
            std::cerr << "[Assets] Skipping asset '" << a.info->name
                      << "': missing or empty default animation\n";
            continue;
        }
        all.push_back(std::move(a));
    }

    // find player asset
    for (auto& asset : all) {
        if (asset.info->type == "Player") {
            player = &asset;
            std::cout << "[Assets] Found player asset: "
                      << player->info->name << "\n";
            break;
        }
    }

    // initialize activeAssetsManager
    activeManager.initialize(all, player, screen_center_x, screen_center_y);

    // now capture active_assets and closest_assets
    active_assets  = activeManager.getActive();
    closest_assets = activeManager.getClosest();
    set_shading_groups();
    std::cout << "[Assets] Initialization complete. Total assets: "
              << all.size() << "\n";

    try {
        set_static_sources();
    } catch (const std::length_error& e) {
        std::cerr << "[Assets] light-gen failed: " << e.what() << "\n";
    }
    std::cout << "[Assets] All static sources set.\n";
}

void Assets::update(const std::unordered_set<SDL_Keycode>& keys,
                    int screen_center_x,
                    int screen_center_y)
{
    set_player_light_render();
    dx = 0; dy = 0;

    // handle movement & interaction
    ControlsManager controls(player, closest_assets);
    controls.update(keys);
    dx = controls.get_dx();
    dy = controls.get_dy();

    // update manager visibility & closest lists
    activeManager.updateDynamicChunks();
    activeManager.updateVisibility(player, screen_center_x, screen_center_y);
    activeManager.updateClosest(player, /*max_count=*/3);

    // refresh our local copies so spawn/despawn actually changes them
    active_assets  = activeManager.getActive();
    closest_assets = activeManager.getClosest();

    // then update animations
    player->update();
    for (Asset* a : active_assets) {
        if (a && a != player)
            a->update();
    }

    // resort if we moved
    if (dx != 0 || dy != 0) {
        activeManager.sortByZIndex();  // ensure z-order
    }
}



std::vector<Asset*> Assets::get_all_in_range(int cx, int cy, int radius) const {
    std::vector<Asset*> result;
    int r2 = radius * radius;
    for (const auto& asset : all) {
        if (!asset.info) continue;
        int ddx = asset.pos_X - cx;
        int ddy = asset.pos_Y - cy;
        if (ddx*ddx + ddy*ddy <= r2)
            result.push_back(const_cast<Asset*>(&asset));
    }
    return result;
}

void Assets::set_static_sources() {
    for (auto& owner : all) {
        if (!owner.info) continue;
        for (LightSource& light : owner.info->light_sources) {
            int lx = owner.pos_X + light.offset_x;
            int ly = owner.pos_Y + light.offset_y;
            auto targets = get_all_in_range(lx, ly, light.radius);
            for (Asset* t : targets) {
                if (!t || !t->info || !t->info->has_shading) continue;
                t->add_static_light_source(&light, lx, ly);
            }
        }
    }
}
void Assets::set_player_light_render() {
    if (!player || !player->info) return;

    // First, turn off player light for all active assets
    for (Asset* a : active_assets) {
        if (a && a != player) {
            a->set_render_player_light(false);
        }
    }

    // Then, compute which assets should be lit this frame and enable them
    for (LightSource& light : player->info->light_sources) {
        int lx = player->pos_X + light.offset_x;
        int ly = player->pos_Y + light.offset_y;

        for (Asset* a : get_all_in_range(lx, ly, light.radius)) {
            if (a && a != player) {
                a->set_render_player_light(true);
            }
        }
    }
}




void Assets::set_shading_groups() {
    int current_group = 1;
    for (auto& a : all) {
        if (!a.info || !a.has_shading) continue;
        if (a.is_shading_group_set()) continue;
        a.set_shading_group(current_group);
        current_group++;
        if (current_group > num_groups_)
            current_group = 1;
    }

}