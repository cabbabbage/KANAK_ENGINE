// === File: generate_trails.cpp ===
#include "generate_trails.hpp"
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// Helper: Catmull–Rom interpolation between four control points
static std::pair<double, double> catmullRomInterpolate(
    const std::pair<double,double>& p0,
    const std::pair<double,double>& p1,
    const std::pair<double,double>& p2,
    const std::pair<double,double>& p3,
    double t)
{
    double t2 = t * t;
    double t3 = t2 * t;
    double x = 0.5 * ((2.0 * p1.first) +
                      (-p0.first + p2.first) * t +
                      (2.0*p0.first - 5.0*p1.first + 4.0*p2.first - p3.first) * t2 +
                      (-p0.first + 3.0*p1.first - 3.0*p2.first + p3.first) * t3);
    double y = 0.5 * ((2.0 * p1.second) +
                      (-p0.second + p2.second) * t +
                      (2.0*p0.second - 5.0*p1.second + 4.0*p2.second - p3.second) * t2 +
                      (-p0.second + 3.0*p1.second - 3.0*p2.second + p3.second) * t3);
    return { x, y };
}

// Build a smooth centerline (vector of points) between start and end using Catmull–Rom
std::vector<GenerateTrails::Point> GenerateTrails::buildCenterline(
    const Point& start,
    const Point& end,
    int curvyness)
{
    int num_mid = std::clamp(curvyness, 0, 8);
    double dx = end.first - start.first;
    double dy = end.second - start.second;
    double len = std::hypot(dx, dy);

    // Raw control points
    std::vector<std::pair<double,double>> control_pts;
    control_pts.reserve(num_mid + 2);
    control_pts.emplace_back(static_cast<double>(start.first),
                             static_cast<double>(start.second));

    if (num_mid > 0) {
        double max_offset = (static_cast<double>(curvyness) / 8.0) * (len * 0.25);
        std::uniform_real_distribution<double> offset_dist(-max_offset, max_offset);

        for (int k = 1; k <= num_mid; ++k) {
            double t = static_cast<double>(k) / (num_mid + 1);
            double px = start.first + t * dx;
            double py = start.second + t * dy;
            double nx = -dy / len;
            double ny = dx / len;
            double off = offset_dist(rng_);
            control_pts.emplace_back(px + nx * off, py + ny * off);
        }
    }

    control_pts.emplace_back(static_cast<double>(end.first),
                             static_cast<double>(end.second));

    // Sample Catmull–Rom spline
    std::vector<std::pair<double,double>> center_samples;
    if (control_pts.size() == 2) {
        center_samples = control_pts;
    } else {
        int m = static_cast<int>(control_pts.size());
        std::vector<std::pair<double,double>> pts_ext;
        pts_ext.reserve(m + 2);
        pts_ext.push_back(control_pts.front());
        pts_ext.insert(pts_ext.end(), control_pts.begin(), control_pts.end());
        pts_ext.push_back(control_pts.back());

        int samples_per_segment = 20;
        for (int i = 0; i < m - 1; ++i) {
            for (int s = 0; s < samples_per_segment; ++s) {
                double t = static_cast<double>(s) / samples_per_segment;
                auto interp = catmullRomInterpolate(
                    pts_ext[i], pts_ext[i + 1], pts_ext[i + 2], pts_ext[i + 3], t);
                center_samples.push_back(interp);
            }
        }
        center_samples.push_back(control_pts.back());
    }

    // Convert to integer points
    std::vector<Point> centerline;
    centerline.reserve(center_samples.size());
    for (auto& [cx, cy] : center_samples) {
        centerline.emplace_back(static_cast<int>(std::round(cx)),
                                static_cast<int>(std::round(cy)));
    }
    return centerline;
}

// Extrude a polyline (centerline) outward by width to form polygon area
std::vector<GenerateTrails::Point> GenerateTrails::extrudeToWidth(
    const std::vector<Point>& centerline,
    double width)
{
    double half_w = width * 0.5;
    size_t m = centerline.size();
    std::vector<Point> left_pts, right_pts;
    left_pts.reserve(m);
    right_pts.reserve(m);

    for (size_t i = 0; i < m; ++i) {
        double cx = centerline[i].first;
        double cy = centerline[i].second;
        double dirx, diry;
        if (i == 0) {
            dirx = centerline[i + 1].first - cx;
            diry = centerline[i + 1].second - cy;
        } else if (i == m - 1) {
            dirx = cx - centerline[i - 1].first;
            diry = cy - centerline[i - 1].second;
        } else {
            dirx = centerline[i + 1].first - centerline[i - 1].first;
            diry = centerline[i + 1].second - centerline[i - 1].second;
        }
        double seg_len = std::hypot(dirx, diry);
        if (seg_len == 0.0) {
            dirx = 1.0; diry = 0.0;
            seg_len = 1.0;
        }
        double nx = -diry / seg_len;
        double ny = dirx / seg_len;

        int lx = static_cast<int>(std::round(cx + nx * half_w));
        int ly = static_cast<int>(std::round(cy + ny * half_w));
        int rx = static_cast<int>(std::round(cx - nx * half_w));
        int ry = static_cast<int>(std::round(cy - ny * half_w));

        left_pts.emplace_back(lx, ly);
        right_pts.emplace_back(rx, ry);
    }

    std::vector<Point> polygon;
    polygon.reserve(left_pts.size() + right_pts.size());
    for (auto& p : left_pts) polygon.push_back(p);
    for (int i = static_cast<int>(right_pts.size()) - 1; i >= 0; --i) {
        polygon.push_back(right_pts[i]);
    }
    return polygon;
}

