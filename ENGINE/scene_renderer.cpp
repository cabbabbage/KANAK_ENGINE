// === File: scene_renderer.cpp ===
#include "scene_renderer.hpp"
#include "generate_light.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
#include <vector>
#include <tuple>
#include <iostream>

static constexpr SDL_Color SLATE_COLOR = {50, 70, 50, 110};

SceneRenderer::SceneRenderer(SDL_Renderer* renderer,
                             Assets* assets,
                             RenderUtils& util,
                             int screen_width,
                             int screen_height,
                             const std::string& map_path)
    : map_path_(map_path),
      renderer_(renderer),
      assets_(assets),
      util_(util),
      screen_width_(screen_width),
      screen_height_(screen_height),
      main_light_source_(renderer, screen_width/2, screen_height/2,
                         screen_width, {255,255,255,255}, map_path),
      num_groups_(0)
{}

void SceneRenderer::update_shading_groups() {
    int current_group = 0;
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->has_shading) continue;
        if (a->is_shading_group_set()) continue;
        a->set_shading_group(current_group);
        current_group = (current_group + 1) % num_groups_;
    }
}

void SceneRenderer::render_asset_lights_z() {
    // Collect Z‐sorted lights into a vector of (texture, dst, alpha)
    std::vector<std::tuple<SDL_Texture*, SDL_Rect, Uint8>> z_lights;

    // Include main light last
    SDL_Texture* main_tex = main_light_source_.get_texture();
    if (main_tex) {
        auto [mx, my] = main_light_source_.get_position();
        int main_sz   = screen_width_ * 3;
        SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
        Uint8 main_alpha = main_light_source_.current_color_.a;
        z_lights.emplace_back(main_tex, main_rect, main_alpha * 0.8);
    }

    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_light_source) continue;

        for (const auto& light : a->info->light_sources) {
            if (!light.texture || light.orbit_radius > 0) continue;

            SDL_Point p = util_.applyParallax(
                a->pos_X + light.offset_x,
                a->pos_Y + light.offset_y
            );

            int lw, lh;
            SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
            SDL_Rect dst{ p.x - lw / 2, p.y - lh / 2, lw, lh };

            Uint8 alpha = main_light_source_.light_brightness;
            z_lights.emplace_back(light.texture, dst, alpha);
        }
    }

    // Create offscreen mask
    SDL_Texture* mask = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          screen_width_, screen_height_);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, mask);

    // Clear to black
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    // Draw all lights to mask
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);
    for (const auto& zl : z_lights) {
        SDL_Texture* tex    = std::get<0>(zl);
        const SDL_Rect& dst = std::get<1>(zl);
        Uint8 alpha         = std::get<2>(zl);

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    }

    // Restore screen target
    SDL_SetRenderTarget(renderer_, nullptr);

    // Composite: modulate the screen with mask
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, mask, nullptr, nullptr);
    SDL_DestroyTexture(mask);
}

void SceneRenderer::render() {
    bool in_front = false;

    // === 1) Camera shake & shading groups ===
    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);
    update_shading_groups();

    // === 2) Main light update ===
    main_light_source_.update();
    SDL_Texture* main_tex = main_light_source_.get_texture();
    auto [mx, my]         = main_light_source_.get_position();
    int main_sz           = screen_width_ * 3;
    SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
    Uint8 main_alpha      = main_light_source_.current_color_.a;
    Uint8 light_alpha     = main_light_source_.light_brightness;

    SDL_SetRenderDrawColor(renderer_,
                           SLATE_COLOR.r, SLATE_COLOR.g,
                           SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer_);

    // === 3) Render all assets in z_index order ===
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info) continue;

        util_.setAssetTrapezoid(a, px, py);

        if (a == assets_->player) {
            player_shade_percent = 0.8;
            //in_front = true;
        }

        SDL_Texture* base = a->get_current_frame();
        if (!base) continue;

        if (!a->has_shading) {
            // === Simple base render for non-shaded asset ===
            util_.renderAssetTrapezoid(base);
            continue;
        }

        // === Shaded asset with masking and lighting ===
        int bw, bh;
        SDL_QueryTexture(base, nullptr, nullptr, &bw, &bh);
        SDL_Point cp = util_.applyParallax(a->pos_X, a->pos_Y);
        SDL_Rect bounds{ cp.x - bw / 2, cp.y - bh, bw, bh };

        renderOwnedStaticLights(a, bounds, light_alpha);

        bool regen = (a->get_shading_group() == main_light_source_.get_update_index()) ||
                     (a->get_last_mask() == nullptr) ||
                     (!a->static_frame);

        if (regen) {
            SDL_Texture* mask = SDL_CreateTexture(renderer_,
                                                  SDL_PIXELFORMAT_RGBA8888,
                                                  SDL_TEXTUREACCESS_TARGET,
                                                  bw, bh);
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer_, mask);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderClear(renderer_);

            // Silhouette
            SDL_SetTextureBlendMode(base, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(base, 0, 0, 0);
            SDL_SetTextureAlphaMod(base, 255);
            SDL_Rect sil_rect{ 0, 0, bw, bh };
            SDL_RenderCopy(renderer_, base, nullptr, &sil_rect);
            SDL_SetTextureColorMod(base, 255, 255, 200);

            // Lights
            renderReceivedStaticLights(a, bounds, light_alpha);



            

            if (!a->info->orbital_light_sources.empty()) {
                renderOrbitalLights(a, bounds, main_alpha);
            } else {
                renderMainLight(main_tex, main_rect, bounds, main_alpha);
            }
            //renderMovingLights(a, bounds, light_alpha);
            SDL_SetRenderTarget(renderer_, nullptr);
            a->set_last_mask(mask);
        }

        // === Draw asset with modulated mask ===
        util_.renderAssetTrapezoid(base);
        SDL_Texture* mask = a->get_last_mask();
        if (mask) {
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
            SDL_RenderCopy(renderer_, mask, nullptr, &bounds);
        }
    }

    // === 4) Additive light overlays (orbital, static) ===
    render_asset_lights_z();

    // === 5) Present final frame ===
    SDL_RenderPresent(renderer_);
}

