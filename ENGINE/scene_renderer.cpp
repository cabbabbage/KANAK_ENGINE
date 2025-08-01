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
      num_groups_(main_light_source_.get_update_interval())
{}

void SceneRenderer::update_shading_groups() {
    int current_group = 0;
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_shading) continue;
        if (a->is_shading_group_set()) continue;
        a->set_shading_group(current_group);
        current_group = (current_group + 1) % num_groups_;
    }
}

void SceneRenderer::render_asset_lights_z() {
    // Collect Z‐sorted lights into a vector of (texture, dst, alpha)
    std::vector<std::tuple<SDL_Texture*, SDL_Rect, Uint8>> z_lights;
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_light_source) continue;
        const auto& src = a->info->lights;
        const auto& tx  = a->info->light_textures;
        size_t cnt      = std::min(src.size(), tx.size());
        for (size_t i = 0; i < cnt; ++i) {
            if (!tx[i]) continue;
            if(src[i].orbit_radius > 0) continue;
            SDL_Point p = util_.applyParallax(
                a->pos_X + src[i].offset_x,
                a->pos_Y + src[i].offset_y
            );
            int lw, lh;
            SDL_QueryTexture(tx[i], nullptr, nullptr, &lw, &lh);
            SDL_Rect lightRect{ p.x - lw/2, p.y - lh/2, lw, lh };
            //Uint8 alpha = static_cast<Uint8>(std::clamp(src[i].intensity, 0, alp));
            z_lights.emplace_back(tx[i], lightRect, main_light_source_.light_brightness);
        }
    }
    // include main light last
    SDL_Texture* main_tex = main_light_source_.get_texture();
    if (main_tex) {
        auto [mx, my] = main_light_source_.get_position();
        int main_sz   = screen_width_ * 3;
        SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz*2, main_sz*2 };
        Uint8 main_alpha = main_light_source_.current_color_.a;
        z_lights.emplace_back(main_tex, main_rect, main_alpha);
    }

    // 5) Global mask pass: black full‐screen, then "punch" transparent holes for lights
    // a) create offscreen mask
    SDL_Texture* mask = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        screen_width_, screen_height_);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, mask);

    // b) clear to black (mask = 0 => final scene=0)
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    // c) draw lights onto mask as white holes (mask=255 => reveal scene)
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);
    for (auto& zl : z_lights) {
        SDL_Texture* tex    = std::get<0>(zl);
        const SDL_Rect& dst = std::get<1>(zl);
        Uint8 alpha         = std::get<2>(zl);

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    }

    // d) restore target to screen
    SDL_SetRenderTarget(renderer_, nullptr);

    // 6) Composite mask mod‐blend: black * scene = black, white * scene = scene
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, mask, nullptr, nullptr);
    SDL_DestroyTexture(mask);
}

// Updated render() in scene_renderer.cpp

