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

// 2) spawn(...) takes an Area each time
void AssetSpawner::spawn(const Area& spawn_area,
                         const nlohmann::json& assets_json,
                         bool batch,
                         const std::string& map_dir,
                         const std::string& room_dir)
{
    assets_json_  = assets_json;
    batch_        = batch;
    map_dir_      = map_dir;
    room_dir_     = room_dir;
    logger_       = SpawnLogger(map_dir_, room_dir_);

    // build planner for *this* area
    AssetSpawnPlanner planner(assets_json_, spawn_area.get_area(), renderer_, *asset_library_);
    spawn_queue_ = planner.get_spawn_queue();

    // pull in the AssetInfo map
    asset_info_library_ = asset_library_->all();

    std::vector<SpawnInfo> boundaries;
    for (auto& queue_item : spawn_queue_) {
        logger_.start_timer();
        if (!queue_item.info) continue;

        const auto& pos = queue_item.spawn_position;
        if (batch_) {
            boundaries.push_back(queue_item);
        }
        else if (pos == "exact") {
            spawn_item_exact(queue_item, &spawn_area);
        }
        else if (pos == "center" || pos == "Center") {
            spawn_item_center(queue_item, &spawn_area);
        }
        else if (pos == "perimeter" || pos == "Perimeter") {
            spawn_item_perimeter(queue_item, &spawn_area);
        }
        else if (pos == "distributed" || pos == "Distributed") {
            spawn_item_distributed(queue_item, &spawn_area);
        }
        else {
            spawn_item_random(queue_item, &spawn_area);
        }
    }

    if (!boundaries.empty()) {
        spawn_distributed_batch(boundaries, &spawn_area);
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


void AssetSpawner::spawn_distributed_batch(const std::vector<SpawnInfo>& items, const Area* area) {
    if (!area || items.empty()) return;

    const int GRID_SPACING = 100;
    std::unordered_map<std::string, int> original_quantities;
    std::unordered_map<std::string, int> placed_quantities;
    std::unordered_map<std::string, std::shared_ptr<AssetInfo>> asset_infos;

    int total_quantity = 0;
    auto pool = items;
    for (const auto& item : pool) {
        if (item.info && item.quantity > 0) {
            total_quantity += item.quantity;
            original_quantities[item.name] = item.quantity;
            placed_quantities[item.name] = 0;
            asset_infos[item.name] = item.info;
        }
    }
    if (total_quantity <= 0) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) return;

    const int wiggle_x = GRID_SPACING / 3;
    const int wiggle_y = GRID_SPACING / 3;

    for (int x = minx; x <= maxx; x += GRID_SPACING) {
        for (int y = miny; y <= maxy; y += GRID_SPACING) {
            if (total_quantity <= 0) return;

            int jitter_x = std::uniform_int_distribution<int>(-wiggle_x, wiggle_x)(rng_);
            int jitter_y = std::uniform_int_distribution<int>(-wiggle_y, wiggle_y)(rng_);
            int cx = x + jitter_x;
            int cy = y + jitter_y;

            if (!area->contains_point({cx, cy})) continue;

            std::vector<int> weights;
            for (const auto& item : pool) weights.push_back(item.quantity);

            std::discrete_distribution<int> picker(weights.begin(), weights.end());
            int chosen_index = picker(rng_);
            auto& selected = pool[chosen_index];
            auto nearest = checker_.get_closest_assets(cx, cy, 3);
            if (!checker_.check_spacing_overlap(selected.info, cx, cy, nearest)) {
                spawn_(selected.name, selected.info, *area, cx, cy, 0, nullptr);
            }
            --selected.quantity;
            --total_quantity;
            ++placed_quantities[selected.name];

            logger_.progress(selected.info, placed_quantities[selected.name], original_quantities[selected.name]);

            if (selected.quantity <= 0) pool.erase(pool.begin() + chosen_index);
        }
    }

    std::cout << std::endl;
    for (const auto& [name, attempted] : original_quantities) {
        int placed = placed_quantities[name];
        logger_.output_and_log(name, attempted, placed, attempted, attempted, "distributed_batch");
    }
}


void AssetSpawner::spawn_item_distributed(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int w = maxx - minx;
    int h = maxy - miny;
    if (w <= 0 || h <= 0) return;

    int grid_cols = static_cast<int>(std::sqrt(item.quantity));
    int grid_rows = (item.quantity + grid_cols - 1) / grid_cols;
    if (grid_cols <= 0 || grid_rows <= 0) return;

    int cell_w = std::max(1, w / grid_cols);
    int cell_h = std::max(1, h / grid_rows);
    int wiggle_x = std::max(1, cell_w / 3);
    int wiggle_y = std::max(1, cell_h / 3);

    int spawned = 0;
    while (spawned < item.quantity) {
        for (int gx = 0; gx < grid_cols && spawned < item.quantity; ++gx) {
            for (int gy = 0; gy < grid_rows && spawned < item.quantity; ++gy) {
                int cx = minx + gx * cell_w + cell_w / 2;
                int cy = miny + gy * cell_h + cell_h / 2;

                cx += std::uniform_int_distribution<int>(-wiggle_x, wiggle_x)(rng_);
                cy += std::uniform_int_distribution<int>(-wiggle_y, wiggle_y)(rng_);

                if (!area->contains_point({cx, cy})) continue;

                auto nearest = checker_.get_closest_assets(cx, cy, 2);
                if (!checker_.check_spacing_overlap(item.info, cx, cy, nearest)) {
                    spawn_(item.name, item.info, *area, cx, cy, 0, nullptr);
                }
                ++spawned;
                logger_.progress(item.info, spawned, item.quantity);
            }
        }
    }

    logger_.output_and_log(item.name, item.quantity, spawned, item.quantity, item.quantity, "distributed");
}


void AssetSpawner::spawn_item_exact(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.x_position < 0 || item.y_position < 0) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int actual_width = maxx - minx;
    int actual_height = maxy - miny;

    Point center = get_area_center(*area);
    double normalized_x = (item.x_position - 50.0) / 100.0;
    double normalized_y = (item.y_position - 50.0) / 100.0;

    int final_x = center.first + static_cast<int>(normalized_x * actual_width);
    int final_y = center.second + static_cast<int>(normalized_y * actual_height);

    auto nearest = checker_.get_closest_assets(final_x, final_y, 5);
    if (!checker_.check_spacing_overlap(item.info, final_x, final_y, nearest) &&
        !checker_.check_min_type_distance(item.info, {final_x, final_y})) {
        spawn_(item.name, item.info, *area, final_x, final_y, 0, nullptr);
    }
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

    // Contract the boundary inward by 10%
    std::vector<Point> contracted;
    contracted.reserve(boundary.size());
    for (const auto& pt : boundary) {
        double dx = pt.first - cx;
        double dy = pt.second - cy;
        int new_x = static_cast<int>(std::round(cx + dx * 0.9));
        int new_y = static_cast<int>(std::round(cy + dy * 0.9));
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

        auto nearest = checker_.get_closest_assets(x, y, 5);
        if (!checker_.check_spacing_overlap(item.info, x, y, nearest)) {
            spawn_(item.name, item.info, *area, x, y, 0, nullptr);
        }
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






