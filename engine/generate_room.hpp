#ifndef GENERATE_ROOM_HPP
#define GENERATE_ROOM_HPP

#include "area.hpp"
#include <string>
#include <vector>
#include <random>

class GenerateRoom {
public:
    using Point = std::pair<int, int>;

    GenerateRoom(const std::vector<GenerateRoom*>& existing_rooms,
                 int map_width,
                 int map_height,
                 int min_size,
                 int max_size,
                 const std::string& optional_assets_json = "");

    int getCenterX() const;
    int getCenterY() const;
    const Area& getArea() const;
    const std::string& getAssetsPath() const;
    Point getPointInside() const;

private:
    void buildArea(int radius);
    void pickAssetsPath(const std::vector<GenerateRoom*>& existing_rooms,
                        const std::string& optional_assets_json);

    int map_width_;
    int map_height_;
    int center_x_;
    int center_y_;
    Area room_area_;
    std::string assets_path_;

    static std::mt19937 rng_;
};

#endif // GENERATE_ROOM_HPP
