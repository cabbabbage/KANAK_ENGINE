#ifndef AREA_HPP
#define AREA_HPP

#include <vector>
#include <string>
#include <tuple>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using Point = std::pair<int, int>;

class Area {
public:
    std::vector<Point> points;

    Area();
    Area(const std::vector<Point>& pts);
    Area(const std::string& json_path);
    bool contains_point(const std::pair<int, int>& pt) const;

    void apply_offset(int dx, int dy);
    void set_position(int x, int y);
    std::tuple<int, int, int, int> get_bounds() const;
    bool intersects(const Area& other) const;
};

#endif
