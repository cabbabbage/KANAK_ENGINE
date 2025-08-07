#include "scene_renderer.hpp"
#include "generate_light.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
#include <vector>
#include <tuple>
#include <iostream>
#include <random>

static constexpr SDL_Color SLATE_COLOR = {50, 70, 50, 110};



void SceneRenderer::render_asset_lights_z() {
    if (debugging) std::cout << "[render_asset_lights_z] start" << std::endl;

    static std::mt19937 flicker_rng{ std::random_device{}() };

    struct LightEntry {
        SDL_Texture* tex;
        SDL_Rect dst;
        Uint8 alpha;
        SDL_RendererFlip flip;
        bool apply_tint;
    };

    std::vector<LightEntry> z_lights;

    // main light (no tint)
    SDL_Texture* main_tex = main_light_source_.get_texture();
    if (main_tex) {
        auto [mx, my] = main_light_source_.get_position();
        int main_sz   = screen_width_ * 3;
        SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
        Uint8 main_alpha = Uint8(main_light_source_.current_color_.a);
        z_lights.push_back({ main_tex, main_rect, main_alpha, SDL_FLIP_NONE, false });
    }

    // asset-specific lights (with tint)
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_light_source) continue;
        for (const auto& light : a->info->light_sources) {
            int offX = a->flipped ? -light.offset_x : light.offset_x;
            SDL_Point p = util_.applyParallax(a->pos_X + offX, a->pos_Y + light.offset_y);

            int lw, lh;
            SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
            SDL_Rect dst{ p.x - lw/2, p.y - lh/2, lw, lh };

            float alpha_f = static_cast<float>(main_light_source_.light_brightness);
            if (a == assets_->player) {
                alpha_f *= 0.9f;
            }

            if (light.flicker > 0) {
                float intensity_scale = std::clamp(light.intensity / 255.0f, 0.0f, 1.0f);
                float max_jitter = (light.flicker / 100.0f) * intensity_scale;
                std::uniform_real_distribution<float> dist(-max_jitter, max_jitter);
                float jitter = dist(flicker_rng);
                alpha_f *= (1.0f + jitter);
            }

            Uint8 alpha = static_cast<Uint8>(std::clamp(alpha_f, 0.0f, 255.0f));
            SDL_RendererFlip flip = a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            z_lights.push_back({ light.texture, dst, alpha, flip, true });
        }
    }

    // render mask
    SDL_Texture* mask = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          screen_width_, screen_height_);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, mask);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);

    for (auto& e : z_lights) {
        SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(e.tex, e.alpha);

        if (e.apply_tint) {
            SDL_Color tinted = main_light_source_.apply_tint_to_color({255, 255, 255, 255}, e.alpha);
            SDL_SetTextureColorMod(e.tex, tinted.r, tinted.g, tinted.b);
        } else {
            SDL_SetTextureColorMod(e.tex, 255, 255, 255);
        }

        SDL_RenderCopyEx(renderer_, e.tex, nullptr, &e.dst, 0, nullptr, e.flip);
        SDL_SetTextureColorMod(e.tex, 255, 255, 255);
    }

    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, mask, nullptr, nullptr);
    SDL_DestroyTexture(mask);

    if (debugging) std::cout << "[render_asset_lights_z] end" << std::endl;
}




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
                         screen_width, {255,255,255,255}, map_path)

{}


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

SDL_Texture* SceneRenderer::generateMask(Asset* a, int bw, int bh) {
    // Create mask texture
    SDL_Texture* mask = SDL_CreateTexture(renderer_,
                                          SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          bw, bh);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, mask);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 0);
    SDL_RenderClear(renderer_);

    // Silhouette of base
    SDL_Texture* base = a->get_current_frame();
    SDL_SetTextureBlendMode(base, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(base, 0, 0, 0);
    SDL_SetTextureAlphaMod(base, 255);
    SDL_RenderCopy(renderer_, base, nullptr, nullptr);
    SDL_SetTextureColorMod(base, 255, 255, 255);

    // Compute light parameters
    SDL_Point p = util_.applyParallax(a->pos_X, a->pos_Y);
    SDL_Rect bounds{ p.x - bw/2, p.y - bh, bw, bh };
    Uint8 light_alpha = main_light_source_.light_brightness;
    renderReceivedStaticLights(a, bounds, light_alpha);
    renderMovingLights(a, bounds, light_alpha);

    Uint8 main_alpha = main_light_source_.current_color_.a;
    int main_sz = screen_width_ * 3;

    renderOrbitalLights(a, bounds, main_alpha * 2);

    renderMainLight(a,main_light_source_.get_texture(),SDL_Rect{ bounds.x - main_sz, bounds.y - main_sz, main_sz * 2, main_sz * 2 },bounds,Uint8(main_alpha/3));
    

    // 80% opacity modulation
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_MOD);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 204);
    SDL_RenderFillRect(renderer_, nullptr);

    return mask;
}

