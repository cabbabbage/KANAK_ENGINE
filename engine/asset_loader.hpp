// asset_loader.hpp
#pragma once

#include "asset.hpp"
#include "asset_generator.hpp"
#include "generate_room.hpp"
#include "generate_trails.hpp"
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <string>
#include <vector>
#include <random>

class AssetLoader {
public:
    AssetLoader(const std::string& map_path, SDL_Renderer* renderer);

    const std::vector<std::unique_ptr<Asset>>& get_all_assets() const;
    Asset* get_player() const;

    int get_screen_center_x() const;
    int get_screen_center_y() const;

private:
    std::string map_path_;
    SDL_Renderer* renderer_;
    nlohmann::json root_;

    int map_width_ = 0;
    int map_height_ = 0;

    int min_rooms_ = 0;
    int max_rooms_ = 0;
    int min_room_h_ = 0;
    int max_room_h_ = 0;
    int min_room_w_ = 0;
    int max_room_w_ = 0;
    int min_trail_w_ = 0;
    int max_trail_w_ = 0;

    std::vector<GenerateRoom> rooms_;
    std::vector<GenerateTrails::Trail> trails_;
    std::vector<std::unique_ptr<Asset>> all_assets_;
    Asset* player_ = nullptr;

    std::mt19937 rng_;

    void load_json();
    void generate_rooms();
    void generate_trails();
    void spawn_assets_for_rooms();
    void spawn_assets_for_trails();
    void spawn_assets_for_open_space();
    void spawn_davey_player();
    void place_player_in_random_room();
};
