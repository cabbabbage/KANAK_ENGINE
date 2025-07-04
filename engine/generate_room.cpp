// generate_room.cpp
#include "generate_room.hpp"

#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

std::mt19937 GenerateRoom::rng_(std::random_device{}());

GenerateRoom::GenerateRoom(std::string map_path,
                           const std::vector<GenerateRoom*>& existing_rooms,
                           int map_width,
                           int map_height,
                           const std::string& json_path,
                           SDL_Renderer* renderer)
    : map_path(map_path),
      map_width_(map_width),
      map_height_(map_height),
      renderer_(renderer)
{
    std::ifstream in(json_path);
    if (!in.is_open()) throw std::runtime_error("[GenerateRoom] Failed to open room json: " + json_path);

    nlohmann::json J;
    in >> J;
    assets_path_ = json_path;

    int min_width = J["min_width"];
    int max_width = J["max_width"];
    int min_height = J["min_height"];
    int max_height = J["max_height"];
    int edge_smoothness = J["edge_smoothness"];
    std::string geometry = J["geometry"];

    is_spawn_ = J.value("is_spawn", false);
    is_boss_ = J.value("is_boss", false);
    inherits = J.value("inherits_map_assets", false);

    const double avg_dim = (map_width_ + map_height_) / 2.0;
    const double center_dist_threshold = 0.1 * avg_dim;
    const double line_dist_threshold = 0.002 * avg_dim;
    const double intersection_min_dist = 0.03 * avg_dim;

    const int edge_margin_x = static_cast<int>(map_width_ * 0.10);
    const int edge_margin_y = static_cast<int>(map_height_ * 0.10);

    bool placed = false;
    int attempts = 0;

    while (!placed && attempts < 10000) {
        double scale = (attempts >= 20) ? 0.9 : 1.0;
        int cur_min_w = static_cast<int>(min_width * scale);
        int cur_max_w = static_cast<int>(max_width * scale);
        int cur_min_h = static_cast<int>(min_height * scale);
        int cur_max_h = static_cast<int>(max_height * scale);

        std::uniform_int_distribution<int> w_dist(cur_min_w, cur_max_w);
        std::uniform_int_distribution<int> h_dist(cur_min_h, cur_max_h);
        int w = w_dist(rng_);
        int h = h_dist(rng_);

        if (is_spawn_) {
            center_x_ = map_width_ / 2;
            center_y_ = map_height_ / 2;
        } else if (is_boss_) {
            std::uniform_int_distribution<int> x_dist(map_width_ * 8 / 10 - w, map_width_ - edge_margin_x);
            std::uniform_int_distribution<int> y_dist(edge_margin_y, map_height_ - edge_margin_y - h);
            center_x_ = x_dist(rng_) + w / 2;
            center_y_ = y_dist(rng_) + h / 2;
        } else {
            std::uniform_int_distribution<int> x_dist(edge_margin_x, map_width_ - edge_margin_x - w);
            std::uniform_int_distribution<int> y_dist(edge_margin_y, map_height_ - edge_margin_y - h);
            center_x_ = x_dist(rng_) + w / 2;
            center_y_ = y_dist(rng_) + h / 2;
        }

        if (center_x_ < edge_margin_x || center_x_ > map_width_ - edge_margin_x ||
            center_y_ < edge_margin_y || center_y_ > map_height_ - edge_margin_y) {
            attempts++;
            continue;
        }

        Area candidate(center_x_, center_y_, w, h, geometry, edge_smoothness, map_width_, map_height_);

        bool invalid = false;
        for (const auto* other : existing_rooms) {
            if (candidate.intersects(other->getArea())) {
                invalid = true;
                break;
            }

            int ox = other->getCenterX();
            int oy = other->getCenterY();
            double dx = center_x_ - ox;
            double dy = center_y_ - oy;
            double dist = std::hypot(dx, dy);

            if (json_path.find("intersection.json") != std::string::npos) {
                if (dist < intersection_min_dist) {
                    invalid = true;
                    break;
                }
            } else if (dist < center_dist_threshold) {
                invalid = true;
                break;
            }
        }

        for (size_t i = 0; i < existing_rooms.size(); ++i) {
            for (size_t j = i + 1; j < existing_rooms.size(); ++j) {
                int x1 = existing_rooms[i]->getCenterX();
                int y1 = existing_rooms[i]->getCenterY();
                int x2 = existing_rooms[j]->getCenterX();
                int y2 = existing_rooms[j]->getCenterY();

                double dx = x2 - x1;
                double dy = y2 - y1;
                double length_sq = dx * dx + dy * dy;

                double t = ((center_x_ - x1) * dx + (center_y_ - y1) * dy) / (length_sq + 1e-9);
                t = std::max(0.0, std::min(1.0, t));
                double proj_x = x1 + t * dx;
                double proj_y = y1 + t * dy;

                double dist_to_line = std::hypot(proj_x - center_x_, proj_y - center_y_);
                if (dist_to_line < line_dist_threshold) {
                    invalid = true;
                    break;
                }
            }
            if (invalid) break;
        }

        if (!invalid || attempts >= 999) {
            room_area_ = std::move(candidate);
            placed = true;
        }

        attempts++;
    }

    if (!placed) throw std::runtime_error("[GenerateRoom] Failed to place required room: " + json_path);
}

const Area& GenerateRoom::getArea() const {
    return room_area_;
}

int GenerateRoom::getCenterX() const {
    return center_x_;
}

bool GenerateRoom::isSpawn() const {
    return is_spawn_;
}

bool GenerateRoom::isBoss() const {
    return is_boss_;
}

int GenerateRoom::getCenterY() const {
    return center_y_;
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
