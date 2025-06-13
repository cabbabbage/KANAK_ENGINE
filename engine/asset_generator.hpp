// === File: asset_generator.hpp ===
#pragma once

#include "Area.hpp"
#include "Asset.hpp"
#include "asset_info.hpp"
#include "asset_spawn_planner.hpp"
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <nlohmann/json.hpp>
#include <SDL.h>




class AssetGenerator {
public:
    using Point = std::pair<int, int>;
// === In asset_generator.hpp ===




AssetGenerator(const Area& spawn_area,
               const nlohmann::json& assets_json,
               SDL_Renderer* renderer,
               int map_width,
               int map_height,
               AssetLibrary* asset_library,
               bool batch = false);



    std::vector<std::unique_ptr<Asset>>&& extract_all_assets();
    void spawn_all_children();

private:
// === In asset_generator.hpp ===
    std::unordered_map<std::string, std::shared_ptr<AssetInfo>> asset_info_library_;
    AssetLibrary* asset_library_ = nullptr;

    Area spawn_area_;
    SDL_Renderer* renderer_;
    nlohmann::json assets_json_;
    int map_width_;
    int map_height_;
    std::string map_path;

    std::vector<std::unique_ptr<Asset>> all_;
    std::vector<SpawnInfo> spawn_queue_;

    std::mt19937 rng_;
    bool batch = false;
    void spawn_item_random(const SpawnInfo& item, const Area* area);
    void spawn_item_exact(const SpawnInfo& item, const Area* area);
    void spawn_item_center(const SpawnInfo& item, const Area* area);
    void spawn_item_perimeter(const SpawnInfo& item, const Area* area);
    void spawn_item_distributed(const SpawnInfo& item, const Area* area);
    void spawn_distributed_batch(const std::vector<SpawnInfo>& items, const Area* area);

    bool check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                               int test_pos_X,
                               int test_pos_Y,
                               const std::vector<Asset*>& closest_assets) const;

    bool check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                 const Point& pos) const;

    Point get_area_center(const Area& area) const;
    Point get_point_within_area(const Area& area);

    Asset* spawn(const std::string& type,
                 const std::shared_ptr<AssetInfo>& info,
                 const Area& area,
                 int x,
                 int y,
                 int depth,
                 Asset* parent);

    std::vector<Asset*> get_closest_assets(int x, int y, int max_count) const;
};
