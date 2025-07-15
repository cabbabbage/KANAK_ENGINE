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

class AssetLoader {
public:
    // Constructor takes the map directory and an SDL_Renderer for later texture work
    AssetLoader(const std::string& map_dir, SDL_Renderer* renderer);

    // Extracts all spawned Asset instances by value, with animations loaded and finalized
    std::vector<Asset> extract_all_assets();

    // Returns the Areas of all rooms and trails for minimap or collision
    std::vector<Area> getAllRoomAndTrailAreas() const;

    // Builds a minimap texture of given size
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

    // For internal traversal
    std::vector<Room*>                  rooms_;
    std::vector<std::unique_ptr<Room>>  all_rooms_;

    std::mt19937                        rng_;
};