GenerateTrails::GenerateTrails(std::string map_path,
                               const std::vector<GenerateRoom>& rooms,
                               int map_width,
                               int map_height)
    : map_path(std::move(map_path)),
      map_width_(map_width),
      map_height_(map_height),
      rng_(std::random_device{}())
{
    // Gather JSON configs under trails/
    for (const auto& entry : fs::directory_iterator(this->map_path + "/trails")) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
            available_assets_.push_back(path);
        }
    }
    if (available_assets_.empty()) {
        std::cerr << "[GenerateTrails] Warning: No JSON files in 'trails/'\n";
        return;
    }

    size_t n = rooms.size();
    if (n < 2) return;

    // Build room-center pairs sorted by distance
    std::vector<std::tuple<double, int, int>> all_pairs;
    for (size_t i = 0; i < n; ++i) {
        Point ci = { rooms[i].getCenterX(), rooms[i].getCenterY() };
        for (size_t j = i + 1; j < n; ++j) {
            Point cj = { rooms[j].getCenterX(), rooms[j].getCenterY() };
            double d = std::hypot(ci.first - cj.first, ci.second - cj.second);
            all_pairs.emplace_back(d, static_cast<int>(i), static_cast<int>(j));
        }
    }
    std::sort(all_pairs.begin(), all_pairs.end());

    // Disjoint-set for connectivity
    std::vector<int> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](int x) {
        while (x != parent[x]) x = parent[x] = parent[parent[x]];
        return x;
    };
    auto unite = [&](int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        parent[rb] = ra;
        return true;
    };

    int connected_components = static_cast<int>(n);
    std::vector<int> degree(n, 0);
    std::vector<bool> is_terminal(n, false);
    for (size_t i = 0; i < n; ++i) {
        is_terminal[i] = rooms[i].isSpawn() || rooms[i].isBoss();
    }

    for (const auto& [dist, i, j] : all_pairs) {
        if ((is_terminal[i] && degree[i] >= 1) || (!is_terminal[i] && degree[i] >= 3)) continue;
        if ((is_terminal[j] && degree[j] >= 1) || (!is_terminal[j] && degree[j] >= 3)) continue;

        Point start = rooms[i].getPointInside();
        Point end   = rooms[j].getPointInside();

        int intersections = countTrailIntersections(start, end);
        bool early_phase = trail_areas_.size() < 2;
        bool ok = early_phase ? (intersections <= 3) : (intersections >= 1 && intersections <= 3);
        if (!ok) continue;

        addTrail(start, end);
        degree[i]++;
        degree[j]++;
        if (unite(i, j)) connected_components--;
        if (connected_components == 1) break;
    }
}

int GenerateTrails::countTrailIntersections(const Point& start, const Point& end) {
    if (trail_areas_.empty()) return 0;

    std::string path = pickAssetsPath();
    std::ifstream in(path);
    if (!in.is_open()) return 999;
    nlohmann::json config;
    in >> config;
    in.close();

    int min_width = config.value("min_width", 50);
    int max_width = config.value("max_width", 150);
    int curvyness = config.value("curvyness", 1);

    std::uniform_int_distribution<int> width_dist(min_width, max_width);
    double width = static_cast<double>(width_dist(rng_));

    // Build centerline and polygon
    std::vector<Point> centerline = buildCenterline(start, end, curvyness);
    std::vector<Point> polygon = extrudeToWidth(centerline, width);
    Area test_trail(polygon);

    int count = 0;
    for (const auto& existing : trail_areas_) {
        if (test_trail.intersects(existing)) count++;
    }
    return count;
}

void GenerateTrails::addTrail(const Point& start, const Point& end) {
    std::string path = pickAssetsPath();
    std::ifstream in(path);
    if (!in.is_open()) return;
    nlohmann::json config;
    in >> config;
    in.close();

    int min_width = config.value("min_width", 50);
    int max_width = config.value("max_width", 150);
    int curvyness = config.value("curvyness", 1);

    std::uniform_int_distribution<int> width_dist(min_width, max_width);
    double width = static_cast<double>(width_dist(rng_));

    // Step 1: build the curvy centerline
    std::vector<Point> centerline = buildCenterline(start, end, curvyness);

    // Step 2: extrude that line by width to form the final polygon
    std::vector<Point> polygon = extrudeToWidth(centerline, width);

    Area trail_area(polygon);
    trail_areas_.push_back(std::move(trail_area));
}

std::string GenerateTrails::pickAssetsPath() {
    if (available_assets_.empty()) {
        throw std::runtime_error("[GenerateTrails] No JSON files in 'trails/'");
    }
    std::uniform_int_distribution<size_t> dist(0, available_assets_.size() - 1);
    return available_assets_[dist(rng_)];
}

const std::vector<Area>& GenerateTrails::getTrailAreas() const {
    return trail_areas_;
}
