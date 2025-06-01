#include "generate_room.hpp"
#include <filesystem>
#include <cmath>
#include <iostream>

std::mt19937 GenerateRoom::rng_(std::random_device{}());

GenerateRoom::GenerateRoom(const std::vector<GenerateRoom*>& existing_rooms,
                           int map_width,
                           int map_height,
                           int min_size,
                           int max_size,
                           const std::string& optional_assets_json)
    : map_width_(map_width),
      map_height_(map_height)
{
    std::uniform_int_distribution<int> radius_dist(min_size, max_size);
    const double min_dist = 0.2 * ((map_width_ + map_height_) / 2.0);

    while (true) {
        int radius = radius_dist(rng_);
        std::uniform_int_distribution<int> x_dist(radius, map_width_ - radius);
        std::uniform_int_distribution<int> y_dist(radius, map_height_ - radius);
        center_x_ = x_dist(rng_);
        center_y_ = y_dist(rng_);
        buildArea(radius);

        bool invalid = false;
        for (const auto* other : existing_rooms) {
            if (room_area_.intersects(other->getArea())) {
                invalid = true;
                break;
            }

            const auto& other_bounds = other->getArea().get_points();
            for (const auto& pt : room_area_.get_points()) {
                for (const auto& opt : other_bounds) {
                    double dx = pt.first - opt.first;
                    double dy = pt.second - opt.second;

                    double dist = std::hypot(dx, dy);
                    if (dist < min_dist) {
                        invalid = true;
                        break;
                    }
                }
                if (invalid) break;
            }
            if (invalid) break;
        }

        if (!invalid) break;
    }

    pickAssetsPath(existing_rooms, optional_assets_json);
}

void GenerateRoom::buildArea(int radius) {
    room_area_ = Area(center_x_, center_y_, radius, map_width_, map_height_);
}

void GenerateRoom::pickAssetsPath(const std::vector<GenerateRoom*>& existing_rooms,
                                  const std::string& optional_assets_json)
{
    if (!optional_assets_json.empty()) {
        assets_path_ = optional_assets_json;
        return;
    }

    std::vector<std::string> candidates;
    for (const auto& entry : std::filesystem::directory_iterator("rooms")) {
        if (!entry.is_regular_file()) continue;
        auto full = entry.path().string();
        auto fname = entry.path().filename().string();
        if (fname.size() >= 5 && fname.substr(fname.size() - 5) == ".json") {
            candidates.push_back(full);
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error("[GenerateRoom] No JSON files found in 'rooms/' to assign.");
    }

    std::uniform_int_distribution<size_t> index_dist(0, candidates.size() - 1);
    assets_path_ = candidates[index_dist(rng_)];
}

int GenerateRoom::getCenterX() const {
    return center_x_;
}

int GenerateRoom::getCenterY() const {
    return center_y_;
}

const Area& GenerateRoom::getArea() const {
    return room_area_;
}

const std::string& GenerateRoom::getAssetsPath() const {
    return assets_path_;
}

GenerateRoom::Point GenerateRoom::getPointInside() const {
    auto [minx, miny, maxx, maxy] = room_area_.get_bounds();
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::uniform_int_distribution<int> x_dist(minx, maxx);
        std::uniform_int_distribution<int> y_dist(miny, maxy);
        Point pt = { x_dist(rng_), y_dist(rng_) };
        if (room_area_.contains_point(pt)) return pt;
    }
    return {center_x_, center_y_};
}
