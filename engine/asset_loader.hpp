// === File: asset_loader.hpp ===
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <random>
#include <SDL.h>
#include "area.hpp"
#include "asset.hpp"
#include "asset_library.hpp"
#include "room.hpp"
#include "generate_rooms.hpp"
#include "assets.hpp"

class AssetLoader {
public:
    AssetLoader(const std::string& map_dir, SDL_Renderer* renderer);

    std::vector<Asset> extract_all_assets();

    // Builds and returns a fully initialized Assets manager
    std::unique_ptr<Assets> createAssets(int screen_width, int screen_height);

    std::vector<Area> getAllRoomAndTrailAreas() const;

    SDL_Texture* createMinimap(int width, int height);

private:
    void load_map_json();

    std::string                         map_path_;
    std::string                         map_boundary_file_;
    int                                 map_radius_    = 0;
    int                                 map_center_x_  = 0;
    int                                 map_center_y_  = 0;
    std::vector<LayerSpec>              map_layers_;

    SDL_Renderer*                       renderer_;
    std::unique_ptr<AssetLibrary>       asset_library_;

    std::vector<Room*>                  rooms_;
    std::vector<std::unique_ptr<Room>>  all_rooms_;

    std::mt19937                        rng_;
};
