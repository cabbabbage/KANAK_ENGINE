#ifndef ASSET_HPP
#define ASSET_HPP

#include "AssetInfo.hpp"
#include "Area.hpp" // placeholder for your upcoming Area class
#include <SDL.h>
#include <string>

class Asset {
public:
    int pos_X = 0;
    int pos_Y = 0;
    int z_index = 0;
    int z_offset = 0;
    Area spawn_area; // will default to Area::map_area later
    AssetInfo* info = nullptr;

    std::string current_animation = "default";
    int current_frame_index = 0;

    Asset(int z_offset = 0, const Area& spawn_area = Area())
        : z_offset(z_offset), spawn_area(spawn_area) {}

    void update() {
        if (!info) return;
        auto& anim = info->animations[current_animation];
        if (anim.frames.empty()) return;

        current_frame_index++;
        if (current_frame_index >= anim.frames.size()) {
            if (anim.loop) {
                current_frame_index = 0;
            } else {
                if (!anim.on_end.empty()) {
                    current_animation = anim.on_end;
                    current_frame_index = 0;
                } else {
                    current_frame_index = anim.frames.size() - 1;
                }
            }
        }
    }

    void change_animation(const std::string& animation_name) {
        if (!info) return;
        auto it = info->animations.find(animation_name);
        if (it != info->animations.end()) {
            current_animation = animation_name;
            current_frame_index = 0;
        }
    }

    SDL_Texture* get_current_frame() const {
        if (!info) return nullptr;
        const auto& anim = info->animations.at(current_animation);
        if (anim.frames.empty()) return nullptr;
        return anim.frames[current_frame_index];
    }

    SDL_Texture* get_image() const {
        return get_current_frame();
    }

    void set_z_index(int z) {
        z_index = z;
    }

    void set_position(int x, int y) {
        pos_X = x;
        pos_Y = y;
    }

    std::string get_type() const {
        return info ? info->type : "Unknown";
    }
};

#endif
