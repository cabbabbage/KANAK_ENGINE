#include "asset_generator.hpp"
#include "asset_spawn_planner.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;



AssetGenerator::AssetGenerator(const Area& spawn_area,
                               const nlohmann::json& assets_json,
                               SDL_Renderer* renderer,
                               int map_width,
                               int map_height,
                               AssetLibrary* asset_library,
                               bool batch)
    : spawn_area_(spawn_area),
      renderer_(renderer),
      assets_json_(assets_json),
      map_width_(map_width),
      map_height_(map_height),
      asset_library_(asset_library),
      rng_(std::random_device{}()),
      batch(batch)
{
    AssetSpawnPlanner planner(assets_json_, spawn_area_.get_area(), renderer_, *asset_library_);
    spawn_queue_ = planner.get_spawn_queue();
    asset_info_library_ = asset_library_->all();

    std::vector<SpawnInfo> boundaries;

    for (const SpawnInfo& queue_item : spawn_queue_) {
        if (!queue_item.info) continue;

        const std::string& pos = queue_item.spawn_position;

        if (batch) {
            boundaries.push_back(queue_item);
        } else if (pos == "exact") {
            spawn_item_exact(queue_item, &spawn_area_);
        } else if (pos == "center" || pos == "Center") {
            spawn_item_center(queue_item, &spawn_area_);
        } else if (pos == "perimeter" || pos == "Perimeter") {
            spawn_item_perimeter(queue_item, &spawn_area_);
        } else if (pos == "distributed" || pos == "Distributed") {
            spawn_item_distributed(queue_item, &spawn_area_);
        } else {
            spawn_item_random(queue_item, &spawn_area_);
        }
    }

    if (!boundaries.empty()) {
        spawn_distributed_batch(boundaries, &spawn_area_);
    }

    spawn_all_children();
}


void AssetGenerator::spawn_all_children() {
    std::vector<Asset*> parents;
    for (const auto& uptr : all_) {
        Asset* parent = uptr.get();
        if (!parent || !parent->info || parent->info->child_assets.empty()) continue;
        parents.push_back(parent);
    }

    for (Asset* parent : parents) {
        const int base_x = parent->pos_X;
        const int base_y = parent->pos_Y;

        for (const ChildAsset& def : parent->info->child_assets) {
            int attempts = 0;
            bool placed = false;

            while (attempts < 10 && !placed) {
                int dx = std::uniform_int_distribution<int>(-def.radius, def.radius)(rng_);
                int dy = std::uniform_int_distribution<int>(-def.radius, def.radius)(rng_);
                int cx = base_x + def.point_x + dx;
                int cy = base_y + def.point_y + dy;

                if (!spawn_area_.contains_point({cx, cy})) {
                    ++attempts;
                    continue;
                }

                auto it = asset_info_library_.find(def.asset);
                if (it == asset_info_library_.end() || !it->second) {
                    std::cerr << "[ChildSpawn] No info for child asset type: " << def.asset << "\n";
                    break;
                }

                std::shared_ptr<AssetInfo> child_info = it->second;
                std::unique_ptr<Asset> child = std::make_unique<Asset>(child_info->z_threshold, spawn_area_, renderer_, parent);
                child->info = child_info;
                child->finalize_setup(base_x, base_y + 100);

                Asset copy = *child;  // Dangerous deep copy
                parent->add_child(copy);  // Your busted interface

                placed = true;
            }
        }
    }
}




void AssetGenerator::spawn_item_random(const SpawnInfo& item, const Area* area) {
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
            std::vector<Asset*> nearest = get_closest_assets(pos.first, pos.second, 5);

            if (check_spacing && check_spacing_overlap(item.info, pos.first, pos.second, nearest)) {
                spacing_failed = true;
            }

            if (check_distance && check_min_type_distance(item.info, pos)) {
                distance_failed = true;
            }
        }

        if (!spacing_failed && !distance_failed) {
            spawn(item.name, item.info, *area, pos.first, pos.second, 0, nullptr);
            ++spawned;
        }

        ++attempts;
    }

    if (spawned < item.quantity) {
        std::cout << "[spawn_item_random] Only spawned " << spawned
                  << " of " << item.quantity << " for asset '" << item.name << "'\n";
    }
}



