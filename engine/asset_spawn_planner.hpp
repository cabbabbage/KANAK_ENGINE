#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <SDL.h>
#include "asset_info.hpp"
#include "asset_library.hpp"

struct SpawnInfo {
    std::string name;
    std::string position;
    int quantity = 1;

    bool check_overlap = false;
    bool check_min_spacing = false;

    int grid_spacing = 100;
    int jitter = 0;
    int empty_grid_spaces = 0;

    int ep_x = -1;
    int ep_y = -1;

    int border_shift = 0;
    int sector_center = 0;
    int sector_range = 360;

    int perimeter_x_offset = 0;
    int perimeter_y_offset = 0;

    std::shared_ptr<AssetInfo> info;
};

struct BatchSpawnInfo {
    std::string name;
    int percent = 0;
};

class AssetSpawnPlanner {
public:
    AssetSpawnPlanner(const nlohmann::json& json_data,
                      double area,
                      AssetLibrary& asset_library);

    const std::vector<SpawnInfo>& get_spawn_queue() const;
    const std::vector<BatchSpawnInfo>& get_batch_spawn_assets() const;

    int get_batch_grid_spacing() const { return batch_grid_spacing_; }
    int get_batch_jitter() const { return batch_jitter_; }

private:
    void parse_asset_spawns(double area);
    void parse_batch_assets();
    nlohmann::json resolve_asset_from_tag(const nlohmann::json& tag_entry);

    nlohmann::json root_json_;
    AssetLibrary* asset_library_ = nullptr;

    std::vector<SpawnInfo> spawn_queue_;
    std::vector<BatchSpawnInfo> batch_spawn_assets_;

    int batch_grid_spacing_ = 100;
    int batch_jitter_ = 0;
    const double REPRESENTATIVE_SPAWN_AREA = 5000000.0;
};
