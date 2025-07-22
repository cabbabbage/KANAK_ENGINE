#include "asset_spawner.hpp"
#include "asset_spawn_planner.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <nlohmann/json.hpp>  // Ensure this is included at the top of the file

namespace fs = std::filesystem;

AssetSpawner::AssetSpawner(AssetLibrary* asset_library,
                           std::vector<Area> exclusion_zones)
    : asset_library_(asset_library),
      exclusion_zones(std::move(exclusion_zones)),
      rng_(std::random_device{}()),
      checker_(false),
      logger_("", "") {}

void AssetSpawner::spawn(Room& room) {

    const Area& spawn_area = *room.room_area;
    const std::string& map_dir = room.map_path;
    const std::string& room_dir = room.room_directory;

    if (!room.planner) {
        std::cerr << "[AssetSpawner] Room planner is null — skipping room: " << room.room_name << "\n";
        return;
    }

    const auto& planner = room.planner;

    logger_ = SpawnLogger(map_dir, room_dir);

    spawn_queue_ = planner->get_spawn_queue();
    auto batch_assets = planner->get_batch_spawn_assets();
    int batch_spacing = planner->get_batch_grid_spacing();
    int batch_jitter  = planner->get_batch_jitter();

    asset_info_library_ = asset_library_->all();

    std::vector<std::unique_ptr<Asset>> generated;

    for (auto& queue_item : spawn_queue_) {
        logger_.start_timer();
        if (!queue_item.info) {
            continue;
        }

                 // << " at position type: " << queue_item.position << "\n";

        const std::string& pos = queue_item.position;
        if (pos == "Exact Position") {
            spawn_item_exact(queue_item, &spawn_area);
        }
        else if (pos == "Center") {
            spawn_item_center(queue_item, &spawn_area);
        }
        else if (pos == "Perimeter") {
            spawn_item_perimeter(queue_item, &spawn_area);
        }
        else if (pos == "Distributed") {
            spawn_item_distributed(queue_item, &spawn_area);
        }
        else {
            spawn_item_random(queue_item, &spawn_area);
        }
    }

    if (!batch_assets.empty()) {
        spawn_distributed_batch(batch_assets, &spawn_area, batch_spacing, batch_jitter);
    }


    // Transfer created assets to the room
    room.add_room_assets(std::move(all_));
}




std::vector<std::unique_ptr<Asset>> AssetSpawner::spawn_boundary_from_file(const std::string& json_path, const Area& spawn_area) {

    std::ifstream file(json_path);
    if (!file.is_open()) {
        std::cerr << "[BoundarySpawner] Failed to open file: " << json_path << "\n";
        return {};
    }

    nlohmann::json boundary_json;
    file >> boundary_json;
    std::vector<nlohmann::json> json_sources;
    json_sources.push_back(boundary_json);
    AssetSpawnPlanner planner(json_sources, spawn_area.get_area(), *asset_library_);
    const auto& batch_items = planner.get_batch_spawn_assets();
    int spacing = planner.get_batch_grid_spacing();
    int jitter = planner.get_batch_jitter();

             
    asset_info_library_ = asset_library_->all();
    spawn_distributed_batch(batch_items, &spawn_area, spacing, jitter);
    return extract_all_assets();
}



void AssetSpawner::spawn_item_distributed(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) return;

    int spacing = std::max(1, item.grid_spacing);
    int jitter = std::max(0, item.jitter);

    int placed = 0;
    int attempts = 0;
    int max_attempts = item.quantity * 10;

    for (int x = minx; x <= maxx && placed < item.quantity && attempts < max_attempts; x += spacing) {
        for (int y = miny; y <= maxy && placed < item.quantity && attempts < max_attempts; y += spacing) {
            int cx = x + std::uniform_int_distribution<int>(-jitter, jitter)(rng_);
            int cy = y + std::uniform_int_distribution<int>(-jitter, jitter)(rng_);
            ++attempts;

            if (std::uniform_int_distribution<int>(0, 99)(rng_) < item.empty_grid_spaces) continue;
            if (!area->contains_point({cx, cy})) continue;

            if (checker_.check(item.info, cx, cy, exclusion_zones, all_, 1, 0, 5)) continue;

            spawn_(item.name, item.info, *area, cx, cy, 0, nullptr);
            ++placed;
            logger_.progress(item.info, placed, item.quantity);
        }
    }

    logger_.output_and_log(item.name, item.quantity, placed, attempts, max_attempts, "distributed");
}

