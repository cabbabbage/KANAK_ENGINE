// area.cpp
#include "area.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <cmath>


namespace fs = std::filesystem;

Area::Area() = default;

Area::Area(const std::vector<Point>& pts)
    : points(pts) {}

// ---------------------------------------------------------
// New constructor: we accept user_scale (e.g. 0.75 for “75%”).
// We will load JSON, read “original_dimensions” and “original_anchor”,
// compute each raw point in full‐res, then multiply by user_scale.
// Finally we shift the anchor so that “bottom‐center” → (0,0).
// ---------------------------------------------------------

Area::Area(int center_x, int center_y, int radius, int map_width, int map_height) {
    if (radius <= 0 || map_width <= 0 || map_height <= 0) {
        throw std::runtime_error("[Area] Invalid dimensions in ellipse constructor");
    }

    int vertex_count = 60;
    std::vector<Point> pts;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> scale_dist(0.8, 1.2); // slight variation per point

    for (int i = 0; i < vertex_count; ++i) {
        double theta = 2.0 * M_PI * i / vertex_count;

        double rx = radius * scale_dist(rng);
        double ry = radius * scale_dist(rng) * 0.6; // squashed vertically

        double x = center_x + rx * std::cos(theta);
        double y = center_y + ry * std::sin(theta);

        int xi = static_cast<int>(std::round(std::clamp(x, 0.0, static_cast<double>(map_width))));
        int yi = static_cast<int>(std::round(std::clamp(y, 0.0, static_cast<double>(map_height))));
        pts.emplace_back(xi, yi);
    }

    if (pts.empty()) {
        throw std::runtime_error("[Area] Failed to generate elliptical points");
    }

    points = std::move(pts);
}





Area::Area(const std::string& json_path, int orig_w, int orig_h, float user_scale) {
    if (!fs::exists(json_path)) {
        throw std::runtime_error("[Area] File not found: " + json_path);
    }
    if (orig_w <= 0 || orig_h <= 0) {
        throw std::runtime_error("[Area] Invalid original dimensions");
    }
    if (user_scale <= 0.0f) {
        throw std::runtime_error("[Area] scale must be > 0");
    }

    std::ifstream in(json_path);
    if (!in.is_open()) {
        throw std::runtime_error("[Area] Failed to open file: " + json_path);
    }

    nlohmann::json J;
    in >> J;

    // 1) Verify JSON’s "original_dimensions" matches what we expect
    auto od = J["original_dimensions"];
    if (!od.is_array() || od.size() != 2) {
        throw std::runtime_error("[Area] original_dimensions missing/invalid");
    }
    float json_w = od[0].get<float>();
    float json_h = od[1].get<float>();
    if (static_cast<int>(json_w + 0.5f) != orig_w ||
        static_cast<int>(json_h + 0.5f) != orig_h) {
        std::cerr << "[Area] Warning: JSON original_dimensions ("
                  << json_w << "," << json_h
                  << ") does not match passed in ("
                  << orig_w << "," << orig_h << ")\n";
        // We continue anyway; ideally they should match.
    }

    // 2) Read “original_anchor” from JSON (we assume bottom-center anchor).
    auto oa = J["original_anchor"];
    if (!oa.is_array() || oa.size() != 2) {
        throw std::runtime_error("[Area] original_anchor missing/invalid");
    }
    float orig_ax = oa[0].get<float>();
    float orig_ay = oa[1].get<float>();

    // 3) For each JSON “(rx, ry)”, compute “full‐res absolute” and then multiply by user_scale:
    //    abs_x = (orig_ax + rx); abs_y = (orig_ay + ry)
    //    scaled_x = round(abs_x * user_scale), scaled_y = round(abs_y * user_scale)
    for (const auto& pt : J["points"]) {
        if (!pt.is_array() || pt.size() != 2) continue;
        float rx = pt[0].get<float>();
        float ry = pt[1].get<float>();

        float full_x = orig_ax + rx;
        float full_y = orig_ay + ry;

        int scaled_x = static_cast<int>(full_x * user_scale + 0.5f);
        int scaled_y = static_cast<int>(full_y * user_scale + 0.5f);
        points.emplace_back(scaled_x, scaled_y);
    }

    if (points.empty()) {
        throw std::runtime_error("[Area] No valid points found in file: " + json_path);
    }

    // 4) Finally shift so that “(orig_ax, orig_ay) in full‐res” → (0, 0) in scaled‐local:
    int dx = static_cast<int>(-orig_ax * user_scale + 0.5f);
    int dy = static_cast<int>(-orig_ay * user_scale + 0.5f);
    apply_offset(dx, dy);
}


const std::vector<Area::Point>& Area::get_points() const {
    return points;
}



void Area::apply_offset(int dx, int dy) {
    for (auto& p : points) {
        p.first  += dx;
        p.second += dy;
    }
}

void Area::set_color(Uint8 r, Uint8 g, Uint8 b) {
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;
}

void Area::union_with(const Area& other) {
    if (other.points.empty()) return;

    // Naive merge: just include all points, even overlapping
    points.insert(points.end(), other.points.begin(), other.points.end());
}




bool Area::contains(int x, int y) const {
    // Use ray-casting algorithm to determine if the point is inside polygon
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



bool Area::intersects(const Area& other) const {
    auto [a0,a1,a2,a3] = get_bounds();
    auto [b0,b1,b2,b3] = other.get_bounds();
    return !(a2 < b0 || b2 < a0 || a3 < b1 || b3 < a1);
}

bool Area::contains_point(const std::pair<int,int>& pt) const {
    bool inside = false;
    int x = pt.first, y = pt.second;
    size_t n = points.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        auto [xi, yi] = points[i];
        auto [xj, yj] = points[j];
        bool intersect =
            ((yi > y) != (yj > y)) &&
            ( x < (xj - xi) * ((y - yi)/float(yj - yi + 1e-9f)) + xi );
        if (intersect) inside = !inside;
    }
    return inside;
}

std::tuple<int,int,int,int> Area::get_bounds() const {
    if (points.empty()) {
        throw std::out_of_range("[Area] get_bounds() on empty points");
    }
    int minx = points[0].first, maxx = minx;
    int miny = points[0].second, maxy = miny;
    for (const auto& [x,y] : points) {
        minx = std::min(minx, x);
        maxx = std::max(maxx, x);
        miny = std::min(miny, y);
        maxy = std::max(maxy, y);
    }
    return {minx, miny, maxx, maxy};
}

SDL_Texture* Area::get_image(SDL_Renderer* renderer) const {
    auto [minx, miny, maxx, maxy] = get_bounds();
    int w = maxx - minx + 1;
    int h = maxy - miny + 1;
    if (w <= 0 || h <= 0) return nullptr;

    SDL_Texture* tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    size_t n = points.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        SDL_RenderDrawLine(renderer,
            points[j].first - minx, points[j].second - miny,
            points[i].first - minx, points[i].second - miny
        );
    }

    SDL_SetRenderTarget(renderer, nullptr);
    return tex;
}

SDL_Surface* Area::rescale_surface(SDL_Surface* surf, float sf) {
    if (!surf || sf <= 0.0f) return nullptr;
    int w = static_cast<int>(surf->w * sf);
    int h = static_cast<int>(surf->h * sf);
    SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_BlitScaled(surf, nullptr, scaled, nullptr);
    return scaled;
}
