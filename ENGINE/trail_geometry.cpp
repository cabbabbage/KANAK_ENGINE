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

TrailGeometry::TrailGeometry() : rng_(std::random_device{}()) {}

std::vector<TrailGeometry::Point> TrailGeometry::build_centerline(
    const Point& start, const Point& end, int curvyness)
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
            double off = offset_dist(rng_);

            line.emplace_back(
                int(std::round(px + nx * off)),
                int(std::round(py + ny * off))
            );
        }
    }

    line.push_back(end);

    // — Trim the first and last points inward by 1/6 of the path length —
    if (line.size() >= 2) {
        Point s = line.front();
        Point e = line.back();
        double dx = double(e.first  - s.first);
        double dy = double(e.second - s.second);
        double total_len = std::hypot(dx, dy);
        if (total_len > 0.0) {
            double trim = total_len / 6.0;
            double ux = dx / total_len;
            double uy = dy / total_len;
            line.front() = {
                int(std::round(s.first + ux * trim)),
                int(std::round(s.second + uy * trim))
            };
            line.back()  = {
                int(std::round(e.first - ux * trim)),
                int(std::round(e.second - uy * trim))
            };
        }
    }

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


TrailGeometry::Point TrailGeometry::compute_entry_point(const TrailGeometry::Point& center,
                                                        const TrailGeometry::Point& target,
                                                        const Area* area,
                                                        double depth_percent)
{
    if (!area) return center;

    double dx = target.first  - center.first;
    double dy = target.second - center.second;
    double len = std::hypot(dx, dy);
    if (len == 0.0) return center;

    double dirX = dx / len;
    double dirY = dy / len;

    const int max_steps = 2000;
    const double step_size = 1.0;
    TrailGeometry::Point edge_point = center;

    for (int i = 1; i <= max_steps; ++i) {
        double px = center.first + dirX * i * step_size;
        double py = center.second + dirY * i * step_size;
        int ipx = static_cast<int>(std::round(px));
        int ipy = static_cast<int>(std::round(py));
        if (area->contains_point({ipx, ipy})) {
            edge_point = {px, py};
        } else {
            break;
        }
    }

    double t = std::clamp(depth_percent / 100.0, 0.0, 1.0);
    double final_x = center.first + (edge_point.first  - center.first) * t;
    double final_y = center.second + (edge_point.second - center.second) * t;
    return TrailGeometry::Point{final_x, final_y};
}




bool TrailGeometry::attempt_trail_connection(
    Room*                                    a,
    Room*                                    b,
    std::vector<Area>&                       existing_areas,
    const std::string&                       map_dir,
    AssetLibrary*                            asset_lib,
    std::vector<std::unique_ptr<Room>>&      trail_rooms,
    int                                      allowed_intersections,
    const std::string&                       path,
    bool                                     testing,
    std::mt19937&                            rng)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        if (testing) std::cout << "[TrailGen] Failed to open asset: " << path << "\n";
        return false;
    }

    json config; in >> config;

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

    const double trail_entry_depth_percent = 0.0;

    // Use get_center from Area directly
    Point a_center = a->room_area->get_center();
    Point b_center = b->room_area->get_center();

    Point start = compute_entry_point(a_center, b_center, a->room_area.get(), trail_entry_depth_percent);
    Point end   = compute_entry_point(b_center, a_center, b->room_area.get(), trail_entry_depth_percent);

    auto centerline = TrailGeometry().build_centerline(start, end, curvyness);
    auto polygon    = TrailGeometry().extrude_centerline(centerline, width);

    std::vector<Area::Point> pts;
    for (auto& p : polygon)
        pts.emplace_back(int(std::round(p.first)), int(std::round(p.second)));

    Area candidate("trail_candidate", pts);

    auto [aminx, aminy, amaxx, amaxy] = a->room_area->get_bounds();
    auto [bminx, bminy, bmaxx, bmaxy] = b->room_area->get_bounds();

    int intersection_count = 0;
    for (auto& area : existing_areas) {
        auto [minx, miny, maxx, maxy] = area.get_bounds();
        bool isA = (minx == aminx && miny == aminy && maxx == amaxx && maxy == amaxy);
        bool isB = (minx == bminx && miny == bminy && maxx == bmaxx && maxy == bmaxy);
        if (isA || isB) continue;

        if (candidate.intersects(area)) {
            if (++intersection_count > allowed_intersections) {
                if (testing) {
                    std::cout << "[TrailGen] Trail intersected too many areas ("
                              << intersection_count << " > " << allowed_intersections << ")\n";
                }
                return false;
            }
        }
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
    return true;
}
