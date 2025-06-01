#include "assets.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>

Assets::Assets(std::vector<std::unique_ptr<Asset>>&& loaded,
               Asset* player_ptr,
               int screen_width,
               int screen_height,
               int screen_center_x,
               int screen_center_y)
    : player(nullptr),
      screen_width(screen_width),
      screen_height(screen_height),
      visible_count(0)
{
    std::cout << "[Assets] Initializing Assets manager...\n";

    while (!loaded.empty()) {
        std::unique_ptr<Asset> a = std::move(loaded.back());
        loaded.pop_back();

        if (!a || !a->info) {
            std::cerr << "[Assets] Skipping asset: info is null\n";
            continue;
        }
        auto it = a->info->animations.find("default");
        if (it == a->info->animations.end() || it->second.frames.empty()) {
            std::cerr << "[Assets] Skipping asset '" << a->info->name << "': missing or empty default animation\n";
            continue;
        }
        std::cout << "[Assets] Accepted asset: " << a->info->name << "\n";
        all.push_back(std::move(a));
    }


    for (auto& asset : all) {
        if (asset->info->type == "Player") {
            player = asset.get();
            std::cout << "[Assets] Found player asset: " << player->info->name << "\n";
            break;
        }
    }

    sort_assets_by_distance_to_screen_center(screen_center_x, screen_center_y);

    if (player && std::find(active_assets.begin(), active_assets.end(), player) == active_assets.end()) {
        std::cout << "[Assets] Activating player asset...\n";
        activate(player);
    }

    std::cout << "[Assets] Initialization complete. Total assets: " << all.size() << "\n";
}


void Assets::update(const std::unordered_set<SDL_Keycode>& keys, int screen_center_x, int screen_center_y) {
    std::unordered_set<SDL_Keycode> adjusted_keys = keys;

    if (player && player->info && player->info->has_passability_area) {
        Area player_base = player->info->passability_area;
        Asset* blocking = nullptr;
        Area blocking_area;
        bool collision_found = false;

        float closest_dist_sq = std::numeric_limits<float>::max();
        for (Asset* asset : active_assets) {
            if (!asset || asset == player || !asset->info || asset->info->is_passable || !asset->info->has_passability_area) continue;

            Area other_area = asset->info->passability_area;
            other_area.apply_offset(asset->pos_X, asset->pos_Y);

            Area test = player_base;
            test.apply_offset(player->pos_X, player->pos_Y);
            if (test.intersects(other_area)) {
                int dx = asset->pos_X - player->pos_X;
                int dy = asset->pos_Y - player->pos_Y;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq < closest_dist_sq) {
                    blocking = asset;
                    blocking_area = other_area;
                    closest_dist_sq = dist_sq;
                    collision_found = true;
                }
            }
        }

        if (collision_found) {
            std::cout << "[Assets] Collision detected with: " << (blocking && blocking->info ? blocking->info->name : "Unknown") << "\n";
            std::vector<std::pair<SDL_Keycode, std::pair<int, int>>> directions = {
                {SDLK_w, {0, -5}}, {SDLK_s, {0, 5}},
                {SDLK_a, {-5, 0}}, {SDLK_d, {5, 0}},
            };

            for (const auto& [key, offset] : directions) {
                if (!adjusted_keys.count(key)) continue;

                int test_x = player->pos_X + offset.first;
                int test_y = player->pos_Y + offset.second;

                Area moved = player_base;
                moved.apply_offset(test_x, test_y);

                if (moved.intersects(blocking_area)) {
                    adjusted_keys.erase(key);
                    std::cout << "[Assets] Erasing blocked movement key: " << SDL_GetKeyName(key) << "\n";
                }
            }
        }
    }

    for (auto* asset : active_assets) {
        if (!asset) continue;
        asset->update(adjusted_keys);
    }

    if (!adjusted_keys.empty()) {
        sort_assets_by_distance_to_screen_center(screen_center_x, screen_center_y);
        if (player) resort_active_asset(player);
    }
}

void Assets::activate(Asset* asset) {
    if (!asset || asset->active) return;
    asset->active = true;
    add_active(asset);
    if (asset->info) {
        std::cout << "[Activate] " << asset->info->name << "\n";
    }

    for (auto& child : asset->children) {
        activate(&child);
    }
}

void Assets::sort_assets_by_distance_to_screen_center(int cx, int cy) {
    const float active_width = 1.2f * static_cast<float>(screen_width);
    const float active_height = 1.2f * static_cast<float>(screen_height);

    for (auto& asset : all) {
        float dx = static_cast<float>(asset->pos_X - cx);
        float dy = static_cast<float>(asset->pos_Y - cy);
        bool now_active = std::abs(dx) < active_width && std::abs(dy) < active_height;

        if (now_active && !asset->active) {
            std::cout << "[Assets] Activating asset (in range): " << (asset->info ? asset->info->name : "Unknown") << "\n";
            activate(asset.get());
        } else if (!now_active && asset->active) {
            asset->active = false;
            remove_active(asset.get());
            if (asset->info) {
                std::cout << "[Deactivate] " << asset->info->name << "\n";
            }
        }
    }

    visible_count = static_cast<int>(active_assets.size());
}

void Assets::resort_active_asset(Asset* a) {
    if (!a || !a->info) return;

    auto it = std::find(active_assets.begin(), active_assets.end(), a);
    if (it == active_assets.end()) return;

    int target_z = a->z_index;

    while (it != active_assets.begin()) {
        auto prev = std::prev(it);
        int prev_z = (*prev)->z_index;
        if (prev_z <= target_z) break;
        std::iter_swap(it, prev);
        --it;
    }

    while (std::next(it) != active_assets.end()) {
        auto next = std::next(it);
        int next_z = (*next)->z_index;
        if (target_z <= next_z) break;
        std::iter_swap(it, next);
        ++it;
    }

    std::cout << "[Assets] Resorted active asset: " << a->info->name << " to z=" << a->z_index << "\n";
}

void Assets::add_active(Asset* a) {
    if (!a || !a->info) return;

    auto mid = active_assets.begin() + (active_assets.size() / 2);
    auto it = active_assets.insert(mid, a);

    int target_z = a->z_index;

    while (it != active_assets.begin()) {
        auto prev = std::prev(it);
        int prev_z = (*prev)->z_index;
        if (prev_z <= target_z) break;
        std::iter_swap(it, prev);
        --it;
    }

    while (std::next(it) != active_assets.end()) {
        auto next = std::next(it);
        int next_z = (*next)->z_index;
        if (target_z <= next_z) break;
        std::iter_swap(it, next);
        ++it;
    }

    std::cout << "[Assets] Added active asset: " << a->info->name << " z=" << a->z_index << "\n";
}

void Assets::remove_active(Asset* a) {
    auto it = std::find(active_assets.begin(), active_assets.end(), a);
    if (it != active_assets.end()) {
        std::cout << "[Assets] Removing active asset: " << (a && a->info ? a->info->name : "Unknown") << "\n";
        active_assets.erase(it);
    }
}
