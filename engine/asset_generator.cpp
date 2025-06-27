#include "asset_generator.hpp"
#include "asset_spawn_planner.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>


namespace fs = std::filesystem;



AssetGenerator::AssetGenerator(const Area& spawn_area,
                               const nlohmann::json& assets_json,
                               SDL_Renderer* renderer,
                               int map_width,
                               int map_height,
                               AssetLibrary* asset_library,
                               bool batch,
                               const std::string& map_dir,
                               const std::string& room_dir)
    : spawn_area_(spawn_area),
      renderer_(renderer),
      assets_json_(assets_json),
      map_width_(map_width),
      map_height_(map_height),
      asset_library_(asset_library),
      rng_(std::random_device{}()),
      batch(batch),
      map_dir_(map_dir),
      room_dir_(room_dir)
{

    AssetSpawnPlanner planner(assets_json_, spawn_area_.get_area(), renderer_, *asset_library_);
    spawn_queue_ = planner.get_spawn_queue();
    asset_info_library_ = asset_library_->all();

    std::vector<SpawnInfo> boundaries;

    for (const SpawnInfo& queue_item : spawn_queue_) {
        start_time_ = std::chrono::steady_clock::now();  
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
        progress(item.info, attempts, max_attempts);

    }
    output_and_log(item.name, item.quantity, spawned, attempts, max_attempts, "random");
}



void AssetGenerator::spawn_distributed_batch(const std::vector<SpawnInfo>& items, const Area* area) {
    if (!area || items.empty()) return;

    const int GRID_SPACING = 100;

    // Track original quantities and placements
    std::unordered_map<std::string, int> original_quantities;
    std::unordered_map<std::string, int> placed_quantities;
    std::unordered_map<std::string, std::shared_ptr<AssetInfo>> asset_infos;

    // Create working pool
    int total_quantity = 0;
    std::vector<SpawnInfo> pool = items;
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
            ++placed_quantities[selected.name];

            // 🔄 Progress update for selected asset
            progress(selected.info, placed_quantities[selected.name], original_quantities[selected.name]);

            if (selected.quantity <= 0) {
                pool.erase(pool.begin() + chosen_index);
            }
        }
    }

    std::cout << std::endl;

    // Log each asset's result individually
    for (const auto& [name, attempted] : original_quantities) {
        int placed = placed_quantities[name];
        output_and_log(name, attempted, placed, attempted, attempted, "distributed_batch");
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
            progress(item.info, spawned, item.quantity);
        }
    }

    output_and_log(item.name, item.quantity, spawned, item.quantity, item.quantity, "distributed");

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
    if (!item.info) {
        std::cerr << "[AssetGenerator] Failed to spawn asset: info is null\n";
        return;
    }

    Point center = get_area_center(*area);

    Asset* result = spawn(item.name, item.info, *area, center.first, center.second, 0, nullptr);

    if (item.info->type == "Player") {
        if (result) {
            std::cout << "[AssetGenerator] Player asset '" << item.name << "' successfully spawned at center\n";
        } else {
            std::cerr << "[AssetGenerator] Failed to spawn Player asset '" << item.name << "'\n";
        }
    }
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

void AssetGenerator::output_and_log(const std::string& asset_name,
                                    int quantity,
                                    int spawned,
                                    int attempts,
                                    int max_attempts,
                                    const std::string& method) {
    // === Time calculation ===
    auto end_time_ = std::chrono::steady_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();

    // === CSV Logging ===
    const std::string csv_path = map_dir_ + "/spawn_log.csv";

    std::ifstream infile(csv_path);
    std::vector<std::string> lines;

    if (infile.is_open()) {
        std::string line;
        while (std::getline(infile, line)) {
            lines.push_back(line);
        }
        infile.close();
    }

    // Ensure room section exists
    int room_line_index = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].empty() && i + 3 < lines.size()
            && lines[i + 1].empty() && lines[i + 2].empty()
            && lines[i + 3] == room_dir_) {
            room_line_index = static_cast<int>(i + 3);
            break;
        }
    }

    if (room_line_index == -1) {
        lines.emplace_back("");
        lines.emplace_back("");
        lines.emplace_back("");
        room_line_index = static_cast<int>(lines.size());
        lines.push_back(room_dir_);
    }

    // Find or insert asset line in room section
    int insert_index = room_line_index + 1;
    int asset_line_index = -1;
    while (insert_index < static_cast<int>(lines.size()) && !lines[insert_index].empty()) {
        std::istringstream ss(lines[insert_index]);
        std::string first_col;
        std::getline(ss, first_col, ',');
        if (first_col == asset_name) {
            asset_line_index = insert_index;
            break;
        }
        ++insert_index;
    }

    int total_success = spawned;
    int total_attempts = attempts;
    double new_percent = total_attempts > 0 ? static_cast<double>(total_success) / total_attempts : 0.0;

    double average_time = duration_ms;
    int times_generated = 1;
    double delta_time = 0.0;

    if (asset_line_index != -1) {
        std::istringstream ss(lines[asset_line_index]);
        std::string name, percent_str, success_str, attempts_str, method_str, avg_time_str, times_gen_str;
        std::getline(ss, name, ',');
        std::getline(ss, percent_str, ',');
        std::getline(ss, success_str, ',');
        std::getline(ss, attempts_str, ',');
        std::getline(ss, method_str, ',');
        std::getline(ss, avg_time_str, ',');
        std::getline(ss, times_gen_str, ',');

        if (method_str == method) {
            total_success += std::stoi(success_str);
            total_attempts += std::stoi(attempts_str);
            new_percent = total_attempts > 0 ? static_cast<double>(total_success) / total_attempts : 0.0;

            double prev_avg_time = std::stod(avg_time_str);
            int prev_generations = std::stoi(times_gen_str);

            average_time = (prev_avg_time * prev_generations + duration_ms) / (prev_generations + 1);
            times_generated = prev_generations + 1;

            delta_time = duration_ms - prev_avg_time;
        } else {
            total_success = spawned;
            total_attempts = attempts;
            new_percent = total_attempts > 0 ? static_cast<double>(total_success) / total_attempts : 0.0;
            average_time = duration_ms;
            times_generated = 1;
            delta_time = 0.0;
        }
    } else {
        asset_line_index = insert_index;
        lines.insert(lines.begin() + asset_line_index, "");  // placeholder
    }

    std::ostringstream updated_line;
    updated_line << asset_name << ","
                 << std::fixed << std::setprecision(3) << new_percent << ","
                 << total_success << ","
                 << total_attempts << ","
                 << method << ","
                 << std::fixed << std::setprecision(3) << average_time << ","
                 << times_generated << ","
                 << std::fixed << std::setprecision(3) << delta_time;

    lines[asset_line_index] = updated_line.str();

    std::ofstream outfile(csv_path);
    if (outfile.is_open()) {
        for (const std::string& line : lines) {
            outfile << line << "\n";
        }
        outfile.close();
    }
}




void AssetGenerator::progress(const std::shared_ptr<AssetInfo>& info, int current, int total) {
    const int bar_width = 50;
    double percent = (total > 0) ? static_cast<double>(current) / total : 0.0;
    int filled = static_cast<int>(percent * bar_width);

    std::string bar(filled, '#');
    bar.resize(bar_width, '-');

    std::ostringstream oss;
    oss << "[Checking] " << std::left << std::setw(20) << info->name
        << "[" << bar << "] "
        << std::setw(3) << static_cast<int>(percent * 100) << "%\r";

    std::cout << oss.str() << std::flush;
}
