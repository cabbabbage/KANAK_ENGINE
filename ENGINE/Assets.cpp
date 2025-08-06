// === File: assets.cpp ===
#include "assets.hpp"
#include "controls_manager.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <functional>

namespace {
    void set_shading_group_recursive(Asset& asset, int group, int num_groups) {
        asset.set_shading_group(group);
        for (auto& child : asset.children) {
            set_shading_group_recursive(child, group, num_groups);
        }
    }

    void collect_assets_in_range(const Asset* asset, int cx, int cy, int r2, std::vector<Asset*>& result) {
        int ddx = asset->pos_X - cx;
        int ddy = asset->pos_Y - cy;
        if (ddx*ddx + ddy*ddy <= r2) {
            result.push_back(const_cast<Asset*>(asset));
        }
        for (const auto& child : asset->children) {
            collect_assets_in_range(&child, cx, cy, r2, result);
        }
    }
}

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

    for (auto& asset : all) {
        if (asset.info->type == "Player") {
            player = &asset;
            std::cout << "[Assets] Found player asset: "
                      << player->info->name << "\n";
            break;
        }
    }

    activeManager.initialize(all, player, screen_center_x, screen_center_y);
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

    ControlsManager controls(player, closest_assets);
    controls.update(keys);
    dx = controls.get_dx();
    dy = controls.get_dy();

    activeManager.updateDynamicChunks();
    activeManager.updateVisibility(player, screen_center_x, screen_center_y);
    activeManager.updateClosest(player, /*max_count=*/3);

    active_assets  = activeManager.getActive();
    closest_assets = activeManager.getClosest();

    player->update();
    for (Asset* a : active_assets) {
        if (a && a != player)
            a->update();
    }

    if (dx != 0 || dy != 0) {
        activeManager.sortByZIndex();
    }
}

std::vector<Asset*> Assets::get_all_in_range(int cx, int cy, int radius) const {
    std::vector<Asset*> result;
    int r2 = radius * radius;
    for (const auto& asset : all) {
        if (!asset.info) continue;
        collect_assets_in_range(&asset, cx, cy, r2, result);
    }
    return result;
}

void Assets::set_static_sources() {
    // recurse through all assets and their children to collect lights
    std::function<void(Asset&)> recurse = [&](Asset& owner) {
        if (owner.info) {
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
        for (auto& child : owner.children) {
            recurse(child);
        }
    };

    for (auto& owner : all) {
        recurse(owner);
    }
}

void Assets::set_player_light_render() {
    if (!player || !player->info) return;

    for (Asset* a : active_assets) {
        if (a && a != player) {
            a->set_render_player_light(false);
        }
    }

    for (LightSource& light : player->info->light_sources) {
        int lx = player->pos_X + light.offset_x;
        int ly = player->pos_Y + light.offset_y;
        auto targets = get_all_in_range(lx, ly, light.radius);
        for (Asset* a : targets) {
            if (a && a != player) {
                a->set_render_player_light(true);
            }
        }
    }
}

void Assets::set_shading_groups() {
    int current_group = 1;
    for (auto& a : all) {
        if (!a.info) continue;
        set_shading_group_recursive(a, current_group, num_groups_);
        current_group++;
        if (current_group > num_groups_)
            current_group = 1;
    }
}
