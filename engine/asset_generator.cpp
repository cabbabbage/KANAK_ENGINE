// asset_generator.cpp
#include "asset_generator.hpp"
#include <algorithm>
#include <iostream>

AssetGenerator::AssetGenerator(const Area& spawn_area,
                               const nlohmann::json& assets_json,
                               SDL_Renderer* renderer,
                               bool invert_area,
                               int map_width,
                               int map_height)
    : spawn_area_(spawn_area),
      renderer_(renderer),
      assets_json_(assets_json),
      invert_area_(invert_area),
      map_width_(map_width),
      map_height_(map_height),
      rng_(std::random_device{}())
{
    auto [minx, miny, maxx, maxy] = spawn_area_.get_bounds();
    std::cout << "[AssetGenerator] Initializing with spawn area bounds: (" << minx << "," << miny << ") to (" << maxx << "," << maxy << ")\n";

    std::cout << "[AssetGenerator] Building AssetInfo library:\n";
    for (const auto& entry : assets_json_) {
        std::string name = entry.value("name", "");
        if (name.empty()) continue;

        try {
            asset_info_library_[name] = std::make_shared<AssetInfo>(name, renderer_);
            std::cout << "  [AssetGenerator] Loaded AssetInfo for '" << name << "'\n";
        } catch (const std::exception& e) {
            std::cerr << "[AssetGenerator] Failed to create AssetInfo for '" << name << "': " << e.what() << "\n";
        }
    }
}

void AssetGenerator::generate_all() {
    for (const auto& entry : assets_json_) {
        std::string type = entry.value("name", "");
        if (type.empty() || !entry.value("is_active", false)) continue;
        if (!asset_info_library_.count(type)) continue;

        int min_n = entry.value("min_number", 0);
        int max_n = entry.value("max_number", min_n);
        int count = std::uniform_int_distribution<int>(min_n, max_n)(rng_);

        std::cout << "[AssetGenerator] Attempting to spawn " << count << " of '" << type << "'\n";
        std::vector<Asset*> siblings;

        for (int i = 0; i < count; ++i) {
            bool success = generate_asset_recursive(type, spawn_area_, 0, nullptr, siblings);
            if (!success) {
                std::cerr << "[AssetGenerator] FAILED to place '" << type << "' instance " << i + 1 << " of " << count << "\n";
            }
        }
    }
}

bool AssetGenerator::generate_asset_recursive(const std::string& type,
                                              const Area& valid_area,
                                              int depth,
                                              Asset* parent,
                                              std::vector<Asset*>& siblings,
                                              bool terminate_with_parent)
{
    std::cout << "[AssetGenerator] Trying to place '" << type << "' (depth=" << depth << ")\n";

    if (!asset_info_library_.count(type)) return false;
    std::shared_ptr<AssetInfo> info = asset_info_library_[type];
    if (!info || depth > info->child_depth) return false;

    int attempt = 0;
    while (type == "Davey" || attempt < 5) {
        Point pos = get_random_point(valid_area);
        bool inside = valid_area.contains_point(pos);

        if ((inside && invert_area_) || (!inside && !invert_area_)) {
            std::cerr << "[Attempt " << attempt << "] Skipped '" << type << "' at (" << pos.first << "," << pos.second << "): invalid position (inside=" << inside << ", invert=" << invert_area_ << ")\n";
            ++attempt;
            continue;
        }

        bool overlaps = false;
        if (info->has_spacing_area && !invert_area_) {
            Area test_area = info->spacing_area;
            test_area.apply_offset(pos.first, pos.second);

            for (const auto& existing : all_) {
                if (existing.get() == parent) continue;
                if (!existing->info || !existing->info->has_spacing_area) continue;
                Area existing_area = existing->info->spacing_area;
                existing_area.apply_offset(existing->pos_X, existing->pos_Y);
                if (test_area.intersects(existing_area)) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                for (Asset* sib : siblings) {
                    if (!sib->info || !sib->info->has_spacing_area) continue;
                    Area sib_area = sib->info->spacing_area;
                    sib_area.apply_offset(sib->pos_X, sib->pos_Y);
                    if (test_area.intersects(sib_area)) {
                        overlaps = true;
                        break;
                    }
                }
            }

            if (overlaps) {
                std::cerr << "[Attempt " << attempt << "] Skipped '" << type << "' at (" << pos.first << "," << pos.second << "): spacing overlap\n";
                ++attempt;
                continue;
            }
        }

        if (info->min_same_type_distance > 0) {
            bool too_close = false;
            for (const auto& existing : all_) {
                if (!existing->info || existing->info->name != type) continue;
                int dx = pos.first - existing->pos_X;
                int dy = pos.second - existing->pos_Y;
                if ((dx * dx + dy * dy) < (info->min_same_type_distance * info->min_same_type_distance)) {
                    too_close = true;
                    std::cerr << "[Attempt " << attempt << "] Skipped '" << type << "' at (" << pos.first << "," << pos.second << "): too close to another of same type\n";
                    break;
                }
            }
            if (too_close) {
                ++attempt;
                continue;
            }
        }

        auto asset = std::make_unique<Asset>(info->z_threshold, valid_area, renderer_, parent);
        asset->info = info;
        asset->set_position(pos.first, pos.second);
        asset->finalize_setup();

        std::vector<Asset*> child_siblings;
        for (const ChildAsset& child_def : info->child_assets) {
            try {
                child_def.area.get_bounds();
            } catch (...) {
                continue;
            }

            int spawn_count = std::uniform_int_distribution<int>(child_def.min, child_def.max)(rng_);
            Area child_area = child_def.area;
            child_area.apply_offset(pos.first, pos.second);

            for (int c = 0; c < spawn_count; ++c) {
                generate_asset_recursive(child_def.asset, child_area, depth + 1, asset.get(), child_siblings, child_def.terminate_with_parent);
            }
        }

        Asset* raw_asset_ptr = asset.get();
        if (depth == 0 || terminate_with_parent) {
            all_.push_back(std::move(asset));
            siblings.push_back(raw_asset_ptr);
        } else if (parent) {
            parent->add_child(std::move(*asset));
        }

        std::cout << "[AssetGenerator] Successfully placed '" << type << "' at (" << pos.first << "," << pos.second << ")\n";
        return true;
    }

    return false;
}


std::vector<std::unique_ptr<Asset>>&& AssetGenerator::extract_all_assets() {
    return std::move(all_);
}

AssetGenerator::Point AssetGenerator::get_random_point(const Area& area) {
    return invert_area_ ? get_point_outside_area(area) : get_point_within_area(area);
}

AssetGenerator::Point AssetGenerator::get_point_within_area(const Area& area) {
    auto [minx, miny, maxx, maxy] = area.get_bounds();
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::uniform_int_distribution<int> x_dist(minx, maxx);
        std::uniform_int_distribution<int> y_dist(miny, maxy);
        Point pt = { x_dist(rng_), y_dist(rng_) };
        if (area.contains_point(pt)) return pt;
    }
    return {0, 0};
}

AssetGenerator::Point AssetGenerator::get_point_outside_area(const Area& area) {
    for (int attempt = 0; attempt < 1000; ++attempt) {
        std::uniform_int_distribution<int> x_dist(0, map_width_);
        std::uniform_int_distribution<int> y_dist(0, map_height_);
        Point pt = { x_dist(rng_), y_dist(rng_) };
        if (!area.contains_point(pt)) return pt;
    }
    return {0, 0};
}
