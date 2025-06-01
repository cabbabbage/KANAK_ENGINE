// asset_generator.hpp
#pragma once

#include "asset.hpp"
#include "asset_info.hpp"
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <random>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetGenerator {
public:
    using Point = std::pair<int, int>;

    AssetGenerator(const Area& spawn_area,
                   const nlohmann::json& assets_json,
                   SDL_Renderer* renderer,
                   bool invert_area = false,
                   int map_width = 0,
                   int map_height = 0);

    void generate_all();
    std::vector<std::unique_ptr<Asset>>&& extract_all_assets();

private:
    Area spawn_area_;
    SDL_Renderer* renderer_;
    nlohmann::json assets_json_;
    bool invert_area_;
    int map_width_;
    int map_height_;

    std::unordered_map<std::string, std::shared_ptr<AssetInfo>> asset_info_library_;
    std::vector<std::unique_ptr<Asset>> all_;
    std::mt19937 rng_;

    bool generate_asset_recursive(const std::string& type,
                                  const Area& valid_area,
                                  int depth,
                                  Asset* parent,
                                  std::vector<Asset*>& siblings,
                                  bool terminate_with_parent = false);

    Point get_random_point(const Area& area);
    Point get_point_within_area(const Area& area);
    Point get_point_outside_area(const Area& area);
};
