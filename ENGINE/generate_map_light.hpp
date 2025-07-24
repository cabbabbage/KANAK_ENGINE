// generate_map_light.hpp

#pragma once

#include <SDL.h>
#include <vector>
#include <string>

class CacheManager;

class Generate_Map_Light {
public:
    Generate_Map_Light(SDL_Renderer* renderer,
                       int screen_center_x,
                       int screen_center_y,
                       int screen_width,
                       SDL_Color fallback_base_color,
                       const std::string& map_path);

    void update();
    std::pair<int, int> get_position() const;
    SDL_Texture* get_texture() const;
    SDL_Color current_color_;
private:
    void build_texture();
    float compute_opacity_from_horizon(float norm) const;
    SDL_Color compute_color_from_horizon() const;

    struct KeyColor {
        float degree;
        SDL_Color color;
    };

    SDL_Renderer* renderer_;
    SDL_Texture* texture_;

    SDL_Color base_color_;


    int radius_ = 0;
    int intensity_ = 255;
    int orbit_radius = 150;
    int update_interval_ = 2;
    Uint8 min_opacity_ = 50;
    Uint8 max_opacity_ = 255;
    double mult_ = 0.4;

    int pos_x_ = 0;
    int pos_y_ = 0;
    int center_x_ = 0;
    int center_y_ = 0;

    float angle_ = 0.0f;
    bool initialized_ = false;
    int frame_counter_ = 0;

    std::vector<KeyColor> key_colors_;
};
