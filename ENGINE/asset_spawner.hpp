#pragma once

#include "Area.hpp"
#include "Asset.hpp"
#include "asset_info.hpp"
#include "asset_spawn_planner.hpp"
#include "check.hpp"
#include "spawn_logger.hpp"

#include <vector>
#include <string>
#include <memory>
#include <random>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <SDL.h>

class AssetSpawner {
public:
    using Point = std::pair<int, int>;

    // Constructor: only sets up persistent state
    AssetSpawner(SDL_Renderer* renderer,
                 int map_width,
                 int map_height,
                 AssetLibrary* asset_library);

    // Public method: processes a new JSON batch for a given area
    void spawn(const Area& spawn_area,
               const nlohmann::json& assets_json,
               bool batch,
               const std::string& map_dir,
               const std::string& room_dir);

    std::vector<std::unique_ptr<Asset>>&& extract_all_assets();
// In asset_spawner.hpp (add to public section)
    void removeAsset(Asset* asset);

private:
    void spawn_item_random(const SpawnInfo& item, const Area* area);
    void spawn_distributed_batch(const std::vector<SpawnInfo>& items, const Area* area);
    void spawn_item_distributed(const SpawnInfo& item, const Area* area);
    void spawn_item_exact(const SpawnInfo& item, const Area* area);
    void spawn_item_center(const SpawnInfo& item, const Area* area);
    void spawn_item_perimeter(const SpawnInfo& item, const Area* area);

    Point get_area_center(const Area& area) const;
    Point get_point_within_area(const Area& area);

    Asset* spawn_(const std::string& type,
                  const std::shared_ptr<AssetInfo>& info,
                  const Area& area,
                  int x, int y, int depth, Asset* parent);

    SDL_Renderer*                                     renderer_;
    int                                               map_width_;
    int                                               map_height_;
    AssetLibrary*                                     asset_library_;
    std::mt19937                                      rng_;
    Check                                             checker_;
    SpawnLogger                                       logger_;

    nlohmann::json                                    assets_json_;
    bool                                              batch_;
    std::string                                       map_dir_;
    std::string                                       room_dir_;
    std::vector<SpawnInfo>                            spawn_queue_;
    std::unordered_map<std::string, std::shared_ptr<AssetInfo>>
                                                      asset_info_library_;
    std::vector<std::unique_ptr<Asset>>               all_;
};
