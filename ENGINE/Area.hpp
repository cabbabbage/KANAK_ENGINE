// === File: area.hpp ===
#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <utility>
#include <SDL.h>

class Area {
public:
    using Point = std::pair<int,int>;

    // Constructors
    Area();
    explicit Area(const std::vector<Point>& pts);
    Area(int center_x, int center_y, int w, int h,
         const std::string& geometry,
         int edge_smoothness,
         int map_width, int map_height);
    Area(const std::string& json_path, float scale);

    // Transformations
    void apply_offset(int dx, int dy);
    void align(int target_x, int target_y);

    // Bounds & area
    std::tuple<int,int,int,int> get_bounds() const;
    double get_area() const;

    // Point containment & intersection
    bool contains_point(const Point& pt) const;
    bool contains(int x, int y) const;
    bool intersects(const Area& other) const;

    void contract(int inset);

    void generate_static_faded_areas();

    // Point access
    const std::vector<Point>& get_points() const;
    int get_x() const;
    int get_y() const;

    // Procedural generation
    void generate_circle(int center_x, int center_y, int radius,
                         int edge_smoothness,
                         int map_width, int map_height);
    void generate_square(int center_x, int center_y, int w, int h,
                         int edge_smoothness,
                         int map_width, int map_height);
    void generate_random(int center_x, int center_y, int w, int h,
                         int edge_smoothness,
                         int map_width, int map_height);


    // Utilities
    void set_color(Uint8 r, Uint8 g, Uint8 b);
    void union_with(const Area& other);

private:
    std::vector<Point> points;
    int pos_X;
    int pos_Y;
    SDL_Color color;
};
