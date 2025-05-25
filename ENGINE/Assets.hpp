// assets.hpp

#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <random>
#include <SDL.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "asset.hpp"
#include "area.hpp"
#include "asset_info.hpp"

class Assets {
public:
    using Point = std::pair<int, int>;

    std::vector<Asset> all;
    std::unordered_map<std::string, AssetInfo*> asset_info_library;
    Area map_area;
    Asset* player = nullptr;

    Assets(const std::string& map_json, SDL_Renderer* renderer);

    void update(const std::unordered_set<SDL_Keycode>& keys);
    void sort_assets_by_distance();

private:
    std::mt19937 rng{ std::random_device{}() };
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* overlay_center_texture;

    void parse_perimeter(const nlohmann::json& root);
    void create_asset_info(const nlohmann::json& root, SDL_Renderer* renderer);
    void generate_assets(const nlohmann::json& root);
    void spawn_child_assets(AssetInfo* parent_info, const Asset& parent);
    void place_child_assets(); // Deprecated
    Point get_random_position_within_perimeter();
    Point get_random_position_within_area(const Area& area);
};

bool areas_overlap(const Area& a, const Area& b);

#endif
