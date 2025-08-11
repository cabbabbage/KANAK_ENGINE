#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "Asset.hpp"
#include "view.hpp"

class ActiveAssetsManager {
public:
    using ChunkKey = std::uint64_t;

    ActiveAssetsManager(int screen_width, int screen_height, view& v);

    void initialize(std::vector<Asset>& all_assets,
                    Asset* player,
                    int screen_center_x,
                    int screen_center_y);

    void updateVisibility(Asset* player,
                          int screen_center_x,
                          int screen_center_y);

    void updateClosest(Asset* player, std::size_t max_count);

    void sortByZIndex();

    std::vector<Asset*>& getActive()   { return active_assets_; }
    std::vector<Asset*>& getClosest()  { return closest_assets_; }

private:
    view& view_;
    int screen_width_;
    int screen_height_;

    std::vector<Asset>* all_assets_;
    std::vector<Asset*> movable_assets_;
    std::vector<Asset*> active_assets_;
    std::vector<Asset*> closest_assets_;

    std::unordered_map<ChunkKey, std::vector<Asset*>> static_chunks_;
    std::unordered_map<ChunkKey, std::vector<Asset*>> dynamic_chunks_;

    static constexpr ChunkKey makeKey(int cx, int cy) {
        return (static_cast<ChunkKey>(static_cast<uint32_t>(cx)) << 32) |
                static_cast<uint32_t>(cy);
    }

    void buildStaticChunks();
    void updateDynamicChunks();
    void sortByDistance(int cx, int cy);
    void activate(Asset* asset);
    void remove(Asset* asset);
};
