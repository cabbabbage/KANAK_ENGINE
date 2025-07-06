#include "asset_spawner.hpp"
#include "asset_spawn_planner.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

AssetSpawner::AssetSpawner(SDL_Renderer* renderer,
                           int map_width,
                           int map_height,
                           AssetLibrary* asset_library)
    : renderer_(renderer),
      map_width_(map_width),
      map_height_(map_height),
      asset_library_(asset_library),
      rng_(std::random_device{}()),
      checker_(all_),
      logger_("", "")
{}

void AssetSpawner::spawn(const Area& spawn_area,
                         const nlohmann::json& assets_json,
                         const std::string& map_dir,
                         const std::string& room_dir)
{
    assets_json_  = assets_json;
    map_dir_      = map_dir;
    room_dir_     = room_dir;
    logger_       = SpawnLogger(map_dir_, room_dir_);

    AssetSpawnPlanner planner(assets_json_, spawn_area.get_area(), renderer_, *asset_library_);
    spawn_queue_ = planner.get_spawn_queue();
    auto batch_assets = planner.get_batch_spawn_assets();
    int batch_spacing = planner.get_batch_grid_spacing();
    int batch_jitter = planner.get_batch_jitter();

    asset_info_library_ = asset_library_->all();

    for (auto& queue_item : spawn_queue_) {
        logger_.start_timer();
        if (!queue_item.info) continue;

        const auto& pos = queue_item.position;
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
   //     else if (pos == "Entrance") {
 //           spawn_item_random(queue_item, &spawn_area);  // TODO: Implement entrance-specific logic if needed
//        }
//        else if (pos == "Intersection") {
//            spawn_item_random(queue_item, &spawn_area);  // TODO: Implement intersection-specific logic if needed
//        }
        else {
            spawn_item_random(queue_item, &spawn_area);
        }
    }

    if (!batch_assets.empty()) {
        spawn_distributed_batch(batch_assets, &spawn_area, batch_spacing, batch_jitter);
    }
}




void AssetSpawner::spawn_item_distributed(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) return;

    int spacing = std::max(1, item.grid_spacing);
    int jitter = std::max(0, item.jitter);
    int wiggle_x = jitter;
    int wiggle_y = jitter;

    int placed = 0;
    int attempts = 0;
    int max_attempts = item.quantity * 10;

    for (int x = minx; x <= maxx && placed < item.quantity && attempts < max_attempts; x += spacing) {
        for (int y = miny; y <= maxy && placed < item.quantity && attempts < max_attempts; y += spacing) {
            int jitter_x = std::uniform_int_distribution<int>(-wiggle_x, wiggle_x)(rng_);
            int jitter_y = std::uniform_int_distribution<int>(-wiggle_y, wiggle_y)(rng_);
            int cx = x + jitter_x;
            int cy = y + jitter_y;

            ++attempts;

            // Empty grid space chance
            int chance = std::uniform_int_distribution<int>(0, 99)(rng_);
            if (chance < item.empty_grid_spaces) continue;

            if (!area->contains_point({cx, cy})) continue;



                spawn_(item.name, item.info, *area, cx, cy, 0, nullptr);
                ++placed;
                logger_.progress(item.info, placed, item.quantity);
            
        }
    }

    logger_.output_and_log(item.name, item.quantity, placed, attempts, max_attempts, "distributed");
}






void AssetSpawner::spawn_distributed_batch(const std::vector<BatchSpawnInfo>& items, const Area* area, int spacing, int jitter) {
    if (!area || items.empty()) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) return;

    std::unordered_map<std::string, int> placed_quantities;
    for (const auto& item : items) {
        placed_quantities[item.name] = 0;
    }

    std::uniform_int_distribution<int> jitter_dist(-jitter, jitter);

    for (int x = minx; x <= maxx; x += spacing) {
        for (int y = miny; y <= maxy; y += spacing) {
            int jitter_x = jitter_dist(rng_);
            int jitter_y = jitter_dist(rng_);
            int cx = x + jitter_x;
            int cy = y + jitter_y;

            if (!area->contains_point({cx, cy})) continue;

            std::vector<int> weights;
            for (const auto& item : items) {
                weights.push_back(item.percent);
            }

            std::discrete_distribution<int> picker(weights.begin(), weights.end());
            int chosen_index = picker(rng_);
            const auto& selected = items[chosen_index];

            if (selected.name == "null") continue;

            auto it = asset_info_library_.find(selected.name);
            if (it == asset_info_library_.end()) continue;


            spawn_(selected.name, it->second, *area, cx, cy, 0, nullptr);
            ++placed_quantities[selected.name];
            
        }
    }

    std::cout << std::endl;
    for (const auto& item : items) {
        if (item.name == "null") continue;
        int placed = placed_quantities[item.name];
        logger_.output_and_log(item.name, placed, placed, placed, placed, "distributed_batch");
    }
}






