#include "rebuild_assets.hpp"
#include "asset_library.hpp"
#include "generate_rooms.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

RebuildAssets::RebuildAssets(SDL_Renderer* renderer, const std::string& map_dir) {
    try {
        std::cout << "[RebuildAssets] Removing old cache directory...\n";
        fs::remove_all("cache");
        std::cout << "[RebuildAssets] Cache directory deleted.\n";

        std::cout << "[RebuildAssets] Creating new AssetLibrary...\n";
        AssetLibrary asset_lib(renderer);
        std::cout << "[RebuildAssets] AssetLibrary rebuilt successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "[RebuildAssets] Error: " << e.what() << "\n";
    }
}
