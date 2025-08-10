// === File: scene_renderer.cpp ===
#include "scene_renderer.hpp"
#include "assets.hpp"
#include "Asset.hpp"
#include "render_utils.hpp"
#include "light_map.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <tuple>
#include <vector>

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
      main_light_source_(renderer, screen_width / 2, screen_height / 2,
                         screen_width, SDL_Color{255, 255, 255, 255}, map_path),
      fullscreen_light_tex_(nullptr),
      render_asset_(renderer, util, main_light_source_, assets->player)
{
    fullscreen_light_tex_ = SDL_CreateTexture(renderer_,
                                              SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET,
                                              screen_width_,
                                              screen_height_);
    if (fullscreen_light_tex_) {
        SDL_SetTextureBlendMode(fullscreen_light_tex_, SDL_BLENDMODE_BLEND);
        SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
        SDL_SetRenderTarget(renderer_, fullscreen_light_tex_);

        SDL_Color color = main_light_source_.get_current_color();
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_RenderClear(renderer_);

        SDL_SetRenderTarget(renderer_, prev);
    } else {
        std::cerr << "[SceneRenderer] Failed to create fullscreen light texture: "
                  << SDL_GetError() << "\n";
    }

    z_light_pass_ = std::make_unique<LightMap>(renderer_,
                                               assets_,
                                               util_,
                                               main_light_source_,
                                               screen_width_,
                                               screen_height_,
                                               fullscreen_light_tex_);
}

void SceneRenderer::update_shading_groups() {
    ++current_shading_group_;
    if (current_shading_group_ > num_groups_)
        current_shading_group_ = 1;
}

bool SceneRenderer::shouldRegen(Asset* a) {
    return (a->get_shading_group() > 0 &&
            a->get_shading_group() == current_shading_group_) ||
           (!a->get_final_texture() ||
            !a->static_frame ||
            a->get_render_player_light());
}

void SceneRenderer::render() {
    update_shading_groups();

    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);

    main_light_source_.update();

    SDL_Color base = SLATE_COLOR;
    SDL_Color tinted = main_light_source_.apply_tint_to_color(base, 255);
    SDL_SetRenderDrawColor(renderer_, tinted.r, tinted.g, tinted.b, tinted.a);
    SDL_RenderClear(renderer_);

    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info) continue;

        if (shouldRegen(a)) {
            SDL_Texture* tex = render_asset_.regenerateFinalTexture(a);
            a->set_final_texture(tex);
        }

        SDL_Texture* final_tex = a->get_final_texture();
        if (final_tex) {
            int fw = 0, fh = 0;
            SDL_QueryTexture(final_tex, nullptr, nullptr, &fw, &fh);
            SDL_Point cp = util_.applyParallax(a->pos_X, a->pos_Y);
            SDL_Rect fb{ cp.x - fw/2, cp.y - fh, fw, fh };
            SDL_RenderCopyEx(renderer_,
                             final_tex,
                             nullptr,
                             &fb,
                             0,
                             nullptr,
                             a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
    }

    // Build and composite Z-index light map (lower-res, blurred, composited)
    z_light_pass_->render(debugging);

    util_.renderMinimap();
    SDL_RenderPresent(renderer_);
}
