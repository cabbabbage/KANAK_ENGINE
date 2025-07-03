// asset_loader.cpp

#include "asset_loader.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <random>

AssetLoader::AssetLoader(const std::string& map_dir, SDL_Renderer* renderer)
    : map_path_(map_dir),
      renderer_(renderer),
      rng_(std::random_device{}())
{
    asset_library_ = std::make_unique<AssetLibrary>(renderer_);

    // Load map_info.json
    std::ifstream in(map_path_ + "/map_info.json");
    if (!in.is_open()) throw std::runtime_error("Could not open map_info.json");
    nlohmann::json root;
    in >> root;
    in.close();

    map_width_          = root["map_width_"];
    map_height_         = root["map_height_"];
    int min_rooms       = root["min_rooms"];
    int max_rooms       = root["max_rooms"];
    map_boundary_file_  = root["map_boundary"].get<std::string>();
    room_templates_     = root["rooms"];

    std::uniform_int_distribution<int> room_dist(min_rooms, max_rooms);
    target_room_count_ = room_dist(rng_);

    // Single shared spawner instance
    spawner_ = std::make_unique<AssetSpawner>(
        renderer_, map_width_, map_height_, asset_library_.get()
    );


    std::ifstream jin2(map_path_ + "/map_assets.json");
    nlohmann::json mapConfig;
    jin2 >> mapConfig;


// === ROOM Assets ===
std::cout << "Generating ROOM Assets\n";
{
    std::unordered_map<std::string,int> room_counts;
    std::vector<std::string> room_paths;

    // Required rooms
    for (auto& e : room_templates_) {
        if (e.value("required", false)) {
            std::string p = e["room_path"].get<std::string>();
            if (room_counts[p]++ == 0) room_paths.push_back(p);
        }
    }

    // Candidates for random fill
    std::vector<nlohmann::json> candidates(room_templates_.begin(),
                                            room_templates_.end());
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
        [&](auto& e) {
            return room_counts[e["room_path"]] >=
                   e["max_instances"].get<int>();
        }), candidates.end());

    // Fill up to target count
    while ((int)room_paths.size() < target_room_count_ && !candidates.empty()) {
        size_t idx = std::uniform_int_distribution<size_t>(
            0, candidates.size() - 1
        )(rng_);
        auto& e = candidates[idx];
        std::string p = e["room_path"].get<std::string>();
        if (++room_counts[p] <= e["max_instances"].get<int>()) {
            room_paths.push_back(p);
        }
        if (room_counts[p] >= e["max_instances"].get<int>()) {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [&](auto& v){ return v["room_path"] == p; }),
                candidates.end());
        }
    }

    std::cout << "Rooms to generate (" << room_paths.size() << "):\n";
    for (auto& p : room_paths) {
        std::cout << "  - " << p << "\n";
    }

    // Spawn assets for each room and save each GenerateRoom
    for (auto& p : room_paths) {
        // collect existing room pointers
        std::vector<GenerateRoom*> existing_ptrs;
        for (auto& r : rooms_) existing_ptrs.push_back(&r);

        // create and store the new room
        rooms_.emplace_back(
            map_path_,
            existing_ptrs,
            map_width_,
            map_height_,
            map_path_ + "/" + p,
            renderer_
        );
        const Area& area = rooms_.back().getArea();

        // load the room's JSON configuration
        std::ifstream jin(map_path_ + "/" + p);
        if (!jin.is_open()) continue;
        nlohmann::json roomConfig;
        jin >> roomConfig;

        // spawn the assets into that area
        spawner_->spawn(area, roomConfig, map_path_, p);
        if (!rooms_.back().inherits){
            

            spawner_->spawn(area, mapConfig, map_path_, p);

        }
    }
}



    // === TRAIL Assets ===
    std::cout << "Generating TRAIL Assets\n";
    trail_gen_ = std::make_unique<GenerateTrails>(
        map_path_, rooms_, map_width_, map_height_);
    for (auto& area : trail_gen_->getTrailAreas()) {
        std::string cfg;
        try { cfg = trail_gen_->pickAssetsPath(); }
        catch (...) { continue; }
        std::ifstream tin(cfg);
        if (!tin.is_open()) continue;
        nlohmann::json config;
        tin >> config;
        tin.close();

        spawner_->spawn(area, config, map_path_, "TRAILS");
        spawner_->spawn(area, mapConfig, map_path_, "TRAILS");
    }

        // === BOUNDARY Assets ===
    std::cout << "Generating BOUNDARY Assets\n";
    {
        std::ifstream jin(map_path_ + "/" + map_boundary_file_);
        if (jin.is_open()) {
            nlohmann::json aset;
            jin >> aset;
            jin.close();
            if (aset.contains("assets")) {
                int scaled_w = static_cast<int>(map_width_ * 1.5);
                int scaled_h = static_cast<int>(map_height_ * 1.5);
                int cx = map_width_ / 2, cy = map_height_ / 2;
                Area area(cx, cy, scaled_w, scaled_h, "Square", 1, map_width_, map_height_);
                auto [minx, miny, maxx, maxy] = area.get_bounds();
                area.apply_offset(cx - (minx + maxx) / 2,
                                  cy - (miny + maxy) / 2);

                spawner_->spawn(area, aset, map_path_, "BOUNDARY");
            }
        }
    }

}

std::vector<Asset> AssetLoader::extract_all_assets() {
    // grab all spawned assets
    auto ptrs = spawner_->extract_all_assets();  // vector<unique_ptr<Asset>>
    // build exclusion zones from rooms and trails
    auto exclusionZones = getAllRoomAndTrailAreas();
    std::vector<Asset> out;
    out.reserve(ptrs.size());

    for (auto& up : ptrs) {
        Asset* a = up.get();
        // if this is a boundary/background asset, skip it when inside any exclusion zone
        if (a->info && a->info->type == "Background") {
            bool insideAny = false;
            for (const auto& zone : exclusionZones) {
                if (zone.contains(a->pos_X, a->pos_Y)) {
                    insideAny = true;
                    break;
                }
            }
            if (insideAny) continue;
        }
        // otherwise move it into the output list
        out.push_back(std::move(*up));
    }

    return out;
}


int AssetLoader::get_screen_center_x() const {
    return map_width_ / 2;
}

int AssetLoader::get_screen_center_y() const {
    return map_height_ / 2;
}

std::vector<Area> AssetLoader::getAllRoomAndTrailAreas() const {
    std::vector<Area> areas;
    for (auto& r : rooms_) areas.push_back(r.getArea());
    for (auto& a : trail_gen_->getTrailAreas()) areas.push_back(a);
    return areas;
}
