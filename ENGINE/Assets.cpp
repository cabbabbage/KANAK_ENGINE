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
    set_static_lights();
    activeManager.initialize(all, player, screen_center_x, screen_center_y);
    std::cout << "[Assets] Initialization complete. Total assets: "
              << all.size() << "\n";

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
{
    dx = 0;
    dy = 0;
    activeManager.updateClosest(player);

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
    }
}

bool Assets::check_collision(const Area& a, const Area& b) {
    return a.intersects(b);
}


void Assets::set_static_lights()
{
        std::cout << "Starting static light gen \n";
    // For each owner asset that defines light sources...
    for (auto& owner : all) {
        if (!owner.info) continue;

        const auto& infos = owner.info->lights;
        const auto& texs  = owner.light_textures;
        size_t cnt = std::min(infos.size(), texs.size());

        for (size_t i = 0; i < cnt; ++i) {
            const auto& li = infos[i];
            SDL_Texture* tex = texs[i];
            if (!tex || li.radius <= 0) continue;

            // Compute global position of this light
            int world_x = owner.pos_X + li.offset_x;
            int world_y = owner.pos_Y + li.offset_y;
            int lw = 0, lh = 0;
            SDL_QueryTexture(tex, nullptr, nullptr, &lw, &lh);
            
            owner.add_static_light_source(                        tex,
                        world_x,
                        world_y,
                        lw,
                        lh,
                        li.radius);


            // Distribute this light to any shaded asset within radius
            for (auto& target : all) {
                if (!target.info || !target.info->has_shading) 
                    continue;

                int dx = world_x - target.pos_X;
                int dy = world_y - target.pos_Y;
                if (dx*dx + dy*dy <= li.radius * li.radius) {
                    target.add_static_light_source(
                        tex,
                        world_x,
                        world_y,
                        lw,
                        lh,
                        li.radius
                    );
                }
            }
        }
    }
    std::cout << "[Assets] Static Light Gen Complete\n";
}