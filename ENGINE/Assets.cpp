#include "assets.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>

Assets::Assets(std::vector<Asset>&& loaded,
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
        Asset a = std::move(loaded.back());
        loaded.pop_back();

        if (!a.info) {
            std::cerr << "[Assets] Skipping asset: info is null\n";
            continue;
        }
        auto it = a.info->animations.find("default");
        if (it == a.info->animations.end() || it->second.frames.empty()) {
            std::cerr << "[Assets] Skipping asset '" << a.info->name << "': missing or empty default animation\n";
            continue;
        }
        all.push_back(std::move(a));
    }

    for (auto& asset : all) {
        if (asset.info->type == "Player") {
            player = &asset;
            std::cout << "[Assets] Found player asset: " << player->info->name << "\n";
            break;
        }
    }

    if (player && player->info && player->info->has_passability_area) {
        const Area& player_base = *player->info->passability_area;
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
    update_closest();

    dx = 0;
    dy = 0;
    std::string new_animation = "";
    std::string current_animation = player->get_current_animation();

    // determine movement intent
    int move_x = 0;
    int move_y = 0;
    bool up    = keys.count(SDLK_w);
    bool down  = keys.count(SDLK_s);
    bool left  = keys.count(SDLK_a);
    bool right = keys.count(SDLK_d);

    if (up)    move_y -= 1;
    if (down)  move_y += 1;
    if (left)  move_x -= 1;
    if (right) move_x += 1;

    if (move_x != 0 || move_y != 0) {
        // normalize diagonal speed
        float len = std::sqrt(static_cast<float>(move_x * move_x + move_y * move_y));
        float speed = player->player_speed_mult / len;
        update_direction_movement(
            static_cast<int>(std::round(move_x * speed)),
            static_cast<int>(std::round(move_y * speed))
        );

        // animation selection
        if (move_y < 0 && !(left ^ right) && current_animation != "backward")
            new_animation = "backward";
        else if (move_y > 0 && !(left ^ right) && current_animation != "forward")
            new_animation = "forward";
        else if (move_x < 0 && !right && current_animation != "left")
            new_animation = "left";
        else if (move_x > 0 && !left && current_animation != "right")
            new_animation = "right";

        if (!new_animation.empty())
            player->change_animation(new_animation);

        if (dx != 0 || dy != 0)
            player->set_position(player->pos_X + dx, player->pos_Y + dy);
    } else {
        if (current_animation != "default")
            player->change_animation("default");
    }

    Area area1 = player->get_global_passability_area();
    if (keys.count(SDLK_e)) {
        std::cout << "Checking interaction \n";
        for (Asset* asset : closest_assets) {
            if (!asset || !asset->info->has_interaction_area) continue;
            Area area2 = asset->get_global_interaction_area();
            if (check_collision(area1, area2)) {
                asset->change_animation("interaction");
                std::cout << "interaction init for asset: " + asset->info->name << "\n";
            }
        }
    }

    adjusted_keys.erase(SDLK_w);
    adjusted_keys.erase(SDLK_a);
    adjusted_keys.erase(SDLK_s);
    adjusted_keys.erase(SDLK_d);

    //player->update(adjusted_keys);
    for (auto* asset : active_assets) {
        if (!asset) continue;
        asset->update();
    }

    if (!(dx == 0 && dy == 0)) {
        resort_active_asset(player);
        if (last_activat_update >= update_interval) {
            last_activat_update = 0;
            sort_assets_by_distance_to_screen_center(screen_center_x, screen_center_y);
        }
        last_activat_update++;
    }
}



void Assets::update_closest() {
    closest_assets.clear();
    if (!player) return;

    std::vector<std::pair<float, Asset*>> distances;
    for (auto* a : active_assets) {
        if (!a || a == player) continue;
        int dx = a->pos_X - player->pos_X;
        int dy = a->pos_Y - player->pos_Y;
        float dist_sq = dx * dx + dy * dy;
        distances.emplace_back(dist_sq, a);
    }

    std::sort(distances.begin(), distances.end());
    for (size_t i = 0; i < std::min<size_t>(3, distances.size()); ++i) {
        closest_assets.push_back(distances[i].second);
    }
}

void Assets::update_direction_movement(int offset_x, int offset_y) {
    if (!player || !player->info || !player->info->has_passability_area) return;

    Area player_area = player->get_global_passability_area();
    player_area.apply_offset(offset_x, offset_y);

    for (Asset* asset : closest_assets) {
        if (!asset || asset == player || !asset->info || asset->info->passable || !asset->info->has_passability_area) continue;

        Area other_area = asset->get_global_passability_area();
        if (check_collision(player_area, other_area)) {
            return;
        }
    }

    dx += offset_x;
    dy += offset_y;
}

bool Assets::check_collision(const Area& a, const Area& b) {
    return a.intersects(b);
}




void Assets::activate(Asset* asset) {
    if (!asset || asset->active) return;
    asset->active = true;
    add_active(asset);


    for (auto& child : asset->children) {
        activate(&child);
    }
}

void Assets::sort_assets_by_distance_to_screen_center(int cx, int cy) {
    const float active_width = 2.0f * static_cast<float>(screen_width);
    const float active_height = 2.0f * static_cast<float>(screen_height);

    for (auto& asset : all) {
        float dx = static_cast<float>(asset.pos_X - cx);
        float dy = static_cast<float>(asset.pos_Y - cy);
        bool now_active = std::abs(dx) < active_width && std::abs(dy) < active_height;

        if (now_active && !asset.active) {
            activate(&asset);
        } else if (!now_active && asset.active) {
            asset.active = false;
            remove_active(&asset);
            if (asset.info) {
            }
        }
    }

    visible_count = static_cast<int>(active_assets.size());
}


void Assets::resort_active_asset(Asset* a) {
    if (!a || !a->info) return;

    auto it = std::find(active_assets.begin(), active_assets.end(), a);
    if (it == active_assets.end()) return;

    active_assets.erase(it);

    int target_z = a->z_index;
    auto insert_it = std::lower_bound(
        active_assets.begin(), active_assets.end(), target_z,
        [](const Asset* lhs, int z) { return lhs->z_index < z; }
    );

    active_assets.insert(insert_it, a);

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


}

void Assets::remove_active(Asset* a) {
    auto it = std::find(active_assets.begin(), active_assets.end(), a);
    if (it != active_assets.end()) {
        active_assets.erase(it);
    }
}
