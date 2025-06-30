// asset_loader.hpp
#ifndef ASSET_LOADER_HPP
#define ASSET_LOADER_HPP

#include "Asset.hpp"
#include "asset_library.hpp"
#include "Area.hpp"
#include "generate_room.hpp"
#include "generate_trails.hpp"
#include "asset_spawner.hpp"

#include <string>
#include <vector>
#include <memory>
#include <random>
#include <SDL.h>
#include <nlohmann/json.hpp>

class AssetLoader {
public:
    AssetLoader(const std::string& map_dir, SDL_Renderer* renderer);

    // Extracts all assets spawned via the single spawner, by value
    std::vector<Asset> extract_all_assets();

    int get_screen_center_x() const;
    int get_screen_center_y() const;

    std::vector<Area> getAllRoomAndTrailAreas() const;

private:
    std::string                         map_path_;
    int                                 map_width_;
    int                                 map_height_;
    SDL_Renderer*                       renderer_;
    std::mt19937                        rng_;

    std::string                         map_boundary_file_;
    nlohmann::json                      room_templates_;
    int                                 target_room_count_;

    std::unique_ptr<AssetLibrary>       asset_library_;
    std::unique_ptr<AssetSpawner>       spawner_;
    std::vector<GenerateRoom>           rooms_;
    std::unique_ptr<GenerateTrails>     trail_gen_;
};

#endif // ASSET_LOADER_HPP
