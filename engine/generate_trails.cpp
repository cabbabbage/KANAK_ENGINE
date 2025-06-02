#include "generate_trails.hpp"
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <set>
#include <iostream>

namespace fs = std::filesystem;

GenerateTrails::GenerateTrails(std::string map_path, const std::vector<GenerateRoom>& rooms,
                               int map_width,
                               int map_height)
    : map_path(map_path),
      rng_(std::random_device{}()),
      map_width_(map_width),
      map_height_(map_height)
{
    for (const auto& entry : fs::directory_iterator(map_path + "/trails")) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
            available_assets_.push_back(path);
        }
    }
    if (available_assets_.empty()) {
        throw std::runtime_error("[GenerateTrails] No JSON files found in 'trails/'");
    }

    size_t n = rooms.size();
    std::set<std::pair<int,int>> connections;
    for (size_t i = 0; i < n; ++i) {
        std::vector<std::pair<double,int>> dist_idx;
        Point ci = { rooms[i].getCenterX(), rooms[i].getCenterY() };
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            Point cj = { rooms[j].getCenterX(), rooms[j].getCenterY() };
            double dx = ci.first - cj.first;
            double dy = ci.second - cj.second;
            double d = std::hypot(dx, dy);
            dist_idx.emplace_back(d, static_cast<int>(j));
        }
        std::sort(dist_idx.begin(), dist_idx.end(),
                  [](auto& a, auto& b){ return a.first < b.first; });
        int limit = std::min<size_t>(3, dist_idx.size());
        for (int k = 0; k < limit; ++k) {
            int j = dist_idx[k].second;
            int a = static_cast<int>(std::min(i, (size_t)j));
            int b = static_cast<int>(std::max(i, (size_t)j));
            if (!connections.count({a, b})) {
                connections.insert({a, b});
            }
        }
    }

    std::set<std::pair<int, int>> used_pairs;
    for (auto& conn : connections) {
        int i = conn.first;
        int j = conn.second;
        if (used_pairs.count({i, j}) || used_pairs.count({j, i})) continue;
        used_pairs.insert({i, j});

        Point start = rooms[i].getPointInside();
        Point end   = rooms[j].getPointInside();
        std::vector<Point> poly_pts = buildPolygon(start, end);
        Area trail_area(poly_pts);
        std::string path = pickAssetsPath();
        trails_.emplace_back(Trail{ trail_area, path });
    }
}

const std::vector<GenerateTrails::Trail>& GenerateTrails::getTrails() const {
    return trails_;
}

std::vector<GenerateTrails::Point> GenerateTrails::buildPolygon(const Point& start,
                                                                 const Point& end)
{
    int num_mid = 2;
    double dx = end.first - start.first;
    double dy = end.second - start.second;
    double len = std::hypot(dx, dy);
    std::vector<std::pair<double,double>> control_pts;
    control_pts.emplace_back(start.first, start.second);

    for (int k = 1; k <= num_mid; ++k) {
        double t = static_cast<double>(k) / (num_mid + 1);
        double px = start.first + t * dx;
        double py = start.second + t * dy;
        double normal_x = -dy / len;
        double normal_y = dx / len;
        std::uniform_real_distribution<double> offset_dist(-0.03 * len, 0.03 * len);
        double offset = offset_dist(rng_);
        double cx = px + normal_x * offset;
        double cy = py + normal_y * offset;
        control_pts.emplace_back(cx, cy);
    }

    control_pts.emplace_back(end.first, end.second);

    double width = std::min(map_width_, map_height_) * 0.025; // reduced by 50%
    double half_w = width / 2.0;
    std::vector<Point> left_pts, right_pts;
    int m = static_cast<int>(control_pts.size());
    for (int i = 0; i < m; ++i) {
        double x = control_pts[i].first;
        double y = control_pts[i].second;
        double dir_x, dir_y;
        if (i == 0) {
            dir_x = control_pts[i + 1].first - x;
            dir_y = control_pts[i + 1].second - y;
        } else if (i == m - 1) {
            dir_x = x - control_pts[i - 1].first;
            dir_y = y - control_pts[i - 1].second;
        } else {
            dir_x = control_pts[i + 1].first - control_pts[i - 1].first;
            dir_y = control_pts[i + 1].second - control_pts[i - 1].second;
        }
        double segment_len = std::hypot(dir_x, dir_y);
        double perp_x = -dir_y / segment_len;
        double perp_y = dir_x / segment_len;

        left_pts.emplace_back(static_cast<int>(std::round(x + perp_x * half_w)),
                              static_cast<int>(std::round(y + perp_y * half_w)));
        right_pts.emplace_back(static_cast<int>(std::round(x - perp_x * half_w)),
                               static_cast<int>(std::round(y - perp_y * half_w)));
    }

    std::vector<Point> polygon = left_pts;
    for (int i = m - 1; i >= 0; --i) {
        polygon.push_back(right_pts[i]);
    }
    return polygon;
}

std::string GenerateTrails::pickAssetsPath() {
    if (available_assets_.empty()) {
        throw std::runtime_error("[GenerateTrails] No JSON files found in 'trails/'");
    }
    std::uniform_int_distribution<size_t> dist(0, available_assets_.size() - 1);
    return available_assets_[dist(rng_)];
}
