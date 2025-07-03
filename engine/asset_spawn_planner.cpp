#include "asset_spawn_planner.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

AssetSpawnPlanner::AssetSpawnPlanner(const nlohmann::json& json_data,
                                     double area,
                                     SDL_Renderer* renderer,
                                     AssetLibrary& asset_library)
    : renderer_(renderer), asset_library_(&asset_library) {
    root_json_ = json_data;
    parse_asset_spawns(area);
    parse_batch_assets();
}

const std::vector<SpawnInfo>& AssetSpawnPlanner::get_spawn_queue() const {
    return spawn_queue_;
}

const std::vector<BatchSpawnInfo>& AssetSpawnPlanner::get_batch_spawn_assets() const {
    return batch_spawn_assets_;
}

void AssetSpawnPlanner::parse_asset_spawns(double area) {
    std::mt19937 rng(std::random_device{}());

    if (!root_json_.contains("assets")) return;

    for (const auto& entry : root_json_["assets"]) {
        nlohmann::json asset = entry;

        if (asset.value("tag", false)) {
            asset = resolve_asset_from_tag(asset);
        }

        std::string name = asset.value("name", "");
        if (name.empty()) continue;

        auto info = asset_library_->get(name);
        if (!info) {
            std::cerr << "[AssetSpawnPlanner] WARNING: Asset '" << name << "' not found in library.\n";
            continue;
        }

        int min_num = asset.value("min_number", 1);
        int max_num = asset.value("max_number", min_num);
        int quantity = std::uniform_int_distribution<int>(min_num, max_num)(rng);

        std::string position = asset.value("position", "Random");
        if (!(min_num == 1 && max_num == 1 && (position == "Center" || position == "center"))) {
            quantity = static_cast<int>(std::round(quantity * (area / REPRESENTATIVE_SPAWN_AREA)));
            if (quantity < 1) quantity = 1;
        }

        SpawnInfo s;
        s.name = name;
        s.position = position;
        s.quantity = quantity;
        s.check_overlap = asset.value("check_overlap", false);
        s.check_min_spacing = asset.value("check_min_spacing", false);

        auto get_val = [&](const std::string& kmin, const std::string& kmax, int def = 0) -> int {
            int vmin = asset.value(kmin, def);
            int vmax = asset.value(kmax, def);
            return (vmin + vmax) / 2;
        };

        s.grid_spacing = get_val("grid_spacing_min", "grid_spacing_max");
        s.jitter = get_val("jitter_min", "jitter_max");
        s.empty_grid_spaces = get_val("empty_grid_spaces_min", "empty_grid_spaces_max");
        s.ep_x = get_val("ep_x_min", "ep_x_max", -1);
        s.ep_y = get_val("ep_y_min", "ep_y_max", -1);
        s.border_shift = get_val("border_shift_min", "border_shift_max");
        s.sector_center = get_val("sector_center_min", "sector_center_max");
        s.sector_range = get_val("sector_range_min", "sector_range_max");
        s.perimeter_x_offset = get_val("perimeter_x_offset_min", "perimeter_x_offset_max");
        s.perimeter_y_offset = get_val("perimeter_y_offset_min", "perimeter_y_offset_max");

        s.info = info;
        spawn_queue_.push_back(s);
    }
}

void AssetSpawnPlanner::parse_batch_assets() {
    if (!root_json_.contains("batch_assets")) return;

    const auto& batch_data = root_json_["batch_assets"];
    if (!batch_data.value("has_batch_assets", false)) return;

    batch_grid_spacing_ = (batch_data.value("grid_spacing_min", 100) + batch_data.value("grid_spacing_max", 100)) / 2;
    batch_jitter_ = (batch_data.value("jitter_min", 0) + batch_data.value("jitter_max", 0)) / 2;

    for (const auto& entry : batch_data.value("batch_assets", std::vector<nlohmann::json>{})) {
        nlohmann::json asset = entry;

        if (asset.value("tag", false)) {
            asset = resolve_asset_from_tag(asset);
        }

        std::string name = asset.value("name", "");
        if (name.empty()) continue;

        BatchSpawnInfo b;
        b.name = name;
        b.percent = asset.value("percent", 0);
        batch_spawn_assets_.push_back(b);
    }
}

nlohmann::json AssetSpawnPlanner::resolve_asset_from_tag(const nlohmann::json& tag_entry) {
    static std::mt19937 rng(std::random_device{}());
    std::string tag = tag_entry.value("tag", "");

    std::vector<std::string> matches;

    for (const auto& [name, info] : asset_library_->all()) {
        if (info && info->has_tag(tag)) {
            matches.push_back(name);
        }
    }

    if (matches.empty()) {
        throw std::runtime_error("No assets found for tag: " + tag);
    }

    std::uniform_int_distribution<size_t> dist(0, matches.size() - 1);
    std::string selected = matches[dist(rng)];

    nlohmann::json result = tag_entry;
    result["name"] = selected;
    result.erase("tag");
    return result;
}
