// generate_map_light.hpp
#pragma once

#include <SDL.h>
#include <string>
#include <vector>
#include <utility>

struct KeyColor {
    float degree;
    SDL_Color color;
};

class Generate_Map_Light {
public:
    // center_x/center_y are the screen-center where the light orbits around
    Generate_Map_Light(SDL_Renderer* renderer,
                       int screen_center_x,
                       int screen_center_y,
                       int screen_width,
                       SDL_Color fallback_base_color,
                       const std::string& map_path);

    // advance the sun/moon by one step
    void update();

    // angle in radians around the orbit
    float get_angle() const;

    // current world‐coords of the light
    std::pair<int,int> get_position() const;

    // the generated radial light texture
    SDL_Texture* get_texture() const;

    // tint to apply to your scene background
    SDL_Color get_tint() const;
    SDL_Color current_color_;    
    // 0…255 brightness for asset‐lighting masks
    int get_brightness() const { return light_brightness; }
    int light_brightness;  
    SDL_Color apply_tint_to_color(const SDL_Color& base, int alpha_mod = 255) const;
private:
    void set_light_brightness();
    void build_texture();
    SDL_Color compute_color_from_horizon() const;

    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    SDL_Color base_color_;         // loaded from JSON
  // from key_colors_ + horizon
    SDL_Color tint_;               // (current_color_ * mult_)

    int center_x_, center_y_;
    float radius_;                 // how far the radial gradient extends
    float intensity_;              // max alpha of the radial gradient
    float mult_;                   // tint strength
    float fall_off_;               // radial light falloff
    int orbit_radius;              // distance of the “sun” from center
    int update_interval_;          // frames between moves

    std::vector<KeyColor> key_colors_;

    float angle_;                  // 0…2π
    bool initialized_;
    int pos_x_, pos_y_;            // computed from angle_
    int frame_counter_;
        // for masking (0…255)
};
