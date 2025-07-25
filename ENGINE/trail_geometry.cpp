// === File: trail_geometry.cpp ===
#include "trail_geometry.hpp"
#include <cmath>
#include <random>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "Room.hpp"
#include "asset_library.hpp"
#include "Area.hpp"
#include <iostream>
#include <limits>

using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<TrailGeometry::Point> TrailGeometry::build_centerline(
    const Point& start, const Point& end, int curvyness, std::mt19937& rng)
{
    std::vector<Point> line;
    line.reserve(curvyness + 2);
    line.push_back(start);

    if (curvyness > 0) {
        double dx = end.first  - start.first;
        double dy = end.second - start.second;
        double len = std::hypot(dx, dy);
        double max_offset = len * 0.25 * (static_cast<double>(curvyness) / 8.0);
        std::uniform_real_distribution<double> offset_dist(-max_offset, max_offset);

        for (int i = 1; i <= curvyness; ++i) {
            double t  = static_cast<double>(i) / (curvyness + 1);
            double px = start.first  + t * dx;
            double py = start.second + t * dy;
            double nx = -dy / len;
            double ny =  dx / len;
            double off = offset_dist(rng);

            line.emplace_back(
                int(std::round(px + nx * off)),
                int(std::round(py + ny * off))
            );
        }
    }

    line.push_back(end);
    return line;
}

std::vector<TrailGeometry::Point> TrailGeometry::extrude_centerline(
    const std::vector<Point>& centerline, double width)
{
    double half_w = width * 0.5;
    std::vector<Point> left, right;

    for (size_t i = 0; i < centerline.size(); ++i) {
        double cx = centerline[i].first;
        double cy = centerline[i].second;
        double dx, dy;

        if (i == 0) {
            dx = centerline[i + 1].first - cx;
            dy = centerline[i + 1].second - cy;
        } else if (i == centerline.size() - 1) {
            dx = cx - centerline[i - 1].first;
            dy = cy - centerline[i - 1].second;
        } else {
            dx = centerline[i + 1].first - centerline[i - 1].first;
            dy = centerline[i + 1].second - centerline[i - 1].second;
        }

        double len = std::hypot(dx, dy);
        if (len == 0) len = 1.0;
        double nx = -dy / len;
        double ny = dx / len;

        left.emplace_back(
            static_cast<int>(std::round(cx + nx * half_w)),
            static_cast<int>(std::round(cy + ny * half_w))
        );
        right.emplace_back(
            static_cast<int>(std::round(cx - nx * half_w)),
            static_cast<int>(std::round(cy - ny * half_w))
        );
    }

    std::vector<Point> polygon;
    polygon.insert(polygon.end(), left.begin(), left.end());
    polygon.insert(polygon.end(), right.rbegin(), right.rend());
    return polygon;
}

TrailGeometry::Point TrailGeometry::compute_edge_point(const Point& center,
                                                       const Point& toward,
                                                       const Area* area)
{
    if (!area) return center;

    double dx = toward.first  - center.first;
    double dy = toward.second - center.second;
    double len = std::hypot(dx, dy);
    if (len == 0.0) return center;

    double dirX = dx / len;
    double dirY = dy / len;

    const int max_steps = 2000;
    const double step_size = 1.0;
    Point edge = center;

    for (int i = 1; i <= max_steps; ++i) {
        double px = center.first + dirX * i * step_size;
        double py = center.second + dirY * i * step_size;
        int ipx = static_cast<int>(std::round(px));
        int ipy = static_cast<int>(std::round(py));
        if (area->contains_point({ipx, ipy})) {
            edge = {px, py};
        } else {
            break;
        }
    }

    return edge;
}

bool TrailGeometry::attempt_trail_connection(
    Room* a,
    Room* b,
    std::vector<Area>& existing_areas,
    const std::string& map_dir,
    AssetLibrary* asset_lib,
    std::vector<std::unique_ptr<Room>>& trail_rooms,
    int allowed_intersections,
    const std::string& path,
    bool testing,
    std::mt19937& rng)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        if (testing) std::cout << "[TrailGen] Failed to open asset: " << path << "\n";
        return false;
    }

    json config;
    in >> config;

    int min_width = config.value("min_width", 40);
    int max_width = config.value("max_width", 80);
    int curvyness = config.value("curvyness", 2);
    std::string name = config.value("name", "trail_segment");

    double width = std::uniform_int_distribution<int>(min_width, max_width)(rng);
    if (testing) {
        std::cout << "[TrailGen] Using asset: " << path
                  << "  width=" << width
                  << "  curvyness=" << curvyness << "\n";
    }

    Point a_center = a->room_area->get_center();
    Point b_center = b->room_area->get_center();

    const double overshoot = 300.0;
    auto extend_past_edge = [&](const Point& center, const Point& toward, const Area* area) -> Point {
        Point edge = TrailGeometry::compute_edge_point(center, toward, area);
        double dx = edge.first - center.first;
        double dy = edge.second - center.second;
        double len = std::hypot(dx, dy);
        if (len == 0.0) return edge;

        double ux = dx / len;
        double uy = dy / len;

        return {
            edge.first + ux * overshoot,
            edge.second + uy * overshoot
        };
    };

    Point a_edge = extend_past_edge(a_center, b_center, a->room_area.get());
    Point b_edge = extend_past_edge(b_center, a_center, b->room_area.get());

    auto [aminx, aminy, amaxx, amaxy] = a->room_area->get_bounds();
    auto [bminx, bminy, bmaxx, bmaxy] = b->room_area->get_bounds();

    for (int attempt = 0; attempt < 1000; ++attempt) {
        std::vector<Point> full_line;
        full_line.push_back(a_center);
        full_line.push_back(a_edge);

        auto middle = build_centerline(a_edge, b_edge, curvyness, rng);
        full_line.insert(full_line.end(), middle.begin(), middle.end());

        full_line.push_back(b_edge);
        full_line.push_back(b_center);

        auto polygon = extrude_centerline(full_line, width);

        std::vector<Area::Point> pts;
        for (auto& p : polygon)
            pts.emplace_back(int(std::round(p.first)), int(std::round(p.second)));

        Area candidate("trail_candidate", pts);

        int intersection_count = 0;
        for (auto& area : existing_areas) {
            auto [minx, miny, maxx, maxy] = area.get_bounds();
            bool isA = (minx == aminx && miny == aminy && maxx == amaxx && maxy == amaxy);
            bool isB = (minx == bminx && miny == bminy && maxx == bmaxx && maxy == bmaxy);
            if (isA || isB) continue;

            if (candidate.intersects(area)) {
                intersection_count++;
                break;
            }
        }

        if (intersection_count > allowed_intersections) {
            if (testing && attempt == 999) {
                std::cout << "[TrailGen] Failed after 1000 attempts due to intersections\n";
            }
            continue;
        }

        std::string room_dir = fs::path(path).parent_path().string();
        auto trail_room = std::make_unique<Room>(
            a->map_origin,
            "trail",
            name,
            nullptr,
            room_dir,
            map_dir,
            asset_lib,
            &candidate
        );

        a->add_connecting_room(trail_room.get());
        b->add_connecting_room(trail_room.get());
        trail_room->add_connecting_room(a);
        trail_room->add_connecting_room(b);

        existing_areas.push_back(candidate);
        trail_rooms.push_back(std::move(trail_room));

        if (testing) {
            std::cout << "[TrailGen] Trail succeeded on attempt " << attempt + 1 << "\n";
        }
        return true;
    }

    return false;
}
