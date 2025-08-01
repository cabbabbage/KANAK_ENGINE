// === File: animation.cpp ===

#include "Animation.hpp"
#include "cache_manager.hpp"
#include <SDL_image.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

Animation::Animation() = default;

void Animation::load(const std::string& trigger,
                     const nlohmann::json& anim_json,
                     const std::string& dir_path,
                     const std::string& root_cache,
                     float scale_factor,
                     SDL_BlendMode blendmode,
                     SDL_Renderer* renderer,
                     SDL_Texture*& base_sprite,
                     int& scaled_sprite_w,
                     int& scaled_sprite_h,
                     int& original_canvas_width,
                     int& original_canvas_height)
{
    CacheManager cache;
    std::string src_folder   = dir_path + "/" + anim_json["frames_path"].get<std::string>();
    std::string cache_folder = root_cache + "/" + trigger;
    std::string meta_file    = cache_folder + "/metadata.json";

    int expected_frames = 0;
    int orig_w = 0, orig_h = 0;
    while (true) {
        std::string f = src_folder + "/" + std::to_string(expected_frames) + ".png";
        if (!fs::exists(f)) break;
        if (expected_frames == 0) {
            if (SDL_Surface* s = IMG_Load(f.c_str())) {
                orig_w = s->w;
                orig_h = s->h;
                SDL_FreeSurface(s);
            }
        }
        ++expected_frames;
    }
    if (expected_frames == 0) return;

    on_end           = anim_json.value("on_end", "");
    randomize        = anim_json.value("randomize", false);
    loop             = anim_json.value("loop", true);
    lock_until_done  = anim_json.value("lock_until_done", false);

    for (int i = 0; i < expected_frames; ++i) {
        std::string f = src_folder + "/" + std::to_string(i) + ".png";
        int new_w = 0, new_h = 0;
        SDL_Surface* scaled = cache.load_and_scale_surface(f, scale_factor, new_w, new_h);
        if (!scaled) {
            std::cerr << "[Animation] Failed to load or scale: " << f << "\n";
            continue;
        }
        SDL_SetSurfaceBlendMode(scaled, blendmode);

        if (i == 0) {
            original_canvas_width  = orig_w;
            original_canvas_height = orig_h;
            scaled_sprite_w = new_w;
            scaled_sprite_h = new_h;
        }

        SDL_Texture* tex = cache.surface_to_texture(renderer, scaled);
        if (!tex) {
            std::cerr << "[Animation] Failed to convert surface to texture at frame " << i << "\n";
            SDL_FreeSurface(scaled);
            continue;
        }

        SDL_SetTextureBlendMode(tex, blendmode);
        frames.push_back(tex);

        SDL_FreeSurface(scaled);
    }

    if (trigger == "default" && !frames.empty()) {
        base_sprite = frames[0];
    }
}

SDL_Texture* Animation::get_frame(int index) const {
    if (index < 0 || index >= static_cast<int>(frames.size())) return nullptr;
    return frames[index];
}

bool Animation::advance(int& index, std::string& next_animation_name) const {
    if (frozen || frames.empty()) return false;
    ++index;
    if (index < static_cast<int>(frames.size())) {
        return true;
    }
    if (loop) {
        index = 0;
        return true;
    } else if (!on_end.empty()) {
        next_animation_name = on_end;
        return false;
    } else {
        index = static_cast<int>(frames.size()) - 1;
        return true;
    }
}

void Animation::change(int& index, bool& static_flag) const {
    if (frozen) return;
    index = 0;
    static_flag = is_static();
}

void Animation::freeze() {
    frozen = true;
}

bool Animation::is_frozen() const {
    return frozen;
}

bool Animation::is_static() const {
    return frames.size() <= 1;
}