void AssetSpawner::spawn_distributed_batch(const std::vector<BatchSpawnInfo>& items, const Area* area, int spacing, int jitter) {
    if (!area || items.empty()) {
        return;
    }

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) {
        return;
    }

    std::unordered_map<std::string, int> placed_quantities;
    for (const auto& item : items) placed_quantities[item.name] = 0;

    std::uniform_int_distribution<int> jitter_dist(-jitter, jitter);

    for (int x = minx; x <= maxx; x += spacing) {
        for (int y = miny; y <= maxy; y += spacing) {
            int cx = x + jitter_dist(rng_);
            int cy = y + jitter_dist(rng_);


            if (!area->contains_point({cx, cy})) {
                continue;
            }

            std::vector<int> weights;
            for (const auto& item : items) weights.push_back(item.percent);

            std::discrete_distribution<int> picker(weights.begin(), weights.end());
            const auto& selected = items[picker(rng_)];

            if (selected.name == "null") {
                continue;
            }

            auto it = asset_info_library_.find(selected.name);
            if (it == asset_info_library_.end()) {
                continue;
            }

            auto& info = it->second;

            bool rejected = checker_.check(info, cx, cy, exclusion_zones, all_, 0, 0, 0);
            if (rejected) continue;

            spawn_(selected.name, info, *area, cx, cy, 0, nullptr);
            ++placed_quantities[selected.name];
        }
    }

    for (const auto& item : items) {
        if (item.name == "null") continue;
        int placed = placed_quantities[item.name];
        logger_.output_and_log(item.name, placed, placed, placed, placed, "distributed_batch");
    }

}



void AssetSpawner::spawn_item_random(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

    int spawned = 0;
    int attempts = 0;
    int max_attempts = item.quantity * 10;

    while (spawned < item.quantity && attempts < max_attempts) {
        Point pos = area->random_point_within();
        ++attempts;

        if (!area->contains_point(pos)) continue;

        if (checker_.check(item.info, pos.first, pos.second, exclusion_zones, all_, 1,1,100)) continue;

        spawn_(item.name, item.info, *area, pos.first, pos.second, 0, nullptr);
        ++spawned;
        logger_.progress(item.info, spawned, item.quantity);
    }

    logger_.output_and_log(item.name, item.quantity, spawned, attempts, max_attempts, "random");
}

void AssetSpawner::spawn_item_exact(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.ep_x < 0 || item.ep_y < 0 || !area) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int actual_width = maxx - minx;
    int actual_height = maxy - miny;

    Point center = get_area_center(*area);
    double normalized_x = (item.ep_x - 50.0) / 100.0;
    double normalized_y = (item.ep_y - 50.0) / 100.0;

    int final_x = center.first + static_cast<int>(normalized_x * actual_width);
    int final_y = center.second + static_cast<int>(normalized_y * actual_height);

    if (checker_.check(item.info, final_x, final_y, exclusion_zones, all_, 1,1,20)) {
        logger_.output_and_log(item.name, item.quantity, 0, 1, 1, "exact");
        return;
    }

    spawn_(item.name, item.info, *area, final_x, final_y, 0, nullptr);
    logger_.progress(item.info, 1, item.quantity);
    logger_.output_and_log(item.name, item.quantity, 1, 1, 1, "exact");
}

void AssetSpawner::spawn_item_center(const SpawnInfo& item, const Area* area) {
    if (!item.info || !area) {
        std::cerr << "[AssetSpawner] Failed to spawn asset: info or area is null\n";
        logger_.output_and_log(item.name, item.quantity, 0, 0, 1, "center");
        return;
    }

    Point center = get_area_center(*area);
    if (checker_.check(item.info, center.first, center.second, exclusion_zones, all_, 0,0,0)) {
        logger_.output_and_log(item.name, item.quantity, 0, 1, 1, "center");
        return;
    }

    Asset* result = spawn_(item.name, item.info, *area, center.first, center.second, 0, nullptr);
    int spawned = result ? 1 : 0;

    logger_.progress(item.info, spawned, item.quantity);
    logger_.output_and_log(item.name, item.quantity, spawned, 1, 1, "center");

    if (item.info->type == "Player") {
        if (result) {
        } else {
            std::cerr << "[AssetSpawner] Failed to spawn Player asset '" << item.name << "'\n";
        }
    }
}

