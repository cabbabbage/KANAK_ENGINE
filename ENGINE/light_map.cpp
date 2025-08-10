// === File: light_z_pass.cpp ===
#include "light_map.hpp"
#include <algorithm>
#include <random>
#include <vector>
#include <iostream>

LightMap::LightMap(SDL_Renderer* renderer,
                       Assets* assets,
                       RenderUtils& util,
                       Global_Light_Source& main_light,
                       int screen_width,
                       int screen_height,
                       SDL_Texture* fullscreen_light_tex)
    : renderer_(renderer),
      assets_(assets),
      util_(util),
      main_light_(main_light),
      screen_width_(screen_width),
      screen_height_(screen_height),
      fullscreen_light_tex_(fullscreen_light_tex)
{}

void LightMap::render(bool debugging) {
    if (debugging) std::cout << "[render_asset_lights_z] start" << std::endl;

    static std::mt19937 flicker_rng{ std::random_device{}() };

    std::vector<LightEntry> z_lights;
    collect_layers(z_lights, flicker_rng);

    const int downscale = 4;
    int low_w = std::max(1, screen_width_ / downscale);
    int low_h = std::max(1, screen_height_ / downscale);

    SDL_Texture* lowres_mask = build_lowres_mask(z_lights, low_w, low_h, downscale);

    // Read back + blur
    SDL_Texture* blurred = blur_texture(lowres_mask, low_w, low_h);

    // Composite to screen
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_Rect dst_rect{ 0, 0, screen_width_, screen_height_ };
    SDL_RenderCopy(renderer_, blurred, nullptr, &dst_rect);

    SDL_DestroyTexture(blurred);
    SDL_DestroyTexture(lowres_mask);

    if (debugging) std::cout << "[render_asset_lights_z] end" << std::endl;
}

void LightMap::collect_layers(std::vector<LightEntry>& out, std::mt19937& rng) {
    // Fullscreen light layer
    Uint8 main_alpha = main_light_.get_current_color().a;
    if (fullscreen_light_tex_) {
        SDL_Rect full_dst{ 0, 0, screen_width_, screen_height_ };
        out.push_back({ fullscreen_light_tex_, full_dst, static_cast<Uint8>(main_alpha / 2), SDL_FLIP_NONE, false });
    }

    // Main orbital/map light
    SDL_Texture* main_tex = main_light_.get_texture();
    if (main_tex) {
        auto [mx, my] = main_light_.get_position();
        int main_sz = screen_width_ * 3;
        SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
        out.push_back({ main_tex, main_rect, main_alpha, SDL_FLIP_NONE, false });
    }

    // Asset lights
    static std::mt19937 flicker_rng{ std::random_device{}() };
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_light_source) continue;

        for (const auto& light : a->info->light_sources) {
            if (!light.texture) continue;

            int offX = a->flipped ? -light.offset_x : light.offset_x;
            SDL_Point p = util_.applyParallax(a->pos_X + offX, a->pos_Y + light.offset_y);

            int lw, lh;
            SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
            SDL_Rect dst{ p.x - lw / 2, p.y - lh / 2, lw, lh };

            float alpha_f = static_cast<float>(main_light_.get_brightness());
            if (a == assets_->player) alpha_f *= 0.9f;

            if (light.flicker > 0) {
                float intensity_scale = std::clamp(light.intensity / 255.0f, 0.0f, 1.0f);
                float max_jitter = (light.flicker / 100.0f) * intensity_scale;
                std::uniform_real_distribution<float> dist(-max_jitter, max_jitter);
                float jitter = dist(flicker_rng);
                alpha_f *= (1.0f + jitter);
            }

            Uint8 alpha = static_cast<Uint8>(std::clamp(alpha_f, 0.0f, 255.0f));
            SDL_RendererFlip flip = a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            out.push_back({ light.texture, dst, alpha, flip, true });
        }
    }
}

SDL_Texture* LightMap::build_lowres_mask(const std::vector<LightEntry>& layers, int low_w, int low_h, int downscale) {
    SDL_Texture* lowres_mask = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                                 SDL_TEXTUREACCESS_TARGET, low_w, low_h);
    SDL_SetTextureBlendMode(lowres_mask, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, lowres_mask);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);

    for (auto& e : layers) {
        SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(e.tex, e.alpha);

        if (e.apply_tint) {
            SDL_Color tinted = main_light_.apply_tint_to_color({255, 255, 255, 255}, e.alpha);
            SDL_SetTextureColorMod(e.tex, tinted.r, tinted.g, tinted.b);
        } else {
            SDL_SetTextureColorMod(e.tex, 255, 255, 255);
        }

        SDL_Rect scaled_dst{
            e.dst.x / downscale,
            e.dst.y / downscale,
            e.dst.w / downscale,
            e.dst.h / downscale
        };
        SDL_RenderCopyEx(renderer_, e.tex, nullptr, &scaled_dst, 0, nullptr, e.flip);
        SDL_SetTextureColorMod(e.tex, 255, 255, 255);
    }

    return lowres_mask;
}

SDL_Texture* LightMap::blur_texture(SDL_Texture* source_tex, int w, int h) {
    // Read back
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA8888, surf->pixels, surf->pitch);

    Uint32* pixels = static_cast<Uint32*>(surf->pixels);
    std::vector<Uint32> temp(pixels, pixels + w * h);

    const int blur_radius = 4;
    std::mt19937 blur_rng{ std::random_device{}() };
    std::uniform_real_distribution<float> weight_dist(0.5f, 2.15f);

    // Horizontal pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            float total_weight = 0;

            for (int k = -blur_radius; k <= blur_radius; ++k) {
                int nx = std::clamp(x + k, 0, w - 1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(temp[y * w + nx], surf->format, &pr, &pg, &pb, &pa);

                float weight = weight_dist(blur_rng);
                r += pr * weight;
                g += pg * weight;
                b += pb * weight;
                a += pa * weight;
                total_weight += weight;
            }

            pixels[y * w + x] = SDL_MapRGBA(surf->format,
                                            static_cast<Uint8>(r / total_weight),
                                            static_cast<Uint8>(g / total_weight),
                                            static_cast<Uint8>(b / total_weight),
                                            static_cast<Uint8>(a / total_weight));
        }
    }

    // Vertical pass
    temp.assign(pixels, pixels + w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            float total_weight = 0;

            for (int k = -blur_radius; k <= blur_radius; ++k) {
                int ny = std::clamp(y + k, 0, h - 1);
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(temp[ny * w + x], surf->format, &pr, &pg, &pb, &pa);

                float weight = weight_dist(blur_rng);
                r += pr * weight;
                g += pg * weight;
                b += pb * weight;
                a += pa * weight;
                total_weight += weight;
            }

            pixels[y * w + x] = SDL_MapRGBA(surf->format,
                                            static_cast<Uint8>(r / total_weight),
                                            static_cast<Uint8>(g / total_weight),
                                            static_cast<Uint8>(b / total_weight),
                                            static_cast<Uint8>(a / total_weight));
        }
    }

    SDL_Texture* blurred = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_SetTextureBlendMode(blurred, SDL_BLENDMODE_MOD);
    SDL_FreeSurface(surf);
    return blurred;
}
