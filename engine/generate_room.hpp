// File: generate_room.hpp
#ifndef GENERATE_ROOM_HPP
#define GENERATE_ROOM_HPP

#include <string>
#include <vector>
#include <random>
#include <SDL.h>
#include "Area.hpp"

class GenerateRoom {
public:
    using Point = std::pair<int,int>;

    // map_path: base directory for map files
    // existing_rooms: pointers to already-generated rooms for overlap checks
    GenerateRoom(std::string map_path,
                 const std::vector<GenerateRoom*>& existing_rooms,
                 int map_width,
                 int map_height,
                 const std::string& json_path,
                 SDL_Renderer* renderer);

    const Area& getArea() const;
    int getCenterX() const;
    int getCenterY() const;
    bool isSpawn() const;
    bool isBoss() const;
    Point getPointInside() const;
    bool inherits;

private:
    std::string map_path;
    std::string assets_path_;
    int map_width_;
    int map_height_;
    SDL_Renderer* renderer_;
    Area room_area_;
    int center_x_;
    int center_y_;
    bool is_spawn_;
    bool is_boss_;

    static std::mt19937 rng_;
};

#endif // GENERATE_ROOM_HPP