void AssetSpawner::spawn_item_random(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0) return;

    bool check_spacing = item.info->has_spacing_area;
    bool check_distance = item.info->min_same_type_distance > 0;

    int attempts = 0;
    int spawned = 0;
    int max_attempts = item.quantity * 10;

    while (spawned < item.quantity && attempts < max_attempts) {
        Point pos = get_point_within_area(*area);

        bool spacing_failed = false;
        bool distance_failed = false;

        if (check_spacing || check_distance) {
            auto nearest = checker_.get_closest_assets(pos.first, pos.second, 3);

            if (check_spacing && checker_.check_spacing_overlap(item.info, pos.first, pos.second, nearest)) {
                spacing_failed = true;
            }

            if (check_distance && checker_.check_min_type_distance(item.info, pos)) {
                distance_failed = true;
            }
        }

        if (!spacing_failed && !distance_failed) {
            spawn_(item.name, item.info, *area, pos.first, pos.second, 0, nullptr);
            ++spawned;
        }

        ++attempts;
        logger_.progress(item.info, attempts, max_attempts);
    }

    logger_.output_and_log(item.name, item.quantity, spawned, attempts, max_attempts, "random");
}



void AssetSpawner::spawn_item_exact(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.ep_x < 0 || item.ep_y < 0) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int actual_width = maxx - minx;
    int actual_height = maxy - miny;

    Point center = get_area_center(*area);
    double normalized_x = (item.ep_x - 50.0) / 100.0;
    double normalized_y = (item.ep_y - 50.0) / 100.0;

    int final_x = center.first + static_cast<int>(normalized_x * actual_width);
    int final_y = center.second + static_cast<int>(normalized_y * actual_height);
    spawn_(item.name, item.info, *area, final_x, final_y, 0, nullptr);
}



void AssetSpawner::spawn_item_center(const SpawnInfo& item, const Area* area) {
    if (!item.info) {
        std::cerr << "[AssetSpawner] Failed to spawn_ asset: info is null\n";
        logger_.output_and_log(item.name, item.quantity, 0, 0, 1, "center");
        return;
    }

    Point center = get_area_center(*area);
    Asset* result = spawn_(item.name, item.info, *area, center.first, center.second, 0, nullptr);
    int spawned = result ? 1 : 0;

    logger_.progress(item.info, spawned, item.quantity);
    logger_.output_and_log(item.name, item.quantity, spawned, 1, 1, "center");

    if (item.info->type == "Player") {
        if (result) {
            std::cout << "[AssetSpawner] Player asset '" << item.name << "' successfully spawned at center\n";
        } else {
            std::cerr << "[AssetSpawner] Failed to spawn_ Player asset '" << item.name << "'\n";
        }
    }
}


void AssetSpawner::spawn_item_perimeter(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

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

    // Contract boundary inward based on border_shift
    double shift_ratio = 1.0 - (item.border_shift / 100.0);
    std::vector<Point> contracted;
    contracted.reserve(boundary.size());
    for (const auto& pt : boundary) {
        double dx = pt.first - cx;
        double dy = pt.second - cy;
        int new_x = static_cast<int>(std::round(cx + dx * shift_ratio));
        int new_y = static_cast<int>(std::round(cy + dy * shift_ratio));
        contracted.emplace_back(new_x, new_y);
    }

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
    int angle_range = item.sector_range;

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

        double angle = std::atan2(y - cy, x - cx) * 180.0 / M_PI;
        if (angle < 0) angle += 360;

        int angle_start = angle_center - angle_range / 2;
        int angle_end = angle_center + angle_range / 2;
        bool within_sector = false;
        if (angle_start < 0 || angle_end >= 360) {
            // wraparound case
            within_sector = (angle >= (angle_start + 360) % 360 || angle <= angle_end % 360);
        } else {
            within_sector = (angle >= angle_start && angle <= angle_end);
        }

        if (!within_sector) continue;

        x += item.perimeter_x_offset;
        y += item.perimeter_y_offset;


        spawn_(item.name, item.info, *area, x, y, 0, nullptr);
    }
}

















AssetSpawner::Point AssetSpawner::get_area_center(const Area& area) const {
    auto [minx, miny, maxx, maxy] = area.get_bounds();
    int cx = (minx + maxx) / 2;
    int cy = (miny + maxy) / 2;
    return {cx, cy};
}










void AssetSpawner::removeAsset(Asset* asset) {
    auto it = std::find_if(all_.begin(), all_.end(),
        [&](const std::unique_ptr<Asset>& up) { return up.get() == asset; });
    if (it != all_.end()) {
        all_.erase(it);
    }
}



Asset* AssetSpawner::spawn_(const std::string& type,
                             const std::shared_ptr<AssetInfo>& info,
                             const Area& area,
                             int x,
                             int y,
                             int /*depth*/,
                             Asset* parent)
{
    auto asset = std::make_unique<Asset>(info->z_threshold, area, renderer_, parent);
    asset->info = info;

    if (parent) {
        asset->finalize_setup(x, y, parent->pos_X, parent->pos_Y);
    } else {
        asset->finalize_setup(x, y);
    }

    Asset* raw = asset.get();
    all_.push_back(std::move(asset));
    return raw;
}

AssetSpawner::Point AssetSpawner::get_point_within_area(const Area& area) {
    auto [minx, miny, maxx, maxy] = area.get_bounds();
    for (int i = 0; i < 100; ++i) {
        int x = std::uniform_int_distribution<int>(minx, maxx)(rng_);
        int y = std::uniform_int_distribution<int>(miny, maxy)(rng_);
        if (area.contains_point({x, y})) return {x, y};
    }
    return {0, 0};
}

std::vector<std::unique_ptr<Asset>>&& AssetSpawner::extract_all_assets() {
    return std::move(all_);
}






