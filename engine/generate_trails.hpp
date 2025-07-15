#pragma once

#include "Area.hpp"
#include "room.hpp"
#include "Asset.hpp"
#include "asset_loader.hpp"

#include <vector>
#include <string>
#include <random>
#include <memory>

class GenerateTrails {
public:
    using Point = std::pair<double, double>;

    GenerateTrails(const std::string& trail_dir);

    std::vector<std::unique_ptr<Room>> generate_trails(
        const std::vector<std::pair<Room*, Room*>>& room_pairs,
        const std::vector<Area>& existing_areas,
        const std::string& map_dir,
        AssetLibrary* asset_lib);

private:
    std::vector<std::string> available_assets_;
    std::vector<Area> trail_areas_;
    std::mt19937 rng_;

    std::string pick_random_asset();
    std::vector<Point> build_centerline(const Point& start, const Point& end, int curvyness);
    std::vector<Point> extrude_centerline(const std::vector<Point>& centerline, double width);
};
