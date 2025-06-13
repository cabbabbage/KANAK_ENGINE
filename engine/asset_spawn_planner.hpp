#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "spawn_info.hpp"
#include "asset_info.hpp"
#include "asset_library.hpp"
#include <SDL.h>

class AssetSpawnPlanner {
public:
    AssetSpawnPlanner(const nlohmann::json& json_data,
                      double area,
                      SDL_Renderer* renderer,
                      AssetLibrary& asset_library);

    SDL_Renderer* renderer_;
    double REPRESENTATIVE_SPAWN_AREA = 5000000.0;

    const std::vector<SpawnInfo>& get_spawn_queue() const;

private:
    void merge_assets();
    void build_spawn_queue(double area);
    nlohmann::json resolve_asset_from_tag(const nlohmann::json& tag_entry);

    nlohmann::json root_json_;
    std::vector<nlohmann::json> merged_assets_;
    std::vector<SpawnInfo> spawn_queue_;
    AssetLibrary* asset_library_ = nullptr;
};