void AssetSpawner::spawn_item_perimeter(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

    // how much to shift every spawn upward (in pixels)
    const int up_shift = 40;

    // clamp border_shift into [0, 200]
    int bs_clamped = std::clamp(item.border_shift, 0, 200);
    // 100 → on original perimeter; <100 inward; >100 outward
    double shift_ratio = static_cast<double>(bs_clamped) / 100.0;

    const auto& boundary = area->get_points();
    if (boundary.size() < 2) return;

    // Calculate centroid
    double cx = 0.0, cy = 0.0;
    for (const auto& pt : boundary) {
        cx += pt.first;
        cy += pt.second;
    }
    cx /= boundary.size();
    cy /= boundary.size();

    // Contract or expand boundary by shift_ratio
    std::vector<Point> contracted;
    contracted.reserve(boundary.size());
    for (const auto& pt : boundary) {
        double dx = pt.first - cx;
        double dy = pt.second - cy;
        int new_x = static_cast<int>(std::round(cx + dx * shift_ratio));
        int new_y = static_cast<int>(std::round(cy + dy * shift_ratio));
        contracted.emplace_back(new_x, new_y);
    }

    // Measure segment lengths around the loop
    std::vector<double> segment_lengths;
    double total_length = 0.0;
    for (size_t i = 0; i < contracted.size(); ++i) {
        const Point& a = contracted[i];
        const Point& b = contracted[(i + 1) % contracted.size()];
        double len = std::hypot(b.first - a.first, b.second - a.second);
        segment_lengths.push_back(len);
        total_length += len;
    }
    if (total_length <= 0.0) return;

    double spacing = total_length / item.quantity;
    double dist_accum = 0.0;
    size_t seg_index = 0;

    int angle_center = item.sector_center;
    int angle_range  = item.sector_range;

    int placed   = 0;
    int attempts = 0;

    for (int i = 0; i < item.quantity; ++i) {
        double target = i * spacing;
        while (seg_index < segment_lengths.size() &&
               dist_accum + segment_lengths[seg_index] < target) {
            dist_accum += segment_lengths[seg_index++];
        }
        if (seg_index >= segment_lengths.size()) break;

        const Point& p1 = contracted[seg_index];
        const Point& p2 = contracted[(seg_index + 1) % contracted.size()];
        double t = (target - dist_accum) / segment_lengths[seg_index];
        int x = static_cast<int>(std::round(p1.first + t * (p2.first - p1.first)));
        int y = static_cast<int>(std::round(p1.second + t * (p2.second - p1.second)));

        // sector check
        double angle = std::atan2(y - cy, x - cx) * 180.0 / M_PI;
        if (angle < 0) angle += 360;
        int angle_start = angle_center - angle_range / 2;
        int angle_end   = angle_center + angle_range / 2;
        bool within_sector = false;
        if (angle_start < 0 || angle_end >= 360) {
            within_sector = (angle >= (angle_start + 360) % 360 ||
                             angle <= angle_end % 360);
        } else {
            within_sector = (angle >= angle_start && angle <= angle_end);
        }
        if (!within_sector) continue;

        // apply perimeter offsets
        x += item.perimeter_x_offset;
        y += item.perimeter_y_offset;

        // shift everything upward
        y -= up_shift;

        ++attempts;
        if (checker_.check(item.info, x, y, exclusion_zones, all_, 1, 0, 5))
            continue;

        spawn_(item.name, item.info, *area, x, y, 0, nullptr);
        ++placed;
        logger_.progress(item.info, placed, item.quantity);
    }

    logger_.output_and_log(
        item.name, item.quantity, placed, attempts, item.quantity, "perimeter"
    );
}


AssetSpawner::Point AssetSpawner::get_area_center(const Area& area) const {
    return area.get_center();
}


Asset* AssetSpawner::spawn_(const std::string& /*type*/,
                            const std::shared_ptr<AssetInfo>& info,
                            const Area& area,
                            int x,
                            int y,
                            int /*depth*/,
                            Asset* parent)
{
    auto asset = std::make_unique<Asset>(
        info,
        info->z_threshold,
        area,
        x,
        y,
        parent
    );

    Asset* raw = asset.get();
    all_.push_back(std::move(asset));
    return raw;
}






std::vector<std::unique_ptr<Asset>> AssetSpawner::extract_all_assets() {
    return std::move(all_);
}