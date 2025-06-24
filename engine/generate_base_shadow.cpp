// File: generate_base_shadow.cpp

#include "generate_base_shadow.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>

// === CONFIGURATION ===
static constexpr int max_fade_distance = 400;
static constexpr int max_fade_intensity = 100;

GenerateBaseShadow::GenerateBaseShadow(SDL_Renderer* renderer,
                                       const std::vector<Area>& zones,
                                       Assets* game_assets)
    : renderer_(renderer)
{
    if (!game_assets) return;

    // Flatten all zone perimeter points
    std::vector<std::pair<int,int>> all_points;
    all_points.reserve(10000);
    for (const Area& zone : zones) {
        for (auto pt : zone.get_points()) {
            all_points.emplace_back(pt);
        }
    }

    // Compute fade for each background asset (iterate over all, not just active)
    for (Asset& asset : game_assets->all) {
        if (!asset.info || asset.info->type != "Background") {
            asset.gradient_opacity = 0;
            continue;
        }

        int x = asset.pos_X;
        int y = asset.pos_Y;
        double minDist = std::numeric_limits<double>::infinity();

        for (const auto& [zx, zy] : all_points) {
            double dx = zx - x;
            double dy = zy - y;
            double d = std::hypot(dx, dy);
            if (d < minDist) minDist = d;
        }

        int opacity = (minDist >= max_fade_distance)
            ? max_fade_intensity
            : static_cast<int>(std::round(max_fade_intensity * (minDist / max_fade_distance)));

        asset.gradient_opacity = std::clamp(opacity, 0, max_fade_intensity);
        asset.has_base_shadow = true;

        std::cout << "[Shadow] " << asset.info->name
                  << " pos=(" << x << "," << y << ")"
                  << " minDist=" << minDist
                  << " -> opacity=" << asset.gradient_opacity << "\n";
    }
}
