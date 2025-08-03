// === File: assets.cpp ===
#include "assets.hpp"
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
      active_assets(activeManager.getActive()),
      closest_assets(activeManager.getClosest()),
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
    std::cout << "[Assets] Initialization complete. Total assets: "
              << all.size() << "\n";
    set_static_sources();
    std::cout << "[Assets] All static sources set.\n";


}

void Assets::update_direction_movement(int offset_x, int offset_y) {
    if (!player) return;

    int new_px = player->pos_X + offset_x;
    int new_py = player->pos_Y + offset_y;

    // Only test collisions if player has a passability area
    if (player->info && player->info->has_passability_area) {
        for (Asset* asset : closest_assets) {
            if (!asset || asset == player
                || !asset->info || asset->info->passable
                || !asset->info->has_passability_area)
                continue;

            Area obstacle = *asset->info->passability_area;
            obstacle.align(asset->pos_X, asset->pos_Y);

            if (obstacle.contains_point({new_px, new_py})) {
                return;
            }
        }
    }

    dx += offset_x;
    dy += offset_y;
}

void Assets::update(const std::unordered_set<SDL_Keycode>& keys,
                    int screen_center_x,
                    int screen_center_y)
{   set_player_light_render();
    dx = 0;
    dy = 0;


    const std::string current_animation = player->get_current_animation();

    bool up    = keys.count(SDLK_w);
    bool down  = keys.count(SDLK_s);
    bool left  = keys.count(SDLK_a);
    bool right = keys.count(SDLK_d);

    int move_x = (right ? 1 : 0) - (left ? 1 : 0);
    int move_y = (down  ? 1 : 0) - (up   ? 1 : 0);

    bool any_movement = (move_x != 0 || move_y != 0);
    bool diagonal     = (move_x != 0 && move_y != 0);

    if (any_movement) {
        float len   = std::sqrt(float(move_x*move_x + move_y*move_y));
        float speed = player->player_speed_mult / len;

        update_direction_movement(
            int(std::round(move_x * speed)),
            int(std::round(move_y * speed))
        );

        if (!diagonal) {
            std::string anim;
            if      (move_y < 0) anim = "backward";
            else if (move_y > 0) anim = "forward";
            else if (move_x < 0) anim = "left";
            else if (move_x > 0) anim = "right";

            if (!anim.empty() && anim != current_animation)
                player->change_animation(anim);
        }

        if (dx != 0 || dy != 0)
            player->set_position(player->pos_X + dx,
                                 player->pos_Y + dy);
    } else {
        if (current_animation != "default")
            player->change_animation("default");
    }

    // Interaction logic only guards collision block
    if (player && player->info && player->info->has_passability_area) {
        Area pa = *player->info->passability_area;
        pa.align(player->pos_X, player->pos_Y);
        auto [pMinX, pMinY, pMaxX, pMaxY] = pa.get_bounds();
        int px = (pMinX + pMaxX) / 2;
        int py = pMaxY;

        if (keys.count(SDLK_e)) {
            for (Asset* asset : closest_assets) {
                if (!asset || asset == player
                    || !asset->info || !asset->info->interaction_area)
                    continue;

                Area ia = *asset->info->interaction_area;
                ia.align(asset->pos_X, asset->pos_Y);
                auto [minx, miny, maxx, maxy] = ia.get_bounds();

                bool hit = (px >= minx && px <= maxx && py >= miny && py <= maxy)
                        || ia.contains_point({px, py});

                if (hit)
                    asset->change_animation("interaction");
            }
        }

    }

    // Always update animations after movement/interaction
    player->update();
    for (Asset* asset : active_assets) {
        if (asset && asset != player)
            asset->update();
    }

    if (dx != 0 || dy != 0) {

        if (++last_activat_update >= update_interval) {
            last_activat_update = 0;
            activeManager.updateVisibility(player,
                                           screen_center_x,
                                           screen_center_y);
        }
        else{
                activeManager.sortByZIndex();
        }
    }

}

bool Assets::check_collision(const Area& a, const Area& b) {
    return a.intersects(b);
}

void Assets::set_static_lights() {
    std::cout << "Starting static light gen\n";

    for (auto& owner : all) {
        if (!owner.info) continue;

        for (LightSource& light : owner.info->light_sources) {
            if (!light.texture || light.radius <= 0) continue;

            int world_x = owner.pos_X + light.offset_x;
            int world_y = owner.pos_Y + light.offset_y;

            // Distribute light to shaded assets within range
            auto targets = get_all_in_range(world_x, world_y, light.radius);
            for (Asset* target : targets) {
                if (!target || !target->info || !target->info->has_shading)
                    continue;

                target->add_static_light_source(&light, world_x, world_y);
            }
        }
    }

    std::cout << "[Assets] Static Light Gen Complete\n";
}

std::vector<Asset*> Assets::get_all_in_range(int cx, int cy, int radius) const {
    std::vector<Asset*> result;
    int r2 = radius * radius;

    for (const auto& asset : all) {
        if (!asset.info) continue;

        int dx = asset.pos_X - cx;
        int dy = asset.pos_Y - cy;

        if ((dx * dx + dy * dy) <= r2) {
            result.push_back(const_cast<Asset*>(&asset));
        }
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
            for (Asset* target : targets) {
                if (!target || !target->info || !target->info->has_shading)
                    continue;

                target->add_static_light_source(&light, lx, ly);
            }
        }
    }
}

void Assets::set_player_light_render() {
    if (!player || !player->info) return;

    std::unordered_set<Asset*> current_in_light;

    for (LightSource& light : player->info->light_sources) {
        if (light.orbit_radius > 0) continue;

        int lx = player->pos_X + light.offset_x;
        int ly = player->pos_Y + light.offset_y;

        auto in_range = get_all_in_range(lx, ly, light.radius);
        for (Asset* a : in_range) {
            if (a && a != player && a->pos_Y <= player->pos_Y) {
                current_in_light.insert(a);
            }
        }
    }

    for (Asset* a : prev_player_light_assets) {
        if (!current_in_light.count(a)) {
            a->set_render_player_light(false);
        }
    }

    for (Asset* a : current_in_light) {
        a->set_render_player_light(true);
    }

    prev_player_light_assets.swap(current_in_light);
}