SDL_Texture* SceneRenderer::regenerateFinalTexture(Asset* a) {
    SDL_Texture* base = a->get_current_frame();
    if (!base) return nullptr;

    Uint8 main_alpha = main_light_source_.current_color_.a;

    int bw, bh;
    SDL_QueryTexture(base, nullptr, nullptr, &bw, &bh);


    SDL_Texture* final_tex = SDL_CreateTexture(renderer_,
                                               SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_TARGET,
                                               bw, bh);
    SDL_SetTextureBlendMode(final_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_, final_tex);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    float c = std::pow(a->alpha_percentage, 1.0f);
    int alpha_mod = (c >= 1.0f) ? 255 : int(main_alpha * c);
    if (a->info->type == "Player") alpha_mod = std::min(255, alpha_mod * 3);

    SDL_Color mod_color = main_light_source_.apply_tint_to_color({255, 255, 255, 255}, alpha_mod);

    SDL_SetTextureColorMod(base, mod_color.r, mod_color.g, mod_color.b);
    SDL_RenderCopy(renderer_, base, nullptr, nullptr);
    SDL_SetTextureColorMod(base, 255, 255, 255);

    if (a->has_shading) {
        SDL_Texture* mask = generateMask(a, bw, bh);
        SDL_SetRenderTarget(renderer_, final_tex);
        SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
        SDL_RenderCopy(renderer_, mask, nullptr, nullptr);
        SDL_DestroyTexture(mask);
    }

    SDL_SetRenderTarget(renderer_, nullptr);
    return final_tex;
}

void SceneRenderer::render() {
    update_shading_groups();
    if (debugging) std::cout << "[render] begin\n";

    // Camera shake
    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);

    // Global light
    main_light_source_.update();

    SDL_Color base = SLATE_COLOR;
    SDL_Color tinted = main_light_source_.apply_tint_to_color(base, 255);

    SDL_SetRenderDrawColor(renderer_, tinted.r, tinted.g, tinted.b, tinted.a);
    SDL_RenderClear(renderer_);




    // Render assets
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info) continue;
        if (shouldRegen(a)) {
            SDL_Texture* tex = regenerateFinalTexture(a);
            a->set_final_texture(tex);
        }
        SDL_Texture* final_tex = a->get_final_texture();
        if (final_tex) {
            int fw, fh;
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

    render_asset_lights_z();
    util_.renderMinimap();
    SDL_RenderPresent(renderer_);
}



void SceneRenderer::renderOwnedStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a || !a->info) return;
    static std::mt19937 flicker_rng{ std::random_device{}() };

    for (const auto& light : a->info->light_sources) {
        if (!light.texture) continue;

        int offX = a->flipped ? -light.offset_x : light.offset_x;
        SDL_Point p = util_.applyParallax(a->pos_X + offX,
                                          a->pos_Y + light.offset_y);

        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{
            p.x - lw/2 - bounds.x,
            p.y - lh/2 - bounds.y,
            lw, lh
        };

        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);

        // base half-opacity for static lights
        float alpha_f = static_cast<float>(alpha) * 0.5f;

        // apply flicker if configured
        if (light.flicker > 0) {
            std::uniform_real_distribution<float> dist(
                -light.flicker / 100.0f,
                 light.flicker / 100.0f
            );
            float jitter = dist(flicker_rng);
            alpha_f *= (1.0f + jitter);
        }

        Uint8 mod_alpha = static_cast<Uint8>(
            std::clamp(alpha_f, 0.0f, 255.0f)
        );
        SDL_SetTextureAlphaMod(light.texture, mod_alpha);

        SDL_RenderCopyEx(renderer_,
                         light.texture,
                         nullptr,
                         &dst,
                         0,
                         nullptr,
                         a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

void SceneRenderer::renderMovingLights(Asset* a,
                                       const SDL_Rect& bounds,
                                       Uint8 alpha) {
    if (!assets_->player || !assets_->player->info || !a) return;
    Asset* p = assets_->player;

    for (const auto& light : p->info->light_sources) {
        // mirror offset if player is flipped
        int offX_player = p->flipped ? -light.offset_x : light.offset_x;
        int base_x = p->pos_X + offX_player;
        // if the asset receiving light is flipped, mirror the light over its center X
        int world_lx = a->flipped ? (2 * a->pos_X - base_x) : base_x;
        int world_ly = p->pos_Y + light.offset_y - p->info->z_threshold;

        // compute fade factor based on asset’s vertical position
        double factor = calculate_static_alpha_percentage(a->pos_Y, world_ly);
        Uint8 inten = static_cast<Uint8>(alpha * factor);

        SDL_Point pnt = util_.applyParallax(world_lx, world_ly);
        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst {
            pnt.x - bounds.x - lw/2,
            pnt.y - bounds.y - lh/2,
            lw, lh
        };

        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, inten);
        SDL_RenderCopyEx(renderer_,
                         light.texture,
                         nullptr,
                         &dst,
                         0,
                         nullptr,
                         a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(light.texture, 255);
    }
}

void SceneRenderer::renderOrbitalLights(Asset* a,
                                        const SDL_Rect& bounds,
                                        Uint8 alpha) {
    if (!a || !a->info) return;

    // Grab the global light's current angle
    float angle = main_light_source_.get_angle();

    // If the asset is flipped horizontally, invert the X direction
    float dir = a->flipped ? -1.0f : 1.0f;

    for (const auto& light : a->info->orbital_light_sources) {
        // Skip any entries without a valid texture or radii
        if (!light.texture || light.x_radius <= 0 || light.y_radius <= 0)
            continue;

        // Calculate the world‐space offset using an ellipse:
        //   X = cos(angle) * x_radius
        //   Y = sin(angle) * y_radius
        float lx = a->pos_X + std::cos(angle) * light.x_radius * dir;
        float ly = a->pos_Y - std::sin(angle) * light.y_radius;  // subtract sine for "up"

        // Map to screen coords
        SDL_Point p = util_.applyParallax(
            static_cast<int>(std::round(lx)),
            static_cast<int>(std::round(ly))
        );

        // Query the texture size
        int lw = 0, lh = 0;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);

        // Build a dst rect relative to the asset bounds
        SDL_Rect dst {
            p.x - lw/2 - bounds.x,
            p.y - lh/2 - bounds.y,
            lw,
            lh
        };

        // Draw it additively
        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, alpha);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &dst);
    }
}