void SceneRenderer::render() {
    // 1) Camera shake & shading groups
    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);
    update_shading_groups();

    // 2) Main light update
    main_light_source_.update();
    SDL_Texture* main_tex = main_light_source_.get_texture();
    auto [mx, my]         = main_light_source_.get_position();
    int main_sz           = screen_width_ * 3;
    SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
    Uint8 main_alpha      = main_light_source_.current_color_.a;
    Uint8 light_alpha     = main_light_source_.light_brightness;

    // 3) Clear to slate
    SDL_SetRenderDrawColor(renderer_,
                           SLATE_COLOR.r, SLATE_COLOR.g,
                           SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer_);

    // 4A) PASS A – non-shaded assets
    for (Asset* a : assets_->active_assets) {
        if (!a || (a->info && a->info->has_shading)) continue;
        util_.setAssetTrapezoid(a, px, py);
        if (SDL_Texture* base = a->get_current_frame())
            util_.renderAssetTrapezoid(base);
    }

    // 4B) PASS B – shaded assets with masks
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_shading) continue;
        SDL_Texture* base = a->get_current_frame();
        if (!base) continue;

        int bw, bh;
        SDL_QueryTexture(base, nullptr, nullptr, &bw, &bh);
        SDL_Point cp = util_.applyParallax(a->pos_X, a->pos_Y);
        SDL_Rect bounds{ cp.x - bw/2, cp.y - bh, bw, bh };

        // draw solid base
        util_.setAssetTrapezoid(a, px, py);
        util_.renderAssetTrapezoid(base);

        bool regen = (a->get_shading_group() == main_light_source_.get_update_index())
                  || (a->get_last_mask() == nullptr);
        if (regen) {
            SDL_Texture* mask = SDL_CreateTexture(renderer_,
                                                  SDL_PIXELFORMAT_RGBA8888,
                                                  SDL_TEXTUREACCESS_TARGET,
                                                  bw, bh);
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer_, mask);
            SDL_SetRenderDrawColor(renderer_, 255,255,255,255);
            SDL_RenderClear(renderer_);

            // silhouette
            SDL_SetTextureBlendMode(base, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(base, 0,0,0);
            SDL_SetTextureAlphaMod(base, 255);
            SDL_Rect silRect{0, 0, bw, bh};
            SDL_RenderCopy(renderer_, base, nullptr, &silRect);

            SDL_SetTextureColorMod(base, 255,255,255);

            // shading passes
            renderStaticLights(a, bounds, light_alpha);
            renderMovingLights(bounds, light_alpha);
            renderOrbitalLights(a, bounds, main_alpha);
            renderMainLight(main_tex, main_rect, bounds, main_alpha);

            SDL_SetRenderTarget(renderer_, nullptr);
            a->set_last_mask(mask);
        }

        SDL_Texture* mask = regen ? a->get_last_mask() : a->get_last_mask();
        if (mask) {
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
            SDL_RenderCopy(renderer_, mask, nullptr, &bounds);
        }
    }

    render_asset_lights_z();
    util_.renderMinimap();
    SDL_RenderPresent(renderer_);
}

// 1) Static lights on asset
void SceneRenderer::renderStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    for (auto& sl : a->static_light_sources) {
        SDL_SetTextureBlendMode(sl.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(sl.texture, alpha);
        SDL_Rect dst{ sl.world_x - bounds.x, sl.world_y - bounds.y, sl.width, sl.height };
        SDL_RenderCopy(renderer_, sl.texture, nullptr, &dst);
    }
}

// 2) Moving (player) lights
void SceneRenderer::renderMovingLights(const SDL_Rect& bounds, Uint8 alpha) {
    if (!assets_->player) return;
    for (auto& sl : assets_->player->static_light_sources) {
        SDL_SetTextureBlendMode(sl.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(sl.texture, alpha);
        SDL_Rect dst{ sl.world_x - bounds.x, sl.world_y - bounds.y, sl.width, sl.height };
        SDL_RenderCopy(renderer_, sl.texture, nullptr, &dst);
    }
}

// 3) Orbital lights
void SceneRenderer::renderOrbitalLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    const auto& texs = a->info->light_textures;
    const auto& infos = a->info->lights;
    size_t cnt = std::min(texs.size(), infos.size());
    float angle = main_light_source_.get_angle();
    for (size_t i = 0; i < cnt; ++i) {
        if (!texs[i] || infos[i].orbit_radius <= 0) continue;
        float lx = a->pos_X + std::cos(angle) * infos[i].orbit_radius;
        float ly = a->pos_Y + std::sin(angle) * infos[i].orbit_radius;
        SDL_Point p = util_.applyParallax(int(lx), int(ly));
        int lw, lh;
        SDL_QueryTexture(texs[i], nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{ p.x - lw/2 - bounds.x, p.y - lh/2 - bounds.y, lw, lh };
        SDL_SetTextureBlendMode(texs[i], SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(texs[i], alpha);
        SDL_RenderCopy(renderer_, texs[i], nullptr, &dst);
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
