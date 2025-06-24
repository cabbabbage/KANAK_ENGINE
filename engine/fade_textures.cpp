#include "fade_textures.hpp"
#include <cmath>
#include <iostream>
#include <limits>

FadeTextureGenerator::FadeTextureGenerator(SDL_Renderer* renderer, SDL_Color color, double expand)
    : renderer_(renderer), color_(color), expand_(expand) {}

std::vector<std::pair<SDL_Texture*, SDL_Rect>> FadeTextureGenerator::generate_all(const std::vector<Area>& areas) {
    std::vector<std::pair<SDL_Texture*, SDL_Rect>> results;

    size_t index = 0;
    for (const Area& area : areas) {
        std::cout << "  [FadeGen " << index << "] Starting...\n";

        auto [ominx, ominy, omaxx, omaxy] = area.get_bounds();
        int ow = omaxx - ominx + 1;
        int oh = omaxy - ominy + 1;
        if (ow <= 0 || oh <= 0) {
            std::cout << "    [FadeGen " << index << "] Invalid area bounds; skipping.\n";
            ++index;
            continue;
        }

        float base_expand = 0.2f * static_cast<float>(std::min(ow, oh));
        base_expand = std::max(base_expand, 1.0f);
        int fw = static_cast<int>(std::ceil(base_expand * expand_));

        int minx = ominx - fw;
        int miny = ominy - fw;
        int maxx = omaxx + fw;
        int maxy = omaxy + fw;
        int w = maxx - minx + 1;
        int h = maxy - miny + 1;
        if (w <= 0 || h <= 0) {
            std::cout << "    [FadeGen " << index << "] Invalid final size; skipping.\n";
            ++index;
            continue;
        }

        std::vector<std::pair<double, double>> poly;
        for (auto& [x, y] : area.get_points())
            poly.emplace_back(x - minx, y - miny);

        auto point_in_poly = [&](double px, double py) {
            bool inside = false;
            size_t n = poly.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                auto [xi, yi] = poly[i];
                auto [xj, yj] = poly[j];
                bool intersect = ((yi > py) != (yj > py)) &&
                                 (px < (xj - xi) * (py - yi) / (yj - yi + 1e-9) + xi);
                if (intersect) inside = !inside;
            }
            return inside;
        };

        SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!tex) {
            std::cout << "    [FadeGen " << index << "] Texture creation failed; skipping.\n";
            ++index;
            continue;
        }

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(renderer_, tex);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
        SDL_RenderClear(renderer_);
        SDL_SetRenderDrawColor(renderer_, color_.r, color_.g, color_.b, 255);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (point_in_poly(x + 0.5, y + 0.5)) {
                    SDL_RenderDrawPoint(renderer_, x, y);
                }
            }
        }

        SDL_SetRenderTarget(renderer_, nullptr);

        SDL_Rect dst = { minx, miny, w, h };
        results.emplace_back(tex, dst);

        std::cout << "    [FadeGen " << index << "] Texture stored. Size = " << w << "x" << h << "\n";
        ++index;
    }

    return results;
}
