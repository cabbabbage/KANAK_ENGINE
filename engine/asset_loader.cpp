#include "asset_loader.hpp"
#include "generate_trails.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

AssetLoader::AssetLoader(const std::string& map_json_path, SDL_Renderer* renderer)
    : map_path_(map_json_path),
      renderer_(renderer),
      rng_(std::random_device{}())
{
    load_json();
    generate_rooms();
    generate_trails();
    spawn_assets_for_rooms();
    spawn_assets_for_trails();
    spawn_assets_for_open_space();
    std::cout << "[AssetLoader] Spawning assets in open world...\n";
    spawn_davey_player();
    place_player_in_random_room();

    // Remove Background assets that fall within room or trail areas
    std::vector<Area> exclusion_areas;
    for (const auto& room : rooms_) exclusion_areas.push_back(room.getArea());
    for (const auto& trail : trails_) exclusion_areas.push_back(trail.area);

    auto it = all_assets_.begin();
    while (it != all_assets_.end()) {
        Asset* a = it->get();
        if (!a || !a->info || a->info->type != "Background") {
            ++it;
            continue;
        }

        bool inside = false;
        for (const auto& area : exclusion_areas) {
            if (area.contains(a->pos_X, a->pos_Y)) {
                inside = true;
                break;
            }
        }

        if (inside) {
            it = all_assets_.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetLoader::spawn_assets_for_open_space() {
    Area combined_area;
    for (const auto& room : rooms_) {
        combined_area.union_with(room.getArea());
    }
    for (const auto& trail : trails_) {
        combined_area.union_with(trail.area);
    }

    std::ifstream in("boundaries.json");
    if (!in.is_open()) {
        std::cerr << "[AssetLoader] Failed to open boundaries.json\n";
        return;
    }

    nlohmann::json assets_json;
    in >> assets_json;
    in.close();

    if (!assets_json.contains("assets") || !assets_json["assets"].is_array()) {
        std::cerr << "[AssetLoader] boundaries.json missing assets array\n";
        return;
    }

    AssetGenerator gen(combined_area, assets_json["assets"], renderer_, true, map_width_, map_height_);
    gen.generate_all();

    auto assets = gen.extract_all_assets();
    for (auto& a : assets) {
        all_assets_.push_back(std::move(a));
    }
}

void AssetLoader::load_json() {
    std::ifstream in(map_path_);
    if (!in.is_open()) {
        throw std::runtime_error("[AssetLoader] Failed to open " + map_path_);
    }
    in >> root_;
    in.close();

    map_width_        = root_.value("width", 0);
    map_height_       = root_.value("height", 0);
    min_rooms_        = root_.value("min_rooms", 1);
    max_rooms_        = root_.value("max_rooms", min_rooms_);
    min_room_h_       = root_.value("min_room_height", 0);
    max_room_h_       = root_.value("max_room_height", min_room_h_);
    min_room_w_       = root_.value("min_room_width", 0);
    max_room_w_       = root_.value("max_room_width", min_room_w_);
    min_trail_w_      = root_.value("min_trail_width", 0);
    max_trail_w_      = root_.value("max_trail_width", min_trail_w_);
}

int AssetLoader::get_screen_center_x() const { return map_width_ / 2; }
int AssetLoader::get_screen_center_y() const { return map_height_ / 2; }

void AssetLoader::generate_rooms() {
    std::uniform_int_distribution<int> room_count_dist(min_rooms_, max_rooms_);
    int room_count = room_count_dist(rng_);

    for (int i = 0; i < room_count; ++i) {
        std::vector<GenerateRoom*> existing_ptrs;
        for (auto& r : rooms_) {
            existing_ptrs.push_back(&r);
        }

        std::uniform_int_distribution<int> hr_dist(min_room_h_, max_room_h_);
        std::uniform_int_distribution<int> wr_dist(min_room_w_, max_room_w_);
        int room_h = hr_dist(rng_);
        int room_w = wr_dist(rng_);
        int radius = (room_h + room_w) / 2;

        rooms_.emplace_back(existing_ptrs, map_width_, map_height_, radius, radius);
    }
}

void AssetLoader::generate_trails() {
    GenerateTrails gt(rooms_, map_width_, map_height_);
    trails_ = gt.getTrails();
}

void AssetLoader::spawn_assets_for_rooms() {
    for (auto& gr : rooms_) {
        std::ifstream in(gr.getAssetsPath());
        if (!in.is_open()) continue;

        nlohmann::json assets_json;
        in >> assets_json;
        in.close();

        if (!assets_json.contains("assets") || !assets_json["assets"].is_array()) continue;

        AssetGenerator gen(gr.getArea(), assets_json["assets"], renderer_, false, map_width_, map_height_);
        gen.generate_all();

        auto assets = gen.extract_all_assets();
        for (auto& a : assets) {
            all_assets_.push_back(std::move(a));
        }
    }
}

void AssetLoader::spawn_assets_for_trails() {
    for (const auto& trail : trails_) {
        std::ifstream in(trail.assets_path);
        if (!in.is_open()) continue;

        nlohmann::json assets_json;
        in >> assets_json;
        in.close();

        if (!assets_json.contains("assets") || !assets_json["assets"].is_array()) continue;

        AssetGenerator gen(trail.area, assets_json["assets"], renderer_, false, map_width_, map_height_);
        gen.generate_all();

        auto assets = gen.extract_all_assets();
        for (auto& a : assets) {
            all_assets_.push_back(std::move(a));
        }
    }
}

void AssetLoader::spawn_davey_player() {
    if (rooms_.empty()) return;

    const GenerateRoom& target_room = rooms_.front();
    Area room_area = target_room.getArea();

    std::ifstream in("player.json");
    if (!in.is_open()) {
        std::cerr << "[AssetLoader] Failed to open player.json\n";
        return;
    }

    nlohmann::json player_json;
    in >> player_json;
    in.close();

    if (!player_json.contains("assets") || !player_json["assets"].is_array()) {
        std::cerr << "[AssetLoader] player.json missing assets array\n";
        return;
    }

    AssetGenerator gen(room_area, player_json["assets"], renderer_, false, map_width_, map_height_);
    gen.generate_all();

    auto assets = gen.extract_all_assets();
    if (!assets.empty()) {
        std::unique_ptr<Asset> player_asset = std::move(assets[0]);
        player_ = player_asset.get();
        all_assets_.push_back(std::move(player_asset));
    } else {
        std::cerr << "[AssetLoader] Failed to spawn player from player.json\n";
    }
}

void AssetLoader::place_player_in_random_room() {
    if (!player_ || rooms_.empty()) return;

    std::uniform_int_distribution<size_t> room_dist(0, rooms_.size() - 1);
    size_t idx = room_dist(rng_);
    const GenerateRoom& chosen = rooms_[idx];
    int px = chosen.getCenterX();
    int py = chosen.getCenterY();

    player_->set_position(px, py);
}

const std::vector<std::unique_ptr<Asset>>& AssetLoader::get_all_assets() const {
    return all_assets_;
}

Asset* AssetLoader::get_player() const {
    return player_;
}
