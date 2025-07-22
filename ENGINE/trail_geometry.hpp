// === File: trail_geometry.hpp ===
#ifndef TRAIL_GEOMETRY_HPP
#define TRAIL_GEOMETRY_HPP

#include <vector>
#include <random>
#include <string>
#include <memory>
#include "Area.hpp"
#include "Room.hpp"

class AssetLibrary;

class TrailGeometry {
public:
    using Point = std::pair<double, double>;

    TrailGeometry();

    std::vector<Point> build_centerline(const Point& start, const Point& end, int curvyness);
    std::vector<Point> extrude_centerline(const std::vector<Point>& centerline, double width);

    static bool attempt_trail_connection(
        Room* a,
        Room* b,
        std::vector<Area>& existing_areas,
        const std::string& map_dir,
        AssetLibrary* asset_lib,
        std::vector<std::unique_ptr<Room>>& trail_rooms,
        int allowed_intersections,
        const std::string& path,
        bool testing,
        std::mt19937& rng
    );

private:
    std::mt19937 rng_;
};

#endif // TRAIL_GEOMETRY_HPP
