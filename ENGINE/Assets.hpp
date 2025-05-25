#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "Asset.hpp"
#include "assetInfo.hpp"
#include <SDL.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <nlohmann/json.hpp>

class Assets {
public:
    std::vector<Asset> all;
    Asset* player = nullptr;

    Assets(const std::string& map_json, SDL_Renderer* renderer);
    void update(const std::unordered_set<SDL_Keycode>& keys);

private:
    using Point = std::pair<int, int>;
    std::vector<Point> perimeter;
    std::unordered_map<std::string, AssetInfo*> asset_info_library;
    std::mt19937 rng{ std::random_device{}() };

    void parse_perimeter(const nlohmann::json& root);
    void create_asset_info(const nlohmann::json& root, SDL_Renderer* renderer);
    void generate_assets();
    Point get_random_position_within_perimeter();
};

#endif
