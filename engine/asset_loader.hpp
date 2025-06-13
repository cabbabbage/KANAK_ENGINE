#ifndef ASSET_LOADER_HPP
#define ASSET_LOADER_HPP

#include "Asset.hpp"
#include "Area.hpp"
#include "generate_room.hpp"
#include "generate_trails.hpp"
#include <string>
#include <vector>
#include <random>
#include <memory>
#include <SDL.h>
#include <nlohmann/json.hpp>
#include "asset_generator.hpp"

class AssetLoader {
public:
    AssetLoader(const std::string& map_dir, SDL_Renderer* renderer);
    std::unique_ptr<AssetLibrary> asset_library_;

    const std::vector<Asset>& get_all_assets() const;
    std::vector<Asset> extract_all_assets();
    int get_screen_center_x() const;
    int get_screen_center_y() const;
    Asset* get_player();
    std::vector<Area> getAllRoomAndTrailAreas() const;
private:
    std::string map_path_;
    int map_width_;
    int map_height_;
    SDL_Renderer* renderer_;
    std::mt19937 rng_;

    std::vector<GenerateRoom> rooms_;
    std::unique_ptr<GenerateTrails> trail_gen_;
    std::vector<Asset> all_assets_;
};

#endif // ASSET_LOADER_HPP
