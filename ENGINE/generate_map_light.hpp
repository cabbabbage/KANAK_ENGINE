// File: generate_map_light.hpp

#ifndef GENERATE_MAP_LIGHT_HPP
#define GENERATE_MAP_LIGHT_HPP

#include <SDL.h>
#include <utility>

class Generate_Map_Light {
public:
    Generate_Map_Light(SDL_Renderer* renderer,
                       int screen_center_x,
                       int screen_center_y,
                       int screen_width,
                       SDL_Color base_color,
                       Uint8 min_opacity,
                       Uint8 max_opacity);

    void update();
    std::pair<int, int> get_position() const;
    SDL_Texture* get_texture() const;

    SDL_Color current_color_;

private:
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;

    SDL_Color base_color_;
    Uint8 min_opacity_;
    Uint8 max_opacity_;

    int radius_;
    int center_x_;
    int center_y_;
    float angle_;
    bool initialized_;
    int pos_x_;
    int pos_y_;

    int intensity_ = 255;
    int orbit_radius = 1000;
    int frame_counter_ = 0;
    int update_interval_ = 40;

    void build_texture();
    float compute_opacity_from_horizon(float norm) const;
    SDL_Color compute_color_from_horizon(float norm) const;
};

#endif // GENERATE_MAP_LIGHT_HPP
