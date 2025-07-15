#include "area.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>


namespace fs = std::filesystem;

Area::Area() : pos_X(0), pos_Y(0) {}

Area::Area(const std::vector<Point>& pts)
    : points(pts)
{
    if (!points.empty()) {
        auto [minx, miny, maxx, maxy] = get_bounds();
        pos_X = (minx + maxx) / 2;
        pos_Y = maxy;
    }
}



Area::Area(int center_x, int center_y, int w, int h,
           const std::string& geometry,
           int edge_smoothness,
           int map_width, int map_height)
{
    if (w <= 0 || h <= 0 || map_width <= 0 || map_height <= 0) {
        throw std::runtime_error("[Area] Invalid dimensions in Area constructor");
    }

    if (geometry == "Circle") {
        generate_circle(center_x, center_y, w/2, edge_smoothness, map_width, map_height);
    } else if (geometry == "Square") {
        generate_square(center_x, center_y, w, h, edge_smoothness, map_width, map_height);
    } else if (geometry == "Random") {
        generate_random(center_x, center_y, w, h, edge_smoothness, map_width, map_height);
    } else {
        throw std::runtime_error("[Area] Unknown geometry: " + geometry);
    }

    // After generating points, compute bounds and set anchor at bottom-center
    auto [minx, miny, maxx, maxy] = get_bounds();
    pos_X = (minx + maxx) / 2;
    pos_Y = maxy;
}



Area::Area(const std::string& json_path, float scale)
{
    if (scale <= 0.0f)
        throw std::invalid_argument("[Area] 'scale' must be positive");

    std::ifstream in(json_path);
    if (!in.is_open())
        throw std::runtime_error("[Area] Failed to open JSON: " + json_path);

    nlohmann::json j;
    in >> j;
    in.close();

    auto& pts_json = j.at("points");
    auto& dim_json = j.at("original_dimensions");
    if (!pts_json.is_array() || !dim_json.is_array() || dim_json.size() != 2)
        throw std::runtime_error("[Area] Bad JSON in: " + json_path);

    int orig_w = dim_json[0].get<int>();
    int orig_h = dim_json[1].get<int>();
    if (orig_w <= 0 || orig_h <= 0)
        throw std::runtime_error("[Area] Invalid 'original_dimensions'");

    // pivot of trimmed image, bottom-center:
    int pivot_x = static_cast<int>(std::round((orig_w / 2.0f) * scale));
    int pivot_y = static_cast<int>(std::round(orig_h         * scale));

    points.clear();
    points.reserve(pts_json.size());
    for (auto& elem : pts_json) {
        if (!elem.is_array() || elem.size() < 2) continue;
        float rel_x = elem[0].get<float>();
        float rel_y = elem[1].get<float>();

        // now offset *from* the pivot, not from (0,0)
        int x = pivot_x + static_cast<int>(std::round(rel_x * scale));
        int y = pivot_y + static_cast<int>(std::round(rel_y * scale));
        points.emplace_back(x, y);
    }
    if (points.empty())
        throw std::runtime_error("[Area] No valid points loaded");

    // anchor is the pivot itself
    pos_X = pivot_x;
    pos_Y = pivot_y;
}

void Area::apply_offset(int dx, int dy) {
    for (auto& p : points) {
        p.first  += dx;
        p.second += dy;
    }
}

void Area::align(int target_x, int target_y) {
    int dx = target_x - pos_X;
    int dy = target_y - pos_Y;
    apply_offset(dx, dy);
    pos_X = target_x;
    pos_Y = target_y;
}

std::tuple<int, int, int, int> Area::get_bounds() const {
    int minx = points[0].first;
    int maxx = points[0].first;
    int miny = points[0].second;
    int maxy = points[0].second;
    for (const auto& p : points) {
        minx = std::min(minx, p.first);
        maxx = std::max(maxx, p.first);
        miny = std::min(miny, p.second);
        maxy = std::max(maxy, p.second);
    }
    return {minx, miny, maxx, maxy};
}



void Area::generate_circle(int center_x, int center_y, int radius, int edge_smoothness, int map_width, int map_height) {
    int clamped_smoothness = std::clamp(edge_smoothness, 0, 100);
    int vertex_count = std::max(12, 6 + clamped_smoothness * 2);
    std::vector<Point> pts;
    std::mt19937 rng(std::random_device{}());

    double max_deviation = 0.20 * (100 - clamped_smoothness) / 100.0;
    std::uniform_real_distribution<double> scale_dist(1.0 - max_deviation, 1.0 + max_deviation);

    for (int i = 0; i < vertex_count; ++i) {
        double theta = 2.0 * M_PI * i / vertex_count;
        double rx = radius * scale_dist(rng);
        double ry = radius * scale_dist(rng);

        double x = center_x + rx * std::cos(theta);
        double y = center_y + ry * std::sin(theta);

        int xi = static_cast<int>(std::round(std::clamp(x, 0.0, static_cast<double>(map_width))));
        int yi = static_cast<int>(std::round(std::clamp(y, 0.0, static_cast<double>(map_height))));
        pts.emplace_back(xi, yi);
    }

    if (pts.empty()) {
        throw std::runtime_error("[Area] Failed to generate circle points");
    }

    points = std::move(pts);
}

