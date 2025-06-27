#include "generate_light.hpp"
#include "Asset.hpp"
#include <cmath>
#include <algorithm>
#include <random>

GenerateLight::GenerateLight(SDL_Renderer* renderer)
    : renderer_(renderer) {}

SDL_Texture* GenerateLight::generate(const Asset* asset) {
    if (!renderer_ || !asset || !asset->info || !asset->info->has_light_source)
        return nullptr;

    const auto* info = asset->info.get();
    const int radius = info->radius;
    const int falloff = std::clamp(info->fall_off, 0, 100);
    const SDL_Color color = info->light_color;
    const int intensity = std::clamp(info->light_intensity, 0, 255);

    const int size = radius * 2;

    SDL_Texture* texture = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size
    );
    if (!texture) return nullptr;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, texture);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    float white_core_ratio  = std::pow(1.0f - falloff / 100.0f, 2.0f);
    float white_core_radius = radius * white_core_ratio;

    // === Generate ultra-subtle, wide light rays ===
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<float> spread_dist(0.2f, 0.6f);  // much wider spread
    std::uniform_int_distribution<int> ray_count_dist(4, 7);
    int ray_count = ray_count_dist(rng);

    std::vector<std::pair<float, float>> rays;
    for (int i = 0; i < ray_count; ++i) {
        rays.emplace_back(angle_dist(rng), spread_dist(rng));
    }

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - radius + 0.5f;
            float dy = y - radius + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) continue;

            float angle = std::atan2(dy, dx);
            float ray_boost = 1.0f;

            for (const auto& [ray_angle, spread] : rays) {
                float diff = std::fabs(angle - ray_angle);
                diff = std::fmod(diff + 2 * M_PI, 2 * M_PI);
                if (diff > M_PI) diff = 2 * M_PI - diff;
                if (diff < spread) {
                    float falloff = 1.0f - (diff / spread);
                    ray_boost += falloff * 0.05f;  // extremely subtle
                }
            }

            ray_boost = std::clamp(ray_boost, 1.0f, 1.1f);  // max 10% boost

            float alpha_ratio = std::pow(1.0f - (dist / static_cast<float>(radius)), 1.4f);
            alpha_ratio = std::clamp(alpha_ratio * ray_boost, 0.0f, 1.0f);
            Uint8 alpha = static_cast<Uint8>(std::min(255.0f, intensity * alpha_ratio * 1.6f));

            SDL_Color final_color;
            if (dist <= white_core_radius) {
                final_color.r = static_cast<Uint8>((255 + color.r) / 2);
                final_color.g = static_cast<Uint8>((255 + color.g) / 2);
                final_color.b = static_cast<Uint8>((255 + color.b) / 2);
                final_color.a = alpha;
            } else {
                float t = (dist - white_core_radius) / (radius - white_core_radius);
                final_color.r = static_cast<Uint8>((1 - t) * ((255 + color.r) / 2) + t * color.r);
                final_color.g = static_cast<Uint8>((1 - t) * ((255 + color.g) / 2) + t * color.g);
                final_color.b = static_cast<Uint8>((1 - t) * ((255 + color.b) / 2) + t * color.b);
                final_color.a = alpha;
            }

            SDL_SetRenderDrawColor(renderer_, final_color.r, final_color.g, final_color.b, final_color.a);
            SDL_RenderDrawPoint(renderer_, x, y);
        }
    }

    

    SDL_SetRenderTarget(renderer_, nullptr);
    return texture;
}
