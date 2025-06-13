// generate_room.hpp
#ifndef GENERATE_ROOM_HPP
#define GENERATE_ROOM_HPP

#include "area.hpp"
#include "asset.hpp"
#include <vector>
#include <string>
#include <random>
#include <memory>
#include <SDL.h>

class AssetLibrary;

class GenerateRoom {
public:
    using Point = std::pair<int, int>;

    GenerateRoom(std::string map_path,
                const std::vector<GenerateRoom*>& existing_rooms,
                int map_width,
                int map_height,
                const std::string& json_path,
                SDL_Renderer* renderer,
                AssetLibrary* asset_library);

    AssetLibrary* asset_library_;

    int getCenterX() const;
    int getCenterY() const;
    const Area& getArea() const;
    const std::string& getAssetsPath() const;
    Point getPointInside() const;
    std::vector<std::unique_ptr<Asset>> getAssets();  // changed from &&

    bool is_spawn_ = false;
    bool is_boss_ = false;

    bool isSpawn() const;
    bool isBoss() const;

private:
    std::string map_path;
    std::string assets_path_;
    int map_width_;
    int map_height_;
    int center_x_;
    int center_y_;
    Area room_area_;
    SDL_Renderer* renderer_;
    std::vector<std::unique_ptr<Asset>> room_assets_;
    static std::mt19937 rng_;
};

#endif // GENERATE_ROOM_HPP