void SceneRenderer::renderReceivedStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a) return;
    static std::mt19937 flicker_rng{ std::random_device{}() };

    for (const auto& sl : a->static_lights) {
        if (!sl.source || !sl.source->texture) continue;

        int offX = a->flipped ? -sl.offset_x : sl.offset_x;
        SDL_Point p = util_.applyParallax(a->pos_X + offX,
                                          a->pos_Y + sl.offset_y);

        int lw, lh;
        SDL_QueryTexture(sl.source->texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{
            p.x - lw/2 - bounds.x,
            p.y - lh/2 - bounds.y,
            lw, lh
        };

        SDL_SetTextureBlendMode(sl.source->texture, SDL_BLENDMODE_ADD);

        float base_alpha = static_cast<float>(alpha) * sl.alpha_percentage;
        if (sl.source->flicker > 0) {
            float brightness_scale = std::clamp(sl.source->intensity / 255.0f, 0.0f, 1.0f);
            float max_jitter = (sl.source->flicker / 100.0f) * brightness_scale;
            std::uniform_real_distribution<float> dist(-max_jitter, max_jitter);
            float jitter = dist(flicker_rng);
            base_alpha *= (1.0f + jitter);
        }

        Uint8 mod_alpha = static_cast<Uint8>(std::clamp(base_alpha, 0.0f, 255.0f));
        SDL_SetTextureAlphaMod(sl.source->texture, mod_alpha);

        SDL_RenderCopyEx(renderer_,
                         sl.source->texture,
                         nullptr,
                         &dst,
                         0,
                         nullptr,
                         a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}


void SceneRenderer::renderMainLight(Asset* a,
                                    SDL_Texture* tex,
                                    const SDL_Rect& main_rect,
                                    const SDL_Rect& bounds,
                                    Uint8 alpha) {
    if (!a || !tex) return;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_Rect dst{
        main_rect.x - bounds.x,
        main_rect.y - bounds.y,
        main_rect.w,
        main_rect.h
    };
    SDL_RenderCopyEx(renderer_,
                     tex,
                     nullptr,
                     &dst,
                     0,
                     nullptr,
                     a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

double SceneRenderer::calculate_static_alpha_percentage(int asset_y, int light_world_y) {
    constexpr int FADE_ABOVE = 180;
    constexpr int FADE_BELOW = -30;
    constexpr double MIN_OPACITY = 0.15;
    constexpr double MAX_OPACITY = 0.7;

    int delta_y = light_world_y - asset_y;
    double factor;

    if (delta_y <= -FADE_ABOVE) {
        factor = MIN_OPACITY;
    } else if (delta_y >= FADE_BELOW) {
        factor = MAX_OPACITY;
    } else {
        factor = double(delta_y + FADE_ABOVE) / double(FADE_ABOVE + FADE_BELOW);
        factor = MIN_OPACITY + (MAX_OPACITY - MIN_OPACITY) * factor;
    }

    return std::clamp(factor, MIN_OPACITY, MAX_OPACITY);
}


