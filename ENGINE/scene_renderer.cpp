#include "scene_renderer.hpp"
#include <algorithm>
#include <cmath>

static constexpr SDL_Color SLATE_COLOR = {50, 70, 50, 110};

static float compute_light_fade(Uint8 alpha, float dusk_thresh) {
    if (alpha >= dusk_thresh) return 0.0f;
    return 1.0f - (float(alpha) / dusk_thresh);
}

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
            map_light &&
            !asset->info->light_textures.empty() &&
            !asset->info->lights.empty())
        {
            float fade_t = compute_light_fade(map_light->current_color_.a, dusk_thresh_);
            if (fade_t > 0.0f) {
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
                    SDL_SetTextureAlphaMod(light, static_cast<int>(flick * fade_t));

                    util_.setLightDistortionRect(hl);
                    util_.renderLightDistorted(light);
                }
            }
        }

        util_.setAssetTrapezoid(asset, px, py);
        SDL_RendererFlip flip = asset->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_Texture* tex = asset->get_current_frame();
        if (tex) {
            SDL_Rect dst;
            SDL_QueryTexture(tex, nullptr, nullptr, &dst.w, &dst.h);
            SDL_Point pos = util_.applyParallax(asset->pos_X, asset->pos_Y);
            dst.x = pos.x - dst.w / 2;
            dst.y = pos.y - dst.h;
            SDL_RenderCopyEx(renderer_, tex, nullptr, &dst, 0, nullptr, flip);
        }
        render_test_areas(asset->info.get(), asset->pos_X, asset->pos_Y);
    }

    SDL_Texture* lightmap = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        screen_width_, screen_height_
    );
    SDL_SetTextureBlendMode(lightmap, SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer_, lightmap);
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 100);
    SDL_RenderClear(renderer_);

    for (const auto* asset : assets_->active_assets) {
        if (!asset || !asset->is_lit || !asset->info) continue;
        if (!map_light || map_light->current_color_.a >= dusk_thresh_) continue;

        float fade_t = compute_light_fade(map_light->current_color_.a, dusk_thresh_);
        if (fade_t <= 0.0f) continue;

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
            SDL_SetTextureAlphaMod(light, static_cast<int>(flick * fade_t));

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


void SceneRenderer::render_test_areas(const AssetInfo* info, int world_x, int world_y) {
    if (!info) return;

    for (const std::string& key : test_areas) {
        const Area* area = nullptr;
        SDL_Color color = {255, 255, 255, 80}; // default fallback white

        if (key == "spacing" && info->has_spacing_area && info->spacing_area) {
            area = info->spacing_area.get();
            color = {0, 255, 0, 80};       // green
        } else if (key == "pass" && info->has_passability_area && info->passability_area) {
            area = info->passability_area.get();
            color = {255, 255, 0, 80};       // red
        } else if (key == "collision" && info->has_collision_area && info->collision_area) {
            area = info->collision_area.get();
            color = {255, 0, 255, 80};     // magenta
        } else if (key == "interaction" && info->has_interaction_area && info->interaction_area) {
            area = info->interaction_area.get();
            color = {0, 255, 255, 80};     // cyan
        } else if (key == "attack" && info->has_attack_area && info->attack_area) {
            area = info->attack_area.get();
            color = {255, 255, 0, 80};     // yellow
        }

        if (!area) continue;

        SDL_Texture* tex = area->get_texture();
        if (!tex) continue;

        auto [minx, miny, maxx, maxy] = area->get_bounds();
        int w = maxx - minx + 1;
        int h = maxy - miny + 1;

        SDL_Point screen_pos = util_.applyParallax(world_x, world_y);
        SDL_Rect dst{ screen_pos.x - w / 2, screen_pos.y - h, w, h };

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(tex, color.a);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    }
}
