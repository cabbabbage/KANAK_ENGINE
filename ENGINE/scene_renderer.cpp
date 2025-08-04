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
                         screen_width, {255,255,255,255}, map_path)

{}


void SceneRenderer::render_asset_lights_z() {
    if (debugging) std::cout << "[render_asset_lights_z] start" << std::endl;

    struct LightEntry { SDL_Texture* tex; SDL_Rect dst; Uint8 alpha; SDL_RendererFlip flip; };
    std::vector<LightEntry> z_lights;

    // main light (never flipped)
    SDL_Texture* main_tex = main_light_source_.get_texture();
    if (main_tex) {
        auto [mx, my] = main_light_source_.get_position();
        int main_sz   = screen_width_ * 3;
        SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
        Uint8 main_alpha = main_light_source_.current_color_.a;
        z_lights.push_back({ main_tex, main_rect, Uint8(main_alpha * 0.9), SDL_FLIP_NONE });
    }

    // asset‐specific lights
    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info || !a->info->has_light_source) continue;
        for (const auto& light : a->info->light_sources) {
            if (!light.texture || light.orbit_radius > 0) continue;

            // mirror offset if asset is flipped
            int offX = a->flipped ? -light.offset_x : light.offset_x;
            SDL_Point p = util_.applyParallax(a->pos_X + offX,
                                              a->pos_Y + light.offset_y);

            int lw, lh;
            SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
            SDL_Rect dst{ p.x - lw/2, p.y - lh/2, lw, lh };

            Uint8 alpha = main_light_source_.light_brightness;
            SDL_RendererFlip flip = a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            z_lights.push_back({ light.texture, dst, alpha, flip });
        }
    }

    // render into mask
    SDL_Texture* mask = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888,
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
        SDL_RenderCopyEx(renderer_, 
                         e.tex, 
                         nullptr, 
                         &e.dst, 
                         0, 
                         nullptr, 
                         e.flip);
    }

    // composite mask
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, mask, nullptr, nullptr);
    SDL_DestroyTexture(mask);

    if (debugging) std::cout << "[render_asset_lights_z] end" << std::endl;
}






