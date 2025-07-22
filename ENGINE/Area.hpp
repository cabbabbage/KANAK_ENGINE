#pragma once

#include <vector>
#include <string>
#include <tuple>

class Area {
public:
    using Point = std::pair<int, int>;

    // Explicit constructors (no default constructor)
    Area(const std::string& name);
    Area(const std::string& name, const std::vector<Point>& pts);
    Area(const std::string& name, int cx, int cy, int w, int h,
         const std::string& geometry,
         int edge_smoothness,
         int map_width, int map_height);
    Area(const std::string& name, const std::string& json_path, float scale);

    // Geometry
    void apply_offset(int dx, int dy);
    void align(int target_x, int target_y);
    void contract(int inset);
    void union_with(const Area& other);

    // Queries
    std::tuple<int, int, int, int> get_bounds() const;
    const std::vector<Point>& get_points() const;
    bool contains_point(const Point& pt) const;
    bool intersects(const Area& other) const;
    double get_area() const;
    Point random_point_within() const;
    Point get_center() const;
    // Metadata
    const std::string& get_name() const { return area_name_; }
    double get_size() const;
private:
    std::vector<Point> points;
    std::string area_name_;

    int pos_X = 0;
    int pos_Y = 0;
    int center_x = 0;
    int center_y = 0;
    double area_size = 0.0;

    void update_geometry_data();
    void generate_circle(int cx, int cy, int radius,
                         int edge_smoothness,
                         int map_width, int map_height);
    void generate_square(int cx, int cy, int w, int h,
                         int edge_smoothness,
                         int map_width, int map_height);
};