void SceneRenderer::renderOwnedStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a || !a->info) return;

    for (const auto& light : a->info->light_sources) {
        if (!light.texture) continue;

        SDL_Point p = util_.applyParallax(a->pos_X + light.offset_x,
                                          a->pos_Y + light.offset_y);

        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{ p.x - lw / 2 - bounds.x, p.y - lh / 2 - bounds.y, lw, lh };

        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, alpha/2);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &dst);
    }
}

void SceneRenderer::renderReceivedStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a) return;

    for (const auto& sl : a->static_lights) {
        if (!sl.source || !sl.source->texture) continue;

        SDL_Point p = util_.applyParallax(a->pos_X + sl.offset_x,
                                          a->pos_Y + sl.offset_y);

        int lw, lh;
        SDL_QueryTexture(sl.source->texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{ p.x - lw / 2 - bounds.x, p.y - lh / 2 - bounds.y, lw, lh };

        SDL_SetTextureBlendMode(sl.source->texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(sl.source->texture, alpha * sl.alpha_percentage);
        SDL_RenderCopy(renderer_, sl.source->texture, nullptr, &dst);
    }
}

void SceneRenderer::renderMovingLights(Asset* a,
                                       const SDL_Rect& bounds,
                                       Uint8 alpha) {
    if (!assets_->player || !assets_->player->info || !a )
        return;

    Asset* p = assets_->player;

    // fully off when player is ≥30px above the asset
    // fully on  when player is ≥10px below the asset
    constexpr int FADE_ABOVE = 30;
    constexpr int FADE_BELOW = 10;

    for (const auto& light : p->info->light_sources) {
        if (!light.texture || light.orbit_radius > 0) continue;

        // world‐space position of this player light
        int world_lx = p->pos_X + light.offset_x;
        int world_ly = p->pos_Y + light.offset_y;

        // vertical difference: positive means light is below asset
        int delta_y = world_ly - a->pos_Y;

        // compute fade factor ∈ [0..1]
        double factor;
        if (delta_y <= -FADE_ABOVE)      factor = 0.80;
        else if (delta_y >=  FADE_BELOW) factor = 1.0;
        else {
            factor = double(delta_y + FADE_ABOVE)
                   / double(FADE_ABOVE + FADE_BELOW);
        }
        factor = std::clamp(factor, 0.80, 1.0);

        // final intensity inside mask
        Uint8 inten = Uint8(alpha * factor);

        // map to mask‐local coords
        SDL_Point pnt = util_.applyParallax(world_lx, world_ly);
        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);

        SDL_Rect dst {
            pnt.x - bounds.x - lw/2,
            pnt.y - bounds.y - lh/2,
            lw, lh
        };

        // draw additive into mask
        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, inten);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &dst);
        SDL_SetTextureAlphaMod(light.texture, 255);
    }
}





void SceneRenderer::renderOrbitalLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a || !a->info) return;

    float angle = main_light_source_.get_angle();

    for (const auto& light : a->info->orbital_light_sources) {
        if (!light.texture || light.orbit_radius <= 0) continue;

        float lx = a->pos_X + std::cos(angle) * light.orbit_radius;
        float ly = a->pos_Y + std::sin(angle) * light.orbit_radius;

        SDL_Point p = util_.applyParallax(int(lx), int(ly));
        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{ p.x - lw / 2 - bounds.x, p.y - lh / 2 - bounds.y, lw, lh };

        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, alpha);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &dst);
    }
}
    // 4) Fallback main light
void SceneRenderer::renderMainLight(SDL_Texture* tex,
                                    const SDL_Rect& main_rect,
                                    const SDL_Rect& bounds,
                                    Uint8 alpha) {
        if (!tex) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_Rect dst{ main_rect.x - bounds.x, main_rect.y - bounds.y, main_rect.w, main_rect.h };
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    }

double SceneRenderer::calculate_static_alpha_percentage(int asset_y, int light_world_y) {
    const int upper_threshold = 10;
    const int lower_threshold = -30;
    int dy = light_world_y - asset_y;

    if (dy >= upper_threshold) return 1.0;
    if (dy <= lower_threshold) return 0.5;

    // Linear interpolation between 0.5 and 1.0
    double t = static_cast<double>(dy - lower_threshold) / (upper_threshold - lower_threshold);
    return 0.5 + 0.5 * std::clamp(t, 0.0, 1.0);
}
