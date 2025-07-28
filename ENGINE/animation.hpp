#pragma once

#include <string>
#include <vector>
#include <SDL.h>
#include <nlohmann/json.hpp>

class Animation {
public:
    Animation();

    void load(const std::string& trigger,
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
              int& original_canvas_height);

    SDL_Texture* get_frame(int index) const;
    bool advance(int& index, std::string& next_animation_name) const;
    void change(int& index, bool& static_flag) const;
    bool is_static() const;

    void freeze();        // Prevents further updates or changes
    bool is_frozen() const;

    std::vector<SDL_Texture*> frames;
    std::string on_end;
    bool loop = false;
    bool randomize = false;
    bool lock_until_done = false;

private:
    bool frozen = false;
};
