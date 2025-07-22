#pragma once

#include "area.hpp"
#include "asset_library.hpp"
#include "asset_spawn_planner.hpp"
#include "asset.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

class Room {
public:
    using Point = std::pair<int, int>;

    Room(Point origin,
        std::string type_,
         const std::string& room_def_name,
         Room* parent,
         const std::string& room_dir,
         const std::string& map_dir,
         AssetLibrary* asset_lib,

         Area* precomputed_area);

    void set_sibling_left(Room* left_room);
    void set_sibling_right(Room* right_room);

    void add_connecting_room(Room* room);
    void remove_connecting_room(Room* room);

    void add_room_assets(std::vector<std::unique_ptr<Asset>> new_assets);
    std::vector<std::unique_ptr<Asset>>&& get_room_assets();
    void set_layer(int);
    Point map_origin;
    std::string room_name;
    std::string room_directory;
    std::string map_path;
    std::string json_path;
    Room* parent = nullptr;
    Room* left_sibling = nullptr;
    Room* right_sibling = nullptr;
    int layer = -1;
    bool testing = false;
    std::vector<Room*> children;
    std::vector<Room*> connected_rooms;
    std::vector<std::unique_ptr<Asset>> assets;

    std::unique_ptr<Area> room_area;
    std::unique_ptr<AssetSpawnPlanner> planner;
    std::string type;
private:
    nlohmann::json assets_json;
};
