#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <utility>

class Area {
public:
    using Point = std::pair<int, int>;

    Area();
    Area(const std::vector<Point>& pts);
    Area(int center_x, int center_y, int w, int h,
         const std::string& geometry,
         int edge_smoothness,
         int map_width, int map_height);
    Area(const std::string& json_path, float scale);

    void apply_offset(int dx, int dy);
    void align(int target_x, int target_y);
    void contract(int inset);

    double get_area() const;
    const std::vector<Point>& get_points() const;
    std::tuple<int, int, int, int> get_bounds() const;
    void union_with(const Area& other);
    bool contains_point(const Point& pt) const;
    bool intersects(const Area& other) const;

    int pos_X, pos_Y;
    int center_x = 0, center_y = 0;
    double area_size = 0.0;

private:
    std::vector<Point> points;

    void generate_circle(int center_x, int center_y, int radius, int edge_smoothness, int map_width, int map_height);
    void generate_square(int center_x, int center_y, int w, int h, int edge_smoothness, int map_width, int map_height);
    void update_geometry_data();
};
