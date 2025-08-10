// active_assets_manager.hpp
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>

class Asset;

class ActiveAssetsManager {
public:
    using ChunkKey = std::uint64_t;

public:
    ActiveAssetsManager(int screen_width, int screen_height);

    void initialize(std::vector<Asset>& all_assets,
                    Asset* player,
                    int screen_center_x,
                    int screen_center_y);

    void updateVisibility(Asset* player,
                          int screen_center_x,
                          int screen_center_y);

    void updateClosest(Asset* player, std::size_t max_count);

    void activate(Asset* asset);
    void remove(Asset* asset);

    void sortByDistance(int cx, int cy);
    void sortByZIndex();

    const std::vector<Asset*>& active_assets() const { return active_assets_; }
    const std::vector<Asset*>& closest_assets() const { return closest_assets_; }

    std::vector<Asset*>& getActive() { return active_assets_; }
    const std::vector<Asset*>& getActive() const { return active_assets_; }

    std::vector<Asset*>& getClosest() { return closest_assets_; }
    const std::vector<Asset*>& getClosest() const { return closest_assets_; }
    
private:
    static inline ChunkKey makeKey(int cx, int cy) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
               static_cast<std::uint32_t>(cy);
    }

    void buildStaticChunks();
    void updateDynamicChunks();

private:
    int screen_width_;
    int screen_height_;

    std::vector<Asset>* all_assets_;

    std::vector<Asset*> active_assets_;
    std::vector<Asset*> closest_assets_;
    std::vector<Asset*> movable_assets_;

    std::unordered_map<ChunkKey, std::vector<Asset*>> static_chunks_;
    std::unordered_map<ChunkKey, std::vector<Asset*>> dynamic_chunks_;
};