void Area::generate_square(int center_x, int center_y, int w, int h, int edge_smoothness, int map_width, int map_height) {
    int clamped_smoothness = std::clamp(edge_smoothness, 0, 100);
    double max_deviation = 0.25 * (100 - clamped_smoothness) / 100.0;

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> x_offset_dist(-max_deviation * w, max_deviation * w);
    std::uniform_real_distribution<double> y_offset_dist(-max_deviation * h, max_deviation * h);

    int half_w = w / 2;
    int half_h = h / 2;

    std::vector<Point> base = {
        {center_x - half_w, center_y - half_h},
        {center_x + half_w, center_y - half_h},
        {center_x + half_w, center_y + half_h},
        {center_x - half_w, center_y + half_h}
    };

    points.clear();
    for (const auto& [x, y] : base) {
        int jx = static_cast<int>(std::round(x + x_offset_dist(rng)));
        int jy = static_cast<int>(std::round(y + y_offset_dist(rng)));
        points.emplace_back(std::clamp(jx, 0, map_width), std::clamp(jy, 0, map_height));
    }
}

void Area::generate_random(int center_x, int center_y, int w, int h, int edge_smoothness, int map_width, int map_height) {
    int vertex_count = std::max(4, edge_smoothness * 5);
    std::vector<Point> pts;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> angle_dist(0.0, 2 * M_PI);
    std::uniform_real_distribution<double> radius_x_dist(w * 0.3, w * 0.5);
    std::uniform_real_distribution<double> radius_y_dist(h * 0.3, h * 0.5);

    for (int i = 0; i < vertex_count; ++i) {
        double theta = 2.0 * M_PI * i / vertex_count + angle_dist(rng) * 0.1;
        double rx = radius_x_dist(rng);
        double ry = radius_y_dist(rng);

        double x = center_x + rx * std::cos(theta);
        double y = center_y + ry * std::sin(theta);

        int xi = static_cast<int>(std::round(std::clamp(x, 0.0, static_cast<double>(map_width))));
        int yi = static_cast<int>(std::round(std::clamp(y, 0.0, static_cast<double>(map_height))));
        pts.emplace_back(xi, yi);
    }

    if (pts.empty()) {
        throw std::runtime_error("[Area] Failed to generate random shape points");
    }

    points = std::move(pts);
}

void Area::contract(int inset) {
    if (inset <= 0) return;
    for (auto& [x, y] : points) {
        if (x > inset) x -= inset;
        if (y > inset) y -= inset;
    }
}




double Area::get_area() const {
    if (points.size() < 3) return 0.0;

    double area = 0.0;
    size_t n = points.size();

    for (size_t i = 0; i < n; ++i) {
        const auto& [x0, y0] = points[i];
        const auto& [x1, y1] = points[(i + 1) % n];
        area += static_cast<double>(x0 * y1 - x1 * y0);
    }

    return std::abs(area) * 0.5;
}

const std::vector<Area::Point>& Area::get_points() const {
    return points;
}


void Area::set_color(Uint8 r, Uint8 g, Uint8 b) {
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;
}

void Area::union_with(const Area& other) {
    if (other.points.empty()) return;
    points.insert(points.end(), other.points.begin(), other.points.end());
}

bool Area::contains_point(const std::pair<int,int>& pt) const {
    bool inside = false;
    int x = pt.first, y = pt.second;
    size_t n = points.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& [xi, yi] = points[i];
        const auto& [xj, yj] = points[j];
        bool intersect =
            ((yi > y) != (yj > y)) &&
            (x < (xj - xi) * ((y - yi) / float(yj - yi + 1e-9f)) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

bool Area::intersects(const Area& other) const {
    auto [a0, a1, a2, a3] = get_bounds();
    auto [b0, b1, b2, b3] = other.get_bounds();
    return !(a2 < b0 || b2 < a0 || a3 < b1 || b3 < a1);
}








bool Area::contains(int x, int y) const {
    int n = static_cast<int>(points.size());
    if (n < 3) return false;

    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const Point& pi = points[i];
        const Point& pj = points[j];

        bool intersect = ((pi.second > y) != (pj.second > y)) &&
                         (x < (pj.first - pi.first) * (y - pi.second) / double(pj.second - pi.second) + pi.first);
        if (intersect) inside = !inside;
    }
    return inside;
}


