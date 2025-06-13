#include "asset_loader.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <random>

namespace fs = std::filesystem;

AssetLoader::AssetLoader(const std::string& map_dir, SDL_Renderer* renderer)
    : map_path_(map_dir), renderer_(renderer), rng_(std::random_device{}())
{

    asset_library_ = std::make_unique<AssetLibrary>(renderer_);
    std::ifstream in(map_path_ + "/map_info.json");
    if (!in.is_open()) throw std::runtime_error("Could not open map_info.json");

    nlohmann::json root;
    in >> root;
    in.close();

    map_width_ = root["map_width_"];
    map_height_ = root["map_height_"];
    int min_rooms = root["min_rooms"];
    int max_rooms = root["max_rooms"];
    const std::string map_boundary_file = root["map_boundary"];
    const auto& room_templates = root["rooms"];

    std::uniform_int_distribution<int> room_dist(min_rooms, max_rooms);
    int target_room_count = room_dist(rng_);

    std::unordered_map<std::string, int> room_counts;
    std::vector<std::string> room_paths;


    std::ifstream jin(map_path_ + "/" + map_boundary_file);
    if (jin.is_open()) {
        nlohmann::json aset;
        jin >> aset;
        if (aset.contains("assets")) {
            int scaled_width = static_cast<int>(map_width_ * 1.5);
            int scaled_height = static_cast<int>(map_height_ * 1.5);
            int map_cx = map_width_ / 2;
            int map_cy = map_height_ / 2;

            Area oversized_area(map_cx, map_cy, scaled_width, scaled_height, "Square", 1, map_width_, map_height_);

            auto [minx, miny, maxx, maxy] = oversized_area.get_bounds();
            int area_cx = (minx + maxx) / 2;
            int area_cy = (miny + maxy) / 2;
            int dx = map_cx - area_cx;
            int dy = map_cy - area_cy;
            oversized_area.apply_offset(dx, dy);

            AssetGenerator gen(oversized_area, aset, renderer_, map_width_, map_height_, asset_library_.get(), true);
   
            auto result = gen.extract_all_assets();

            for (auto& asset_ptr : result) {
                if (asset_ptr) {
                    all_assets_.push_back(std::move(*asset_ptr));
                }
            }
        }
    }


    for (const auto& entry : room_templates) {
        if (entry.value("required", false)) {
            std::string path = entry["room_path"];
            if (room_counts[path] == 0) {
                room_paths.push_back(path);
                room_counts[path]++;
            }
        }
    }

    std::vector<nlohmann::json> candidates;
    for (const auto& entry : room_templates) {
        std::string path = entry["room_path"];
        int max_inst = entry["max_instances"];
        if (room_counts[path] < max_inst) {
            candidates.push_back(entry);
        }
    }

    while ((int)room_paths.size() < target_room_count && !candidates.empty()) {
        std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
        size_t idx = pick(rng_);
        const auto& entry = candidates[idx];

        std::string path = entry["room_path"];
        int max_inst = entry["max_instances"];

        if (++room_counts[path] <= max_inst) {
            room_paths.push_back(path);
        }

        if (room_counts[path] >= max_inst) {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [&](const nlohmann::json& e) {
                    return e["room_path"] == path;
                }), candidates.end());
        }
    }

    for (const std::string& path : room_paths) {
        std::vector<GenerateRoom*> existing_ptrs;
        for (auto& r : rooms_) existing_ptrs.push_back(&r);
        GenerateRoom new_room(map_path_, existing_ptrs, map_width_, map_height_, map_path_ + "/" + path, renderer_, asset_library_.get());

        auto room_assets = new_room.getAssets();
        for (auto& asset_ptr : room_assets) {
            if (asset_ptr) {
                all_assets_.push_back(std::move(*asset_ptr));
            }
        }

        rooms_.push_back(std::move(new_room));
    }

    trail_gen_ = std::make_unique<GenerateTrails>(map_path_, rooms_, map_width_, map_height_);

    for (const auto& trail_area : trail_gen_->getTrailAreas()) {
        std::string picked_path;

        try {
            picked_path = trail_gen_->pickAssetsPath();
        } catch (const std::exception& e) {
            continue;
        }

        std::ifstream trail_in(picked_path);
        if (!trail_in.is_open()) continue;

        nlohmann::json config;
        trail_in >> config;
        trail_in.close();

        AssetGenerator gen(trail_area, config, renderer_, map_width_, map_height_, asset_library_.get());
        auto generated = gen.extract_all_assets();

        for (auto& asset_ptr : generated) {
            if (asset_ptr) {
                all_assets_.push_back(std::move(*asset_ptr));
            }
        }
    }

    Area cumulative_area;
    for (const auto& room : rooms_) {
        cumulative_area.union_with(room.getArea());
    }
    for (const auto& trail_area : trail_gen_->getTrailAreas()) {
        cumulative_area.union_with(trail_area);
    }



    std::vector<Area> exclusion_zones;
    for (const auto& room : rooms_) {
        exclusion_zones.push_back(room.getArea());
    }
    for (const auto& trail_area : trail_gen_->getTrailAreas()) {
        exclusion_zones.push_back(trail_area);
    }

    for (auto it = all_assets_.begin(); it != all_assets_.end(); ) {
        if (it->info && it->info->type == "Background") {
            bool inside_any = false;
            for (const Area& zone : exclusion_zones) {
                if (zone.contains(it->pos_X, it->pos_Y)) {
                    inside_any = true;
                    break;
                }
            }
            if (inside_any) {
                Asset* bg_ptr = &(*it);
                for (auto child_it = all_assets_.begin(); child_it != all_assets_.end(); ) {
                    if (child_it->parent == bg_ptr) {
                        child_it = all_assets_.erase(child_it);
                    } else {
                        ++child_it;
                    }
                }
                it = all_assets_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

const std::vector<Asset>& AssetLoader::get_all_assets() const {
    return all_assets_;
}

std::vector<Asset> AssetLoader::extract_all_assets() {
    return std::move(all_assets_);
}

int AssetLoader::get_screen_center_x() const {
    return map_width_ / 2;
}

int AssetLoader::get_screen_center_y() const {
    return map_height_ / 2;
}

std::vector<Area> AssetLoader::getAllRoomAndTrailAreas() const {
    std::vector<Area> result;
    for (const auto& room : rooms_) {
        result.push_back(room.getArea());
    }
    for (const auto& trail : trail_gen_->getTrailAreas()) {
        result.push_back(trail);
    }
    return result;
}

Asset* AssetLoader::get_player() {
    for (auto& asset : all_assets_) {
        if (!asset.info) {
            continue;
        }
        if (asset.info->type == "Player") {
            return &asset;
        }
    }
    return nullptr;
}