void AssetGenerator::spawn_distributed_batch(const std::vector<SpawnInfo>& items, const Area* area) {
    if (!area || items.empty()) return;

    const int GRID_SPACING = 100;

    // Calculate total quantity and create mutable working list
    int total_quantity = 0;
    std::vector<SpawnInfo> pool = items;
    for (const auto& item : pool) {
        if (item.info && item.quantity > 0)
            total_quantity += item.quantity;
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

            if (!area->contains_point({cx, cy}))
                continue;

            // Build weight vector for asset selection
            std::vector<int> weights;
            for (const auto& item : pool) {
                weights.push_back(item.quantity);
            }

            std::discrete_distribution<int> picker(weights.begin(), weights.end());
            int chosen_index = picker(rng_);

            SpawnInfo& selected = pool[chosen_index];
            spawn(selected.name, selected.info, *area, cx, cy, 0, nullptr);

            --selected.quantity;
            --total_quantity;

            if (selected.quantity <= 0) {
                pool.erase(pool.begin() + chosen_index);
            }
        }
    }

    // Any leftovers
    for (const auto& leftover : pool) {
        if (leftover.quantity > 0) {
            std::cout << "[spawn_distributed_batch] Leftover: " << leftover.quantity
                      << " of '" << leftover.name << "'\n";
        }
    }
}


void AssetGenerator::spawn_item_distributed(const SpawnInfo& item, const Area* area) {
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
    for (int gx = 0; gx < grid_cols && spawned < item.quantity; ++gx) {
        for (int gy = 0; gy < grid_rows && spawned < item.quantity; ++gy) {
            int cx = minx + gx * cell_w + cell_w / 2;
            int cy = miny + gy * cell_h + cell_h / 2;

            cx += std::uniform_int_distribution<int>(-wiggle_x, wiggle_x)(rng_);
            cy += std::uniform_int_distribution<int>(-wiggle_y, wiggle_y)(rng_);

            if (!area->contains_point({cx, cy})) continue;

            spawn(item.name, item.info, *area, cx, cy, 0, nullptr);
            ++spawned;
        }
    }

    if (spawned < item.quantity) {
        std::cout << "[spawn_item_distributed] Only placed " << spawned
                  << " of " << item.quantity << " for '" << item.name << "'\n";
    }
}


void AssetGenerator::spawn_item_exact(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.x_position < 0 || item.y_position < 0) return;

    auto [minx, miny, maxx, maxy] = area->get_bounds();
    int actual_width = maxx - minx;
    int actual_height = maxy - miny;

    Point center = get_area_center(*area);

    double normalized_x = (static_cast<double>(item.x_position) - 50.0) / 100.0;
    double normalized_y = (static_cast<double>(item.y_position) - 50.0) / 100.0;

    int final_x = center.first + static_cast<int>(normalized_x * actual_width);
    int final_y = center.second + static_cast<int>(normalized_y * actual_height);

    std::vector<Asset*> nearest = get_closest_assets(final_x, final_y, 5);

    if (!check_spacing_overlap(item.info, final_x, final_y, nearest) &&
        !check_min_type_distance(item.info, {final_x, final_y})) {
        spawn(item.name, item.info, *area, final_x, final_y, 0, nullptr);
    }
}


void AssetGenerator::spawn_item_center(const SpawnInfo& item, const Area* area) {
    if (!item.info) return;

    Point center = get_area_center(*area);

    bool check_spacing = item.info->has_spacing_area;
    bool check_distance = item.info->min_same_type_distance > 0;

    if (!check_spacing && !check_distance) {
        spawn(item.name, item.info, *area, center.first, center.second, 0, nullptr);
        return;
    }

    std::vector<Asset*> nearest = get_closest_assets(center.first, center.second, 5);

    if (check_spacing && check_spacing_overlap(item.info, center.first, center.second, nearest)) return;
    if (check_distance && check_min_type_distance(item.info, center)) return;

    spawn(item.name, item.info, *area, center.first, center.second, 0, nullptr);
}



