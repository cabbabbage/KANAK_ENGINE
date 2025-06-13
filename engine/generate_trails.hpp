// === File: generate_trails.hpp ===
#ifndef GENERATE_TRAILS_HPP
#define GENERATE_TRAILS_HPP

#include "area.hpp"
#include "generate_room.hpp"
#include <string>
#include <vector>
#include <random>
#include <utility>

class GenerateTrails {
public:
    using Point = std::pair<int, int>;

    GenerateTrails(std::string map_path,
                   const std::vector<GenerateRoom>& rooms,
                   int map_width,
                   int map_height);

    const std::vector<Area>& getTrailAreas() const;
    std::string pickAssetsPath();

private:
    std::string map_path;
    std::vector<std::string> available_assets_;
    std::vector<Area> trail_areas_;
    int map_width_;
    int map_height_;
    std::mt19937 rng_;

    std::vector<Point> buildCenterline(const Point& start,
                                       const Point& end,
                                       int curvyness);
    std::vector<Point> extrudeToWidth(const std::vector<Point>& centerline,
                                      double width);
    int countTrailIntersections(const Point& start, const Point& end);
    void addTrail(const Point& start, const Point& end);
};

#endif // GENERATE_TRAILS_HPP
