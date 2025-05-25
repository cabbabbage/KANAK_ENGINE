#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "asset.hpp"
#include "AssetInfo.hpp"
#include "Area.hpp"
#include <SDL.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <random>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <unordered_set>

class Assets {
public:
    std::vector<Asset> all;
    Asset* player = nullptr;

    Assets(const std::string& map_json, SDL_Renderer* renderer) {
        std::ifstream in(map_json);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open map json: " + map_json);
        }

        nlohmann::json root;
        in >> root;
        parse_perimeter(root);
        create_asset_info(root, renderer);
        generate_assets(root);

        sort_assets_by_distance();
    }

    void update(const std::unordered_set<SDL_Keycode>& keys) {
        int dx = 0, dy = 0;
        for (SDL_Keycode key : keys) {
            if (key == SDLK_w) dy -= 5;
            else if (key == SDLK_s) dy += 5;
            else if (key == SDLK_a) dx -= 5;
            else if (key == SDLK_d) dx += 5;
        }

        for (auto& asset : all) {
            asset.update();
        }

        if (player) {
            player->set_position(player->pos_X + dx, player->pos_Y + dy);
        }

        sort_assets_by_distance();
    }

private:
    using Point = std::pair<int, int>;
    Area map_area;
    std::unordered_map<std::string, AssetInfo*> asset_info_library;
    std::mt19937 rng{ std::random_device{}() };

    void parse_perimeter(const nlohmann::json& root) {
        std::vector<Point> perimeter;
        for (const auto& point : root["perimeter"]) {
            if (point.size() == 2) {
                perimeter.emplace_back(point[0], point[1]);
            }
        }
        map_area = Area(perimeter);
    }

    void create_asset_info(const nlohmann::json& root, SDL_Renderer* renderer) {
        for (const auto& entry : root["assets"]) {
            std::string type = entry["name"];
            if (asset_info_library.find(type) == asset_info_library.end()) {
                asset_info_library[type] = new AssetInfo(type, renderer);
            }
        }
    }

    void generate_assets(const nlohmann::json& root) {
        std::vector<std::pair<std::string, Asset>> staged_assets;

        for (const auto& entry : root["assets"]) {
            std::string type = entry["name"];
            int min_n = entry["min_number"];
            int max_n = entry["max_number"];
            std::uniform_int_distribution<int> count_dist(min_n, max_n);
            int spawn_count = count_dist(rng);

            AssetInfo* info = asset_info_library[type];

            for (int i = 0; i < spawn_count; ++i) {
                Point pos = get_random_position_within_perimeter();
                Asset asset(0, map_area); // z_offset = 0, spawn area = map_area
                asset.info = info;
                asset.set_position(pos.first, pos.second);
                staged_assets.emplace_back(type, std::move(asset));
            }
        }

        for (auto& [type, asset] : staged_assets) {
            int base_z = asset.pos_Y;
            int z_offset = asset.z_offset;
            asset.set_z_index(base_z + z_offset);

            if (type == "Default_Player") {
                player = &all.emplace_back(std::move(asset));
            } else {
                all.emplace_back(std::move(asset));
            }
        }
    }

    void sort_assets_by_distance() {
        if (!player) return;
        std::sort(all.begin(), all.end(), [this](const Asset& a, const Asset& b) {
            int z_a = a.pos_Y + a.z_offset;
            int z_b = b.pos_Y + b.z_offset;
            return z_a < z_b;
        });
    }

    Point get_random_position_within_perimeter() {
        if (map_area.points.size() != 4) return {0, 0};
        int min_x = map_area.points[0].first;
        int min_y = map_area.points[0].second;
        int max_x = map_area.points[2].first;
        int max_y = map_area.points[2].second;

        std::uniform_int_distribution<int> x_dist(min_x, max_x);
        std::uniform_int_distribution<int> y_dist(min_y, max_y);
        return { x_dist(rng), y_dist(rng) };
    }
};

#endif
