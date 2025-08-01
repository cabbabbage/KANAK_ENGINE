// active_assets_manager.hpp
#ifndef ACTIVE_ASSETS_MANAGER_HPP
#define ACTIVE_ASSETS_MANAGER_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include "asset.hpp"

class ActiveAssetsManager {
public:
    ActiveAssetsManager(int screen_width, int screen_height);

    void initialize(std::vector<Asset>& all_assets,
                    Asset* player,
                    int screen_center_x,
                    int screen_center_y);

    void updateVisibility(Asset* player,
                          int screen_center_x,
                          int screen_center_y);

    void updateClosest(Asset* player, size_t max_count = 10);

    std::vector<Asset*>& getActive() { return active_assets_; }
    const std::vector<Asset*>& getActive() const { return active_assets_; }

    std::vector<Asset*>& getClosest() { return closest_assets_; }
    const std::vector<Asset*>& getClosest() const { return closest_assets_; }

private:
    using ChunkKey = std::int64_t;

    int screen_width_;
    int screen_height_;
    std::vector<Asset>* all_assets_;

    std::unordered_map<ChunkKey, std::vector<Asset*>> static_chunks_;
    std::unordered_map<ChunkKey, std::vector<Asset*>> dynamic_chunks_;
    std::vector<Asset*> movable_assets_;

    std::vector<Asset*> active_assets_;
    std::vector<Asset*> closest_assets_;

    void activate(Asset* asset);
    void remove(Asset* asset);

    void sortByDistance(int cx, int cy);
    void buildStaticChunks();
    void updateDynamicChunks();

    inline ChunkKey makeKey(int cx, int cy) const {
        return (static_cast<ChunkKey>(cx) << 32) | static_cast<uint32_t>(cy);
    }
};

#endif // ACTIVE_ASSETS_MANAGER_HPP
