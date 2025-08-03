#include "generate_base_shadow.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <sstream>

static constexpr int fade_start_distance = 0;
static constexpr int fade_end_distance = 800; // 50 + 1200

GenerateBaseShadow::GenerateBaseShadow(SDL_Renderer* renderer,
                                       const std::vector<Area>& zones,
                                       Assets* game_assets)
    : renderer_(renderer)
{
    if (!game_assets) return;

    const int total = static_cast<int>(game_assets->all.size());
    int count = 0;

    for (Asset& asset : game_assets->all) {
        if (!asset.info || asset.info->type != "Background") {
            asset.alpha_percentage = 1.0;
            asset.has_base_shadow = false;
            asset.gradient_shadow = true;
            continue;
        }

        int x = asset.pos_X;
        int y = asset.pos_Y;
        bool is_inside = false;

        for (const Area& zone : zones) {
            if (zone.contains_point({x, y})) {
                is_inside = true;
                break;
            }
        }

        if (is_inside) {
            asset.alpha_percentage = 1.0;
            asset.has_base_shadow = false;
            asset.gradient_shadow = true;
        } else {
            double minDist = std::numeric_limits<double>::infinity();

            for (const Area& zone : zones) {
                const auto& pts = zone.get_points();
                size_t n = pts.size();
                if (n < 2) continue;
                for (size_t i = 0; i < n; ++i) {
                    auto [x1, y1] = pts[i];
                    auto [x2, y2] = pts[(i + 1) % n];
                    double vx = x2 - x1;
                    double vy = y2 - y1;
                    double wx = x - x1;
                    double wy = y - y1;
                    double len2 = vx * vx + vy * vy;
                    double t = len2 > 0.0 ? (vx * wx + vy * wy) / len2 : 0.0;
                    t = std::clamp(t, 0.0, 1.0);
                    double projx = x1 + t * vx;
                    double projy = y1 + t * vy;
                    double dx = projx - x;
                    double dy = projy - y;
                    double d = std::hypot(dx, dy);
                    if (d < minDist) minDist = d;
                }
            }

            double alpha = 0.0;

            if (minDist <= fade_start_distance) {
                alpha = 1.0;
            } else if (minDist >= fade_end_distance) {
                alpha = 0.0;
            } else {
                double t = (minDist - fade_start_distance) / (fade_end_distance - fade_start_distance);
                alpha = std::pow(1.0 - t, 2.0); // quadratic falloff
            }

            asset.alpha_percentage = alpha;
            asset.has_shading = (alpha > 0.1);
        }

        ++count;
        std::ostringstream oss;
        oss << "[Shadow] "
            << std::left << std::setw(20) << asset.info->name
            << " pos=(" << std::setw(4) << x << "," << std::setw(4) << y << ")"
            << " alpha=" << std::setw(6) << std::fixed << std::setprecision(3) << asset.alpha_percentage;

        double percent = 100.0 * count / total;
        oss << "   [" << std::setw(5) << std::fixed << std::setprecision(1)
            << percent << "%] (" << count << "/" << total << ")\r";

        std::cout << oss.str() << std::flush;
    }

    std::cout << std::endl;
}
