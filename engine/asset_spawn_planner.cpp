#include "asset_spawn_planner.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <random>
#include <fstream>

namespace fs = std::filesystem;

AssetSpawnPlanner::AssetSpawnPlanner(const nlohmann::json& json_data,
                                     double area,
                                     SDL_Renderer* renderer,
                                     AssetLibrary& asset_library)
    : renderer_(renderer), asset_library_(&asset_library)
{
    root_json_ = json_data;
    merge_assets();
    build_spawn_queue(area);
}

const std::vector<SpawnInfo>& AssetSpawnPlanner::get_spawn_queue() const {
    return spawn_queue_;
}

void AssetSpawnPlanner::merge_assets() {
    if (root_json_.contains("assets")) {
        for (const auto& asset : root_json_["assets"]) {
            merged_assets_.push_back(asset);
        }
    }

    if (root_json_.contains("assets_tag")) {
        for (const auto& tag_entry : root_json_["assets_tag"]) {
            nlohmann::json resolved = resolve_asset_from_tag(tag_entry);
            merged_assets_.push_back(resolved);
        }
    }
}

nlohmann::json AssetSpawnPlanner::resolve_asset_from_tag(const nlohmann::json& tag_entry) {
    static std::mt19937 rng(std::random_device{}());

    std::string tag = tag_entry.value("tag", "");
    std::string csv_path = "SRC/tag_map.csv";

    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open tag map: " + csv_path);
    }

    std::string line;
    std::vector<std::string> matching_assets;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> tokens;

        while (std::getline(ss, cell, ',')) {
            tokens.push_back(cell);
        }

        if (!tokens.empty() && tokens[0] == tag) {
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (!tokens[i].empty()) {
                    matching_assets.push_back(tokens[i]);
                }
            }
            break;
        }
    }

    file.close();

    if (matching_assets.empty()) {
        throw std::runtime_error("No assets found for tag: " + tag);
    }

    std::uniform_int_distribution<size_t> dist(0, matching_assets.size() - 1);
    std::string selected_name = matching_assets[dist(rng)];

    nlohmann::json result = tag_entry;
    result["name"] = selected_name;
    result.erase("tag");

    return result;
}

void AssetSpawnPlanner::build_spawn_queue(double area) {
    std::mt19937 rng(std::random_device{}());

    for (const auto& entry : merged_assets_) {
        std::string type = entry.value("name", "");
        if (type.empty()) continue;

        auto info = asset_library_->get(type);
        if (!info) continue;

        std::string pos = "random";
        int x = -1, y = -1;
        int spacing_min = 0, spacing_max = 0;

        if (entry.contains("position") && !entry["position"].is_null()) {
            pos = entry["position"].get<std::string>();
        }
        if (entry.contains("x")) x = entry.value("x", -1);
        if (entry.contains("y")) y = entry.value("y", -1);

        if (entry.contains("spacing") && entry["spacing"].is_object()) {
            spacing_min = entry["spacing"].value("min", 0);
            spacing_max = entry["spacing"].value("max", 0);
        }

        int min_num = entry.value("min_number", 1);
        int max_num = entry.value("max_number", min_num);
        int quantity = std::uniform_int_distribution<int>(min_num, max_num)(rng);
        quantity = static_cast<int>(std::round(quantity * (area / REPRESENTATIVE_SPAWN_AREA)));
        if (quantity < 1) quantity = 1;

        SpawnInfo info_obj;
        info_obj.name = type;
        info_obj.type = info->type;
        info_obj.spawn_position = pos;
        info_obj.quantity = quantity;
        info_obj.x_position = x;
        info_obj.y_position = y;
        info_obj.spacing_min = spacing_min;
        info_obj.spacing_max = spacing_max;
        info_obj.info = info;

        spawn_queue_.push_back(std::move(info_obj));
    }
}
