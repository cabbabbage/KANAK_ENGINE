// area.hpp
#ifndef AREA_HPP
#define AREA_HPP

#include <vector>
#include <tuple>
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <SDL.h>

class Area {
public:
    using Point = std::pair<int,int>;

    Area();
    explicit Area(const std::vector<Point>& pts);
    Area(int center_x, int center_y, int radius, int map_width, int map_height);
// area.hpp
    void union_with(const Area& other);

    // New constructor: pass (json_path, orig_w, orig_h, user_scale)
    //   - orig_w, orig_h = the PNG’s *unscaled* width/height
    //   - user_scale     = (scale_percentage / 100.0f)
    Area(const std::string& json_path,
         int orig_w,
         int orig_h,
         float user_scale);

    void apply_offset(int dx, int dy);
    void set_color(Uint8 r, Uint8 g, Uint8 b);
    bool contains(int x, int y) const;
    const std::vector<Point>& get_points() const;

    bool intersects(const Area& other) const;
    bool contains_point(const std::pair<int,int>& pt) const;

    SDL_Texture* get_image(SDL_Renderer* renderer) const;
    SDL_Surface* rescale_surface(SDL_Surface* surf, float sf);

    std::tuple<int,int,int,int> get_bounds() const;

private:
    SDL_Color color{255,255,255,255};
    std::vector<Point> points;
};

#endif // AREA_HPP
