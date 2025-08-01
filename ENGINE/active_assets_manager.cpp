// active_assets_manager.cpp
#include "active_assets_manager.hpp"
#include <algorithm>
#include <cmath>

ActiveAssetsManager::ActiveAssetsManager(int screen_width, int screen_height)
  : screen_width_(screen_width),
    screen_height_(screen_height),
    all_assets_(nullptr)
{
}

void ActiveAssetsManager::initialize(std::vector<Asset>& all_assets,
                                     Asset* player,
                                     int screen_center_x,
                                     int screen_center_y)
{
    all_assets_ = &all_assets;
    buildStaticChunks();
    updateDynamicChunks();
    active_assets_.clear();
    closest_assets_.clear();

    sortByDistance(screen_center_x, screen_center_y);
    if (player)
        activate(player);
}

void ActiveAssetsManager::updateVisibility(Asset* player,
                                           int screen_center_x,
                                           int screen_center_y)
{
    updateDynamicChunks();
    sortByDistance(screen_center_x, screen_center_y);
}

void ActiveAssetsManager::updateClosest(Asset* player, size_t max_count)
{
    closest_assets_.clear();
    if (!player) return;

    std::vector<std::pair<float, Asset*>> dist_pairs;
    dist_pairs.reserve(active_assets_.size());

    for (auto* a : active_assets_) {
        if (a == player) continue;
        float dx = float(a->pos_X - player->pos_X);
        float dy = float(a->pos_Y - player->pos_Y);
        dist_pairs.emplace_back(dx*dx + dy*dy, a);
    }
    std::sort(dist_pairs.begin(), dist_pairs.end(),
              [](auto& A, auto& B){ return A.first < B.first; });

    size_t count = std::min(max_count, dist_pairs.size());
    for (size_t i = 0; i < count; ++i)
        closest_assets_.push_back(dist_pairs[i].second);
}

void ActiveAssetsManager::activate(Asset* asset)
{
    if (!asset || asset->active) return;
    asset->active = true;

    auto it = std::lower_bound(
      active_assets_.begin(), active_assets_.end(), asset,
      [](Asset* A, Asset* B){ return A->z_index < B->z_index; });
    active_assets_.insert(it, asset);

    for (auto& c : asset->children)
        activate(&c);
}

void ActiveAssetsManager::remove(Asset* asset)
{
    if (!asset || !asset->active) return;
    asset->active = false;
    auto it = std::remove(active_assets_.begin(), active_assets_.end(), asset);
    active_assets_.erase(it, active_assets_.end());
}

void ActiveAssetsManager::sortByDistance(int cx, int cy)
{
    if (!all_assets_) return;

    const float radius = float(screen_width_);
    const float radius_sq = radius * radius;

    for (auto& a : *all_assets_)
        a.active = false;

    active_assets_.clear();

    int chunk_x = cx / screen_width_;
    int chunk_y = cy / screen_height_;

    std::vector<Asset*> candidates;
    std::unordered_set<Asset*> seen;
    candidates.reserve(512);

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            ChunkKey key = makeKey(chunk_x + dx, chunk_y + dy);

            auto process_chunk = [&](const std::unordered_map<ChunkKey, std::vector<Asset*>>& chunk_map) {
                auto it = chunk_map.find(key);
                if (it != chunk_map.end()) {
                    for (Asset* a : it->second) {
                        if (seen.insert(a).second) {
                            candidates.push_back(a);
                        }
                    }
                }
            };

            process_chunk(static_chunks_);
            process_chunk(dynamic_chunks_);
        }
    }

    for (Asset* a : candidates) {
        float dx = float(a->pos_X - cx);
        float dy = float(a->pos_Y - cy - 350);
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq <= radius_sq) {
            activate(a);
        }
    }
}

void ActiveAssetsManager::buildStaticChunks()
{
    static_chunks_.clear();
    movable_assets_.clear();
    if (!all_assets_) return;

    for (auto& a : *all_assets_) {
        const std::string& t = a.info->type;
        if (t == "Player" || t == "NPC" || t == "Animal" || t == "Enemy") {
            movable_assets_.push_back(&a);
        } else {
            int cx = a.pos_X / screen_width_;
            int cy = a.pos_Y / screen_height_;
            ChunkKey key = makeKey(cx, cy);
            static_chunks_[key].push_back(&a);
        }
    }
}

void ActiveAssetsManager::updateDynamicChunks()
{
    dynamic_chunks_.clear();
    for (Asset* a : movable_assets_) {
        int cx = a->pos_X / screen_width_;
        int cy = a->pos_Y / screen_height_;
        ChunkKey key = makeKey(cx, cy);
        dynamic_chunks_[key].push_back(a);
    }
}
