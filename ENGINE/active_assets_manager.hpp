// === File: active_assets_manager.hpp ===
#pragma once

#include <vector>
#include <algorithm>
#include "Asset.hpp"

// Manages which assets are in-range (active) and tracks the closest assets to the player.
class ActiveAssetsManager {
public:
    ActiveAssetsManager(int screen_width, int screen_height);

    // Initial scan: activate assets in range and ensure the player is active
    void initialize(std::vector<Asset>& all_assets, Asset* player, int screen_center_x, int screen_center_y);

    // After player moves or the camera center changes, re-compute which assets are active
    void updateVisibility(Asset* player, int screen_center_x, int screen_center_y);

    // Compute the N closest active assets to the player
    void updateClosest(Asset* player, size_t max_count = 30);

    // Accessors
    std::vector<Asset*>& getActive()  { return active_assets_; }
    std::vector<Asset*>& getClosest() { return closest_assets_; }

private:
    void activate(Asset* asset);
    void remove(Asset* asset);
    void sortByDistance(int cx, int cy);

    std::vector<Asset>*       all_assets_;       // pointer to master list
    std::vector<Asset*>       active_assets_;    // currently active (in-range)
    std::vector<Asset*>       closest_assets_;   // nearest to player
    int                       screen_width_;
    int                       screen_height_;
};