void AssetGenerator::spawn_item_perimeter(const SpawnInfo& item, const Area* area) {
    if (!item.info || item.quantity <= 0 || !area) return;

    const std::vector<Point>& boundary = area->get_points();
    if (boundary.size() < 2) return;

    // Compute total perimeter length and segment lengths
    std::vector<double> segment_lengths;
    double total_length = 0.0;

    for (size_t i = 0; i < boundary.size(); ++i) {
        const Point& p1 = boundary[i];
        const Point& p2 = boundary[(i + 1) % boundary.size()];
        double len = std::hypot(p2.first - p1.first, p2.second - p1.second);
        segment_lengths.push_back(len);
        total_length += len;
    }

    if (total_length <= 0.0) return;

    double spacing = total_length / item.quantity;
    double dist_accum = 0.0;
    size_t seg_index = 0;
    double seg_offset = 0.0;

    for (int i = 0; i < item.quantity; ++i) {
        double target_dist = i * spacing;

        while (seg_index < segment_lengths.size() &&
               dist_accum + segment_lengths[seg_index] < target_dist) {
            dist_accum += segment_lengths[seg_index];
            ++seg_index;
        }

        if (seg_index >= segment_lengths.size()) break;

        const Point& a = boundary[seg_index];
        const Point& b = boundary[(seg_index + 1) % boundary.size()];
        double seg_len = segment_lengths[seg_index];
        double local_t = (target_dist - dist_accum) / seg_len;

        int x = static_cast<int>(std::round(a.first + local_t * (b.first - a.first)));
        int y = static_cast<int>(std::round(a.second + local_t * (b.second - a.second)));

        spawn(item.name, item.info, *area, x, y, 0, nullptr);
    }
}



bool AssetGenerator::check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                                           int test_pos_X,
                                           int test_pos_Y,
                                           const std::vector<Asset*>& closest_assets) const {
    if (!info || !info->has_spacing_area) return false;

    Area test_area = info->spacing_area;
    test_area.align(test_pos_X, test_pos_Y);

    for (Asset* other : closest_assets) {
        if (!other || !other->info || !other->info->has_spacing_area) continue;
        Area other_area = other->get_global_spacing_area();
        if (test_area.intersects(other_area)) {
            std::cout << "[SpacingCheck] Overlap with asset at (" << other->pos_X << "," << other->pos_Y << ")\n";
            return true;
        }
    }

    return false;
}


AssetGenerator::Point AssetGenerator::get_area_center(const Area& area) const {
    auto [minx, miny, maxx, maxy] = area.get_bounds();
    int cx = (minx + maxx) / 2;
    int cy = (miny + maxy) / 2;
    return {cx, cy};
}



bool AssetGenerator::check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                             const Point& pos) const {
    if (!info || info->name.empty() || info->min_same_type_distance <= 0) return false;

    const int min_dist_sq = info->min_same_type_distance * info->min_same_type_distance;

    for (const auto& uptr : all_) {
        Asset* existing = uptr.get();
        if (!existing || !existing->info) continue;
        if (existing->info->name != info->name) continue;

        int dx = existing->pos_X - pos.first;
        int dy = existing->pos_Y - pos.second;
        int dist_sq = dx * dx + dy * dy;

        if (dist_sq < min_dist_sq) {
            std::cout << "[DistanceCheck] Rejected '" << info->name
                      << "' — too close to (" << existing->pos_X << "," << existing->pos_Y << ")"
                      << " dist^2=" << dist_sq << " < min^2=" << min_dist_sq << "\n";
            return true;
        }
    }

    return false;
}

Asset* AssetGenerator::spawn(const std::string& type,
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

AssetGenerator::Point AssetGenerator::get_point_within_area(const Area& area) {
    auto [minx, miny, maxx, maxy] = area.get_bounds();
    for (int i = 0; i < 100; ++i) {
        int x = std::uniform_int_distribution<int>(minx, maxx)(rng_);
        int y = std::uniform_int_distribution<int>(miny, maxy)(rng_);
        if (area.contains_point({x, y})) return {x, y};
    }
    return {0, 0};
}

std::vector<std::unique_ptr<Asset>>&& AssetGenerator::extract_all_assets() {
    return std::move(all_);
}

std::vector<Asset*> AssetGenerator::get_closest_assets(int x, int y, int max_count) const {
    std::vector<std::pair<int, Asset*>> pairs;
    for (const auto& uptr : all_) {
        Asset* a = uptr.get();
        if (!a || !a->info || !a->info->has_spacing_area) continue;

        int dx = a->pos_X - x;
        int dy = a->pos_Y - y;
        int dist_sq = dx * dx + dy * dy;
        pairs.emplace_back(dist_sq, a);
    }

    std::sort(pairs.begin(), pairs.end());

    std::vector<Asset*> closest;
    for (int i = 0; i < std::min(max_count, static_cast<int>(pairs.size())); ++i) {
        closest.push_back(pairs[i].second);
    }

    return closest;
}



