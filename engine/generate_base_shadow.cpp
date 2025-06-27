// File: generate_base_shadow.cpp

#include "generate_base_shadow.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <sstream>

static constexpr int max_fade_distance = 800;
static constexpr int max_fade_intensity = 100;
static constexpr int PERIMETER_SKIP_STEP = 5;
static constexpr int PRINT_RATE = 20;

GenerateBaseShadow::GenerateBaseShadow(SDL_Renderer* renderer,
                                       const std::vector<Area>& zones,
                                       Assets* game_assets)
    : renderer_(renderer)
{
    if (!game_assets) return;

    std::vector<std::pair<int, int>> all_points;
    all_points.reserve(10000);
    for (const Area& zone : zones) {
        const auto& pts = zone.get_points();
        for (size_t i = 0; i < pts.size(); i += PERIMETER_SKIP_STEP) {
            all_points.emplace_back(pts[i]);
        }
    }

    const int total = static_cast<int>(game_assets->all.size());
    int count = 0;

    for (Asset& asset : game_assets->all) {
        if (!asset.info || asset.info->type != "Background") {
            asset.gradient_opacity = 1.0;
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
            asset.gradient_opacity = 1.0;
        } else {
            double minDist = std::numeric_limits<double>::infinity();
            for (const auto& [zx, zy] : all_points) {
                double dx = zx - x;
                double dy = zy - y;
                double d = std::hypot(dx, dy);
                if (d < minDist) minDist = d;
            }

            int raw_opacity = (minDist >= max_fade_distance)
                ? max_fade_intensity
                : static_cast<int>(std::round(max_fade_intensity * (minDist / max_fade_distance)));

            double reversed = 1.0 - std::clamp(raw_opacity, 0, max_fade_intensity) / 100.0;
            asset.gradient_opacity = reversed;
        }

        asset.has_base_shadow = true;
        ++count;

        std::ostringstream oss;
        oss << "[Shadow] "
            << std::left << std::setw(20) << asset.info->name
            << " pos=(" << std::setw(4) << x << "," << std::setw(4) << y << ")"
            << " opacity=" << std::setw(6) << std::fixed << std::setprecision(3) << asset.gradient_opacity;

        double percent = 100.0 * count / total;
        oss << "   [" << std::setw(5) << std::fixed << std::setprecision(1)
            << percent << "%] (" << count << "/" << total << ")\r";

        std::cout << oss.str() << std::flush;
    }
    std::cout << std::endl;
}