void SceneRenderer::render() {
    if (debugging) std::cout << "[render] begin" << std::endl;

    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    if (debugging) std::cout << "[render] player pos=(" << px << "," << py << ")" << std::endl;

    util_.updateCameraShake(px, py);
    if (debugging) std::cout << "[render] camera shake updated" << std::endl;


    if (debugging) std::cout << "[render] shading groups updated" << std::endl;

    main_light_source_.update();
    SDL_Texture* main_tex = main_light_source_.get_texture();
    auto [mx, my]         = main_light_source_.get_position();
    int main_sz           = screen_width_ * 3;
    SDL_Rect main_rect{ mx - main_sz, my - main_sz, main_sz * 2, main_sz * 2 };
    Uint8 main_alpha      = main_light_source_.current_color_.a;
    Uint8 light_alpha     = main_light_source_.light_brightness;
    if (debugging) std::cout << "[render] main light brightness=" << int(light_alpha) << std::endl;

    SDL_SetRenderDrawColor(renderer_,
                           SLATE_COLOR.r, SLATE_COLOR.g,
                           SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer_);
    if (debugging) std::cout << "[render] cleared background" << std::endl;

    for (Asset* a : assets_->active_assets) {
        if (!a || !a->info) {
            if (debugging) std::cout << "[render] skipped invalid asset" << std::endl;
            continue;
        }
        if (debugging) std::cout << "[render] drawing asset: " << a->info->name
                                << " flipped=" << a->flipped << std::endl;

        SDL_Texture* base = a->get_current_frame();
        if (!base) {
            if (debugging) std::cout << "[render] no frame, continue" << std::endl;
            continue;
        }

        // compute screen bounds
        int bw, bh;
        SDL_QueryTexture(base, nullptr, nullptr, &bw, &bh);
        SDL_Point cp = util_.applyParallax(a->pos_X, a->pos_Y);
        SDL_Rect bounds{ cp.x - bw/2, cp.y - bh, bw, bh };

        // apply blackening tint
        float c = std::pow(a->alpha_percentage, 1.0f);
        int d = (c >= 1.0f ? 255 : static_cast<int>(main_alpha * c));
        if (a->info->type == "Player") {
            d = std::min(255, d * 3);
        }
        SDL_SetTextureColorMod(base, d, d, d);


        // draw base sprite with horizontal flip if needed
        SDL_RenderCopyEx(renderer_,
                        base,
                        nullptr,
                        &bounds,
                        0,
                        nullptr,
                        a->flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

        // reset tint
        SDL_SetTextureColorMod(base, 255, 255, 255);

        if (!a->has_shading) {
            continue;
        }

        // draw static lights, moving lights, orbital/main lights into mask
        //renderOwnedStaticLights(a, bounds, light_alpha);

        bool regen = (a->get_shading_group() == main_light_source_.get_update_index()) ||
                     (a->get_last_mask() == nullptr) ||
                     (!a->static_frame) ||
                     (a->get_render_player_light());
        if (debugging) std::cout << "[render] regen_mask=" << regen << std::endl;

        if (regen) {
            SDL_Texture* mask = SDL_CreateTexture(renderer_,
                                                  SDL_PIXELFORMAT_RGBA8888,
                                                  SDL_TEXTUREACCESS_TARGET,
                                                  bw, bh);
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer_, mask);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 0);
            SDL_RenderClear(renderer_);

            // silhouette of base
            SDL_SetTextureBlendMode(base, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(base, 0, 0, 0);
            SDL_SetTextureAlphaMod(base, 255);
            SDL_Rect sil_rect{ 0, 0, bw, bh };
            SDL_RenderCopy(renderer_, base, nullptr, &sil_rect);
            SDL_SetTextureColorMod(base, 255, 255, 255);

            renderReceivedStaticLights(a, bounds, light_alpha);
            renderMovingLights(a, bounds, light_alpha);

                     if (!a->info->orbital_light_sources.empty()) {
                renderOrbitalLights(a, bounds, main_alpha);
            } else {
                renderMainLight(a, main_tex, main_rect, bounds, main_alpha * 1.5);
            }

            SDL_SetRenderTarget(renderer_, nullptr);
            a->set_last_mask(mask);
            if (debugging) std::cout << "[render] mask regenerated" << std::endl;
        }

        // draw mask over base
        SDL_Texture* mask = a->get_last_mask();
        if (mask) {
            SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);

            SDL_RenderCopyEx(renderer_,
                             mask,
                             nullptr,
                             &bounds,
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
    for (const auto& light : a->info->light_sources) {
        if (!light.texture) continue;
        int offX = a->flipped ? -light.offset_x : light.offset_x;
        SDL_Point p = util_.applyParallax(a->pos_X + offX,
                                          a->pos_Y + light.offset_y);
        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst{ p.x - lw/2 - bounds.x,
                      p.y - lh/2 - bounds.y,
                      lw, lh };
        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, alpha / 2);
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
        if (!light.texture || light.orbit_radius > 0) continue;

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
    float angle = main_light_source_.get_angle();
    // flip orbit direction if asset is flipped
    float dir = a->flipped ? -1.0f : 1.0f;

    for (const auto& light : a->info->orbital_light_sources) {
        if (!light.texture || light.orbit_radius <= 0) continue;

        float lx = a->pos_X + std::cos(angle) * light.orbit_radius * dir;
        float ly = a->pos_Y + std::sin(angle) * light.orbit_radius;
        SDL_Point p = util_.applyParallax(static_cast<int>(lx), static_cast<int>(ly));

        int lw, lh;
        SDL_QueryTexture(light.texture, nullptr, nullptr, &lw, &lh);
        SDL_Rect dst {
            p.x - lw/2 - bounds.x,
            p.y - lh/2 - bounds.y,
            lw, lh
        };

        SDL_SetTextureBlendMode(light.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(light.texture, alpha);
        SDL_RenderCopy(renderer_, light.texture, nullptr, &dst);
    }
}



void SceneRenderer::renderReceivedStaticLights(Asset* a, const SDL_Rect& bounds, Uint8 alpha) {
    if (!a) return;
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
        Uint8 mod_alpha = static_cast<Uint8>(alpha * sl.alpha_percentage);
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
    constexpr int FADE_ABOVE = 100;
    constexpr int FADE_BELOW = -10;
    constexpr double MIN_OPACITY = 0.3;

    int delta_y = light_world_y - asset_y;
    double factor;
    if (delta_y <= -FADE_ABOVE)      factor = MIN_OPACITY;
    else if (delta_y >= FADE_BELOW)  factor = 1.0;
    else factor = double(delta_y + FADE_ABOVE) / double(FADE_ABOVE + FADE_BELOW);

    factor = std::clamp(factor, MIN_OPACITY, 1.0);
    return factor;
}