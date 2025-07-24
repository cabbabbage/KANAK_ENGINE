// === File: scene_renderer.cpp ===
#include "scene_renderer.hpp"
#include <algorithm>
#include <cmath>

static constexpr SDL_Color SLATE_COLOR = {38, 44, 38, 150};


SceneRenderer::SceneRenderer(SDL_Renderer* renderer,
                             Assets* assets,
                             RenderUtils& util,
                             int screen_width,
                             int screen_height,
                             float dusk_threshold)
    : renderer_(renderer),
      assets_(assets),
      util_(util),
      screen_width_(screen_width),
      screen_height_(screen_height),
      dusk_thresh_(dusk_threshold) {}

void SceneRenderer::render() {
    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);

    SDL_SetRenderDrawColor(renderer_,
                           SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer_);

    auto* map_light = util_.getMapLight();

    for (const auto* asset : assets_->active_assets) {
        if (!asset) continue;

        if (asset->is_lit && asset->info &&
            map_light && map_light->current_color_.a > dusk_thresh_ &&
            !asset->info->light_textures.empty() &&
            !asset->info->lights.empty())
        {
            const auto& textures = asset->info->light_textures;
            const auto& lights = asset->info->lights;
            size_t count = std::min(textures.size(), lights.size());

            for (size_t i = 0; i < count; ++i) {
                SDL_Texture* light = textures[i];
                if (!light) continue;
                const auto& ld = lights[i];

                int lw, lh;
                SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
                SDL_Point lp = util_.applyParallax(asset->pos_X + ld.offset_x,
                                                   asset->pos_Y + ld.offset_y);
                SDL_Rect hl{ lp.x - lw / 2, lp.y - lh / 2, lw, lh };

                SDL_SetTextureBlendMode(light, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(light, 255, 235, 180);
                int flick = ld.flicker ? std::clamp(28 + (rand() % 12), 10, 48) : 32;
                float t = 1.0f - std::clamp(map_light->current_color_.a / dusk_thresh_, 0.0f, 1.0f);
                SDL_SetTextureAlphaMod(light, static_cast<int>(flick * t));

                util_.setLightDistortionRect(hl);
                util_.renderLightDistorted(light);
            }
        }

        util_.setAssetTrapezoid(asset, px, py);
        util_.renderAssetTrapezoid(asset->get_current_frame());
    }

    SDL_Texture* lightmap = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        screen_width_, screen_height_
    );
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer_, lightmap);
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 200);
    SDL_RenderClear(renderer_);

    for (const auto* asset : assets_->active_assets) {
        if (!asset || !asset->is_lit || !asset->info) continue;
        if (!map_light || map_light->current_color_.a <= dusk_thresh_) continue;

        const auto& textures = asset->info->light_textures;
        const auto& lights = asset->info->lights;
        size_t count = std::min(textures.size(), lights.size());

        for (size_t i = 0; i < count; ++i) {
            SDL_Texture* light = textures[i];
            if (!light) continue;
            const auto& ld = lights[i];

            int lw, lh;
            SDL_QueryTexture(light, nullptr, nullptr, &lw, &lh);
            SDL_Point lp = util_.applyParallax(asset->pos_X + ld.offset_x,
                                               asset->pos_Y + ld.offset_y);
            SDL_Rect dst{ lp.x - lw / 2, lp.y - lh / 2, lw, lh };

            SDL_SetTextureBlendMode(light, SDL_BLENDMODE_ADD);
            int flick = ld.flicker ? 235 + (rand() % 16) : 255;
            SDL_SetTextureAlphaMod(light, flick);

            if (asset->info->type == "Player" || asset->info->type == "player") {
                double angle = -0.05 * SDL_GetTicks() * 0.001;
                SDL_RenderCopyEx(renderer_, light, nullptr, &dst,
                                 angle * (180.0 / M_PI), nullptr, SDL_FLIP_NONE);
            } else {
                SDL_RenderCopy(renderer_, light, nullptr, &dst);
            }
        }
    }

    if (map_light) {
        map_light->update();
        auto [lx, ly] = map_light->get_position();
        int size = screen_width_ * 3 * 2;
        SDL_Rect dest{ lx - size / 2, ly - size / 2, size, size };
        SDL_Texture* tex = map_light->get_texture();
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(tex,
                map_light->current_color_.r,
                map_light->current_color_.g,
                map_light->current_color_.b);
            SDL_SetTextureAlphaMod(tex, map_light->current_color_.a);
            SDL_RenderCopy(renderer_, tex, nullptr, &dest);
        }
    }

    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, lightmap, nullptr, nullptr);
    SDL_DestroyTexture(lightmap);

    util_.renderMinimap();
    SDL_RenderPresent(renderer_);
}
