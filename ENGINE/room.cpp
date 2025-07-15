#include "room.hpp"
#include "asset_spawner.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

Room::Room(Point origin,
           const std::string& room_def_name,
           Room* parent,
           const std::string& room_dir,
           const std::string& map_dir,
           AssetLibrary* asset_lib,
           Area* precomputed_area)
    : map_origin(origin),
      parent(parent),
      room_name(room_def_name),
      room_directory(room_dir),
      map_path(map_dir),
      json_path(room_dir + "/" + room_def_name + ".json")
{
    std::cout << "[Room] Created room: " << room_name
              << " at (" << origin.first << ", " << origin.second << ")"
              << (parent ? " with parent\n" : " (no parent)\n");

    // === Load room definition JSON ===
    std::ifstream in(json_path);
    if (!in.is_open()) {
        throw std::runtime_error("[Room] Failed to open room JSON: " + json_path);
    }
    json J;
    in >> J;
    assets_json = J;

    // === Load map_radius from map_info.json ===
    int map_radius = 0;
    {
        std::ifstream minf(map_path + "/map_info.json");
        if (minf.is_open()) {
            json m;
            minf >> m;
            map_radius = m.value("map_radius", 0);
        } else {
            std::cerr << "[Room] Warning: could not open map_info.json at " << map_path << "\n";
        }
    }
    int map_w = map_radius * 2;
    int map_h = map_radius * 2;

    if (precomputed_area) {
        std::cout << "[Room] Using precomputed area for: " << room_name << "\n";
        room_area = *precomputed_area;
    } else {
        int min_w = J.value("min_width", 64);
        int max_w = J.value("max_width", 64);
        int min_h = J.value("min_height", 64);
        int max_h = J.value("max_height", 64);
        int edge_smoothness = J.value("edge_smoothness", 2);
        std::string geometry = J.value("geometry", "square");
        if (!geometry.empty()) geometry[0] = std::toupper(geometry[0]);

        static std::mt19937 rng(std::random_device{}());
        int width = std::uniform_int_distribution<>(min_w, max_w)(rng);
        int height = std::uniform_int_distribution<>(min_h, max_h)(rng);

        std::cout << "[Room] Creating area from JSON: " << room_name
                  << " (" << width << "x" << height << ")"
                  << " at (" << map_origin.first << ", " << map_origin.second << ")"
                  << ", geometry: " << geometry
                  << ", map radius: " << map_radius << "\n";

        room_area = Area(map_origin.first,
                         map_origin.second,
                         width,
                         height,
                         geometry,
                         edge_smoothness,
                         map_w,
                         map_h);
    }

    planner = std::make_unique<AssetSpawnPlanner>(
        assets_json,
        room_area.get_area(),
        *asset_lib
    );

    std::vector<Area> exclusion;
    AssetSpawner spawner(asset_lib, exclusion);
    spawner.spawn(*this);
}

void Room::set_sibling_left(Room* left_room) {
    left_sibling = left_room;
}

void Room::set_sibling_right(Room* right_room) {
    right_sibling = right_room;
}

void Room::add_connecting_room(Room* room) {
    if (room && std::find(connected_rooms.begin(), connected_rooms.end(), room) == connected_rooms.end()) {
        connected_rooms.push_back(room);
    }
}

void Room::remove_connecting_room(Room* room) {
    auto it = std::find(connected_rooms.begin(), connected_rooms.end(), room);
    if (it != connected_rooms.end()) connected_rooms.erase(it);
}

void Room::add_room_assets(std::vector<std::unique_ptr<Asset>> new_assets) {
    for (auto& asset : new_assets)
        assets.push_back(std::move(asset));
}

std::vector<std::unique_ptr<Asset>>&& Room::get_room_assets() {
    return std::move(assets);
}
