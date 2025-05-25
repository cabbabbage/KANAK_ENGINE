#ifndef AREA_HPP
#define AREA_HPP

#include <vector>
#include <utility>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

class Area {
public:
    using Point = std::pair<int, int>;
    std::vector<Point> points;

    Area() = default;

    // Initialize from list of points
    Area(const std::vector<Point>& pts) {
        points = pts;
    }

    // Initialize from JSON file containing an array of [x, y] points
    Area(const std::string& json_path) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open area file: " + json_path);
        }

        nlohmann::json data;
        file >> data;

        for (const auto& pt : data) {
            if (pt.size() == 2) {
                points.emplace_back(pt[0], pt[1]);
            }
        }
    }

    // Offset all points by (dx, dy)
    void apply_offset(int dx, int dy) {
        for (auto& pt : points) {
            pt.first += dx;
            pt.second += dy;
        }
    }

    // Optional: reset to a different base position
    void set_position(int x, int y) {
        if (points.empty()) return;

        int offset_x = x - points[0].first;
        int offset_y = y - points[0].second;
        apply_offset(offset_x, offset_y);
    }
};

#endif
