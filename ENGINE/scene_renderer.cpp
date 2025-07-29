// === File: scene_renderer.cpp ===
#include "scene_renderer.hpp"
#include <algorithm>
#include <cmath>

static constexpr SDL_Color SLATE_COLOR = {50, 70, 50, 110};

SceneRenderer::SceneRenderer(SDL_Renderer* renderer,
                             Assets* assets,
                             RenderUtils& util,
                             int screen_width,
                             int screen_height,
                             std::string map_path)
    : renderer_(renderer),
      assets_(assets),
      util_(util),
      screen_width_(screen_width),
      screen_height_(screen_height),
      lightmap_(renderer, screen_width, screen_height),
      main_light_source_(renderer, screen_width / 2, screen_height / 2,
                         screen_width, {255,255,255,255}, map_path_),
      debug_renderer_(renderer, util),
      map_path_(map_path)
{
    debug_renderer_.setTestAreas({ "interaction" });
}

void SceneRenderer::setTestAreas(const std::vector<std::string>& keys) {
    debug_renderer_.setTestAreas(keys);
}

void SceneRenderer::render()
{
    int px = assets_->player ? assets_->player->pos_X : 0;
    int py = assets_->player ? assets_->player->pos_Y : 0;
    util_.updateCameraShake(px, py);

    // Update main sun/moon light
    main_light_source_.update();
    auto [lx, ly] = main_light_source_.get_position();
    int main_size = screen_width_ * 3 * 2;
    SDL_Rect main_rect{ lx - main_size/2, ly - main_size/2, main_size, main_size };
    Uint8 main_alpha = main_light_source_.current_color_.a;

    // Collect all lights (asset lights + main light) into LightMap
    std::vector<LightInstance> lights;

    // Asset lights (always added)
    for (const auto* asset : assets_->active_assets) {
        if (!asset || !asset->info || !asset->is_lit) continue;
        const auto& textures = asset->info->light_textures;
        const auto& infos = asset->info->lights;
        size_t count = std::min(textures.size(), infos.size());
        for (size_t i = 0; i < count; ++i) {
            SDL_Texture* tex = textures[i];
            if (!tex) continue;
            int lw, lh;
            SDL_QueryTexture(tex, nullptr, nullptr, &lw, &lh);
            SDL_Point p = util_.applyParallax(
                asset->pos_X + infos[i].offset_x,
                asset->pos_Y + infos[i].offset_y);
            SDL_Rect dst{ p.x - lw/2, p.y - lh/2, lw, lh };
            lights.push_back({ tex, dst, infos[i].flicker ? 255 : 255 });
        }
    }

    // Always add main light source
    if (main_light_source_.get_texture()) {
        lights.push_back({ main_light_source_.get_texture(), main_rect, main_alpha });
    }

    // Update LightMap and clear scene
    lightmap_.setLights(lights);
    lightmap_.update();

    SDL_SetRenderDrawColor(renderer_, SLATE_COLOR.r, SLATE_COLOR.g, SLATE_COLOR.b, SLATE_COLOR.a);
    SDL_RenderClear(renderer_);

    // Render all assets
    for (const auto* asset : assets_->active_assets) {
        if (!asset) continue;
        util_.setAssetTrapezoid(asset, px, py);
        util_.renderAssetTrapezoid(asset->get_current_frame());
    }

    // Composite LightMap over scene
    SDL_SetTextureBlendMode(lightmap_.getTexture(), SDL_BLENDMODE_MOD);
    SDL_RenderCopy(renderer_, lightmap_.getTexture(), nullptr, nullptr);

    util_.renderMinimap();
    SDL_RenderPresent(renderer_);
}
