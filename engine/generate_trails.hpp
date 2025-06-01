#ifndef GENERATE_TRAILS_HPP
#define GENERATE_TRAILS_HPP

#include "generate_room.hpp"
#include "area.hpp"
#include <vector>
#include <string>
#include <random>

class GenerateTrails {
public:
    using Point = std::pair<int, int>;

    struct Trail {
        Area area;
        std::string assets_path;
    };

    GenerateTrails(const std::vector<GenerateRoom>& rooms,
                   int map_width,
                   int map_height);

    const std::vector<Trail>& getTrails() const;

private:
    std::vector<Trail> trails_;
    std::vector<std::string> available_assets_;
    std::mt19937 rng_;
    int map_width_;
    int map_height_;

    std::vector<Point> buildPolygon(const Point& start, const Point& end);
    std::string pickAssetsPath();
};

#endif // GENERATE_TRAILS_HPP
