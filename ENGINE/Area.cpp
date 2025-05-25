#include "area.hpp"
#include <algorithm>

Area::Area() = default;

Area::Area(const std::vector<Point>& pts)
    : points(pts) {}

Area::Area(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open area file: " + json_path);
    }

    nlohmann::json data;
    file >> data;

    if (!data.is_array() || data.empty()) {
        throw std::runtime_error("Invalid or empty JSON array in area file: " + json_path);
    }

    for (const auto& pt : data) {
        if (pt.is_array() && pt.size() == 2 &&
            pt[0].is_number_integer() && pt[1].is_number_integer()) {
            points.emplace_back(pt[0].get<int>(), pt[1].get<int>());
        }
    }

    if (points.empty()) {
        throw std::runtime_error("No valid points found in area file: " + json_path);
    }
}

void Area::apply_offset(int dx, int dy) {
    for (auto& pt : points) {
        pt.first += dx;
        pt.second += dy;
    }
}

void Area::set_position(int x, int y) {
    if (points.empty()) return;
    int dx = x - points[0].first;
    int dy = y - points[0].second;
    apply_offset(dx, dy);
}

std::tuple<int, int, int, int> Area::get_bounds() const {
    if (points.empty()) {
        throw std::out_of_range("[Area] get_bounds() called on empty points");
    }

    int min_x = points[0].first;
    int max_x = points[0].first;
    int min_y = points[0].second;
    int max_y = points[0].second;

    for (const auto& [x, y] : points) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }

    return {min_x, min_y, max_x, max_y};
}

bool Area::intersects(const Area& other) const {
    if (points.empty() || other.points.empty()) return false;

    auto [min_x1, min_y1, max_x1, max_y1] = this->get_bounds();
    auto [min_x2, min_y2, max_x2, max_y2] = other.get_bounds();

    bool overlap_x = (min_x1 <= max_x2) && (max_x1 >= min_x2);
    bool overlap_y = (min_y1 <= max_y2) && (max_y1 >= min_y2);

    return overlap_x && overlap_y;
}


bool Area::contains_point(const std::pair<int, int>& pt) const {
    int x = pt.first;
    int y = pt.second;
    bool inside = false;

    size_t n = points.size();
    if (n < 3) return false;

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        int xi = points[i].first, yi = points[i].second;
        int xj = points[j].first, yj = points[j].second;

        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / static_cast<float>(yj - yi + 1e-9) + xi);
        if (intersect) inside = !inside;
    }

    return inside;
}
