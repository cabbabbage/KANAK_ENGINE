#ifndef ENGINE_HPP
#define ENGINE_HPP
#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <vector>
#include <unordered_set>
#include "assets.hpp"
#include "asset.hpp"

class Engine {
public:
    Engine(const std::string& map_json);
    ~Engine();

    void init();
    void game_loop();
    void render_visible();

private:
    std::string map_json;
    Mix_Music* background_music = nullptr;
    SDL_Window* window;
    SDL_Renderer* renderer;
    Assets* game_assets;
    SDL_Texture* overlay_center_texture;

    int SCREEN_WIDTH;
    int SCREEN_HEIGHT;

    // Overlay (JPG)
    SDL_Texture* overlay_texture;
    int overlay_opacity;

    // Animated GIF as sequence of frames
    std::vector<SDL_Texture*> gif_frames;
    int gif_frame_index;
    Uint32 gif_timer;
    Uint32 gif_delay;

    // Persistent earth-tone background color
    SDL_Color background_color;
};

#endif
