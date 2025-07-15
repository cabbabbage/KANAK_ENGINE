#pragma once

#include "Area.hpp"
#include "Asset.hpp"
#include "asset_Info.hpp"
#include "Asset_Library.hpp"
#include "asset_spawn_planner.hpp"

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <SDL.h>

class Room {
public:
    using Point = std::pair<int, int>;

    Room(Point origin,
        const std::string& room_def_name,
        Room* parent,
        const std::string& room_dir,
        const std::string& map_dir,
        AssetLibrary* asset_lib,
        Area* precomputed_area = nullptr);


    std::string type; //new
    std::vector<Room> trails; //new
    
    void set_sibling_left(Room* left_room);
    void set_sibling_right(Room* right_room);

    void add_connecting_room(Room* room);
    void remove_connecting_room(Room* room);

    void add_room_assets(std::vector<std::unique_ptr<Asset>> new_assets);
    std::vector<std::unique_ptr<Asset>>&& get_room_assets();

    // Public fields
    Point map_origin;
    Area room_area;
    std::string room_name;
    std::string map_path;
    std::string room_directory;
    std::string json_path;
    nlohmann::json assets_json;

    int layer = 0;
    Room* parent = nullptr;
    Room* left_sibling = nullptr;
    Room* right_sibling = nullptr;

    std::vector<Room*> children;
    std::vector<Room*> connected_rooms;

    std::unique_ptr<AssetSpawnPlanner> planner;

    std::vector<std::unique_ptr<Asset>> assets;
private:

};




