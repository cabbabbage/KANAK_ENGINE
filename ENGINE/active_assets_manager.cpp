// active_assets_manager.cpp
#include "active_assets_manager.hpp"
#include "Asset.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

ActiveAssetsManager::ActiveAssetsManager(int screen_width, int screen_height)
  : screen_width_(screen_width),
    screen_height_(screen_height),
    all_assets_(nullptr)
{}

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
    if (player) activate(player);
}

void ActiveAssetsManager::updateVisibility(Asset* /*player*/,
                                           int screen_center_x,
                                           int screen_center_y)
{
    updateDynamicChunks();
    sortByDistance(screen_center_x, screen_center_y);
}

void ActiveAssetsManager::updateClosest(Asset* player, std::size_t max_count)
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

    std::size_t count = std::min(max_count, dist_pairs.size());
    if (count > 0) {
        if (count < dist_pairs.size()) {
            std::nth_element(dist_pairs.begin(),
                             dist_pairs.begin() + count,
                             dist_pairs.end(),
                             [](auto& A, auto& B){ return A.first < B.first; });
        }
        std::sort(dist_pairs.begin(),
                  dist_pairs.begin() + count,
                  [](auto& A, auto& B){ return A.first < B.first; });
        for (std::size_t i = 0; i < count; ++i) {
            closest_assets_.push_back(dist_pairs[i].second);
        }
    }
}

void ActiveAssetsManager::activate(Asset* asset)
{
    if (!asset || asset->active) return;
    asset->active = true;

    auto it = std::lower_bound(
        active_assets_.begin(), active_assets_.end(), asset,
        [](Asset* A, Asset* B){ return A->z_index < B->z_index; });

    active_assets_.insert(it, asset);

    // Recurse into children (now vector<Asset*>)
    for (Asset* c : asset->children) {
        if (c && !c->dead && c->info) {
            c->update();
        }
    }
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

    const float half_w = screen_width_ * 1.5f;
    const float radius_sq = half_w * half_w;

    // Reset all active flags and active list
    for (auto& a : *all_assets_) {
        a.active = false;
    }
    active_assets_.clear();

    int chunk_x = cx / screen_width_;
    int chunk_y = cy / screen_height_;

    // Reuse static buffers to avoid per-call allocations
    static std::vector<Asset*> candidates;
    static std::unordered_set<Asset*> seen;
    if (candidates.capacity() == 0) {
        candidates.reserve(512);
        seen.reserve(512);
    }
    candidates.clear();
    seen.clear();

    // Gather unique candidates from neighboring chunks
    for (int dx_off = -1; dx_off <= 1; ++dx_off) {
        for (int dy_off = -1; dy_off <= 1; ++dy_off) {
            ChunkKey key = makeKey(chunk_x + dx_off, chunk_y + dy_off);

            auto it_stat = static_chunks_.find(key);
            if (it_stat != static_chunks_.end()) {
                for (Asset* a : it_stat->second) {
                    if (seen.insert(a).second) {
                        candidates.push_back(a);
                    }
                }
            }

            auto it_dyn = dynamic_chunks_.find(key);
            if (it_dyn != dynamic_chunks_.end()) {
                for (Asset* a : it_dyn->second) {
                    if (seen.insert(a).second) {
                        candidates.push_back(a);
                    }
                }
            }
        }
    }

    // Activate those within range
    for (Asset* a : candidates) {
        float dx = float(a->pos_X - cx);
        float dy = float(a->pos_Y - (cy + 350));
        if (dx*dx + dy*dy <= radius_sq) {
            activate(a);
        }
    }
}

void ActiveAssetsManager::sortByZIndex()
{
    std::sort(active_assets_.begin(), active_assets_.end(),
              [](Asset* A, Asset* B) {
                  if (A->z_index != B->z_index) return A->z_index < B->z_index;
                  if (A->pos_Y != B->pos_Y)     return A->pos_Y < B->pos_Y;
                  if (A->pos_X != B->pos_X)     return A->pos_X < B->pos_X;
                  return A < B;
              });
}

void ActiveAssetsManager::buildStaticChunks()
{
    static_chunks_.clear();
    movable_assets_.clear();
    if (!all_assets_) return;

    for (auto& a : *all_assets_) {
        if (!a.info) continue;

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
