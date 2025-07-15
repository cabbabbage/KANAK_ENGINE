#include "generate_trails.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cmath>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

GenerateTrails::GenerateTrails(const std::string& trail_dir)
    : rng_(std::random_device{}()) 
{
    for (const auto& entry : fs::directory_iterator(trail_dir)) {
        if (entry.path().extension() == ".json") {
            available_assets_.push_back(entry.path().string());
        }
    }

    std::cout << "[GenerateTrails] Loaded " << available_assets_.size() << " trail assets\n";

    if (available_assets_.empty()) {
        throw std::runtime_error("[GenerateTrails] No JSON trail assets found");
    }
}

std::vector<std::unique_ptr<Room>> GenerateTrails::generate_trails(
    const std::vector<std::pair<Room*, Room*>>& room_pairs,
    const std::vector<Area>& existing_areas,
    const std::string& map_dir,

    AssetLibrary* asset_lib)
{
    trail_areas_.clear();
    std::vector<std::unique_ptr<Room>> trail_rooms;

    if (room_pairs.empty()) {
        std::cout << "[GenerateTrails] Warning: No room pairs provided.\n";
        return trail_rooms;
    }

    for (const auto& [a, b] : room_pairs) {
        std::cout << "[GenerateTrails] Connecting: " << a->room_name << " <--> " << b->room_name << "\n";
        bool success = false;

        for (int attempts = 0; attempts < 10 && !success; ++attempts) {
            std::string path = pick_random_asset();
            std::ifstream in(path);
            if (!in.is_open()) {
                std::cout << "[TrailGen] Failed to open asset: " << path << "\n";
                continue;
            }

            json config;
            in >> config;

            int min_width = config.value("min_width", 40);
            int max_width = config.value("max_width", 80);
            int curvyness = config.value("curvyness", 2);
            std::string name = config.value("name", "trail_segment");

            std::uniform_int_distribution<int> width_dist(min_width, max_width);
            double width = static_cast<double>(width_dist(rng_));

            auto centerline = build_centerline(a->map_origin, b->map_origin, curvyness);
            auto polygon = extrude_centerline(centerline, width);

            std::vector<Area::Point> polygon_area_points;
            polygon_area_points.reserve(polygon.size());
            for (const auto& p : polygon) {
                polygon_area_points.emplace_back(
                    static_cast<int>(std::round(p.first)),
                    static_cast<int>(std::round(p.second))
                );
            }

            Area candidate(polygon_area_points);

            bool intersects = false;
            for (const auto& area : existing_areas) {
                if (&area == &a->room_area || &area == &b->room_area) continue;
                if (candidate.intersects(area)) {
                    intersects = true;
                    break;
                }
            }

            if (!intersects) {
                std::cout << "[TrailGen] Trail placed between " 
                          << a->room_name << " and " << b->room_name 
                          << " using asset: " << path << " (attempt " << attempts + 1 << ")\n";

                std::string room_dir = fs::path(path).parent_path().string();

                auto trail_room = std::make_unique<Room>(
                    a->map_origin,
                    name,
                    nullptr,
                    room_dir,
                    map_dir,
                    asset_lib,
                    &candidate
                );

                trail_rooms.push_back(std::move(trail_room));
                trail_areas_.push_back(std::move(candidate));
                success = true;
            } else {
                std::cout << "[TrailGen] Trail intersected existing geometry (attempt " << attempts + 1 << ")\n";
            }
        }

        if (!success) {
            std::cout << "[TrailGen] Failed to place trail between "
                      << a->room_name << " and " << b->room_name << "\n";
        }
    }

    std::cout << "[TrailGen] Total trail rooms created: " << trail_rooms.size() << "\n";
    return trail_rooms;
}

std::string GenerateTrails::pick_random_asset() {
    std::uniform_int_distribution<size_t> dist(0, available_assets_.size() - 1);
    return available_assets_[dist(rng_)];
}

std::vector<GenerateTrails::Point> GenerateTrails::build_centerline(
    const Point& start, const Point& end, int curvyness)
{
    std::vector<Point> line = { start };

    if (curvyness <= 0) {
        line.push_back(end);
        return line;
    }

    double dx = end.first - start.first;
    double dy = end.second - start.second;
    double len = std::hypot(dx, dy);
    double max_offset = len * 0.25 * (static_cast<double>(curvyness) / 8.0);
    std::uniform_real_distribution<double> offset_dist(-max_offset, max_offset);

    for (int i = 1; i <= curvyness; ++i) {
        double t = static_cast<double>(i) / (curvyness + 1);
        double px = start.first + t * dx;
        double py = start.second + t * dy;
        double nx = -dy / len;
        double ny = dx / len;
        double offset = offset_dist(rng_);
        line.emplace_back(px + nx * offset, py + ny * offset);
    }

    line.push_back(end);
    return line;
}

std::vector<GenerateTrails::Point> GenerateTrails::extrude_centerline(
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
