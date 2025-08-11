#include "active_assets_manager.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

ActiveAssetsManager::ActiveAssetsManager(int screen_width, int screen_height, view& v)
    : view_(v),
      screen_width_(screen_width),
      screen_height_(screen_height),
      all_assets_(nullptr)
{}

void ActiveAssetsManager::initialize(std::vector<Asset>& all_assets,
                                     Asset* player,
                                     int screen_center_x,
                                     int screen_center_y)
{
    all_assets_ = &all_assets;
    active_assets_.clear();
    closest_assets_.clear();

    sortByDistance(screen_center_x, screen_center_y);
    if (player) activate(player);
}

void ActiveAssetsManager::updateVisibility(Asset*,
                                           int screen_center_x,
                                           int screen_center_y)
{
    sortByDistance(screen_center_x, screen_center_y);
}

void ActiveAssetsManager::updateClosest(Asset* player, std::size_t max_count)
{
    closest_assets_.clear();
    if (!player) return;

    static std::vector<std::pair<float, Asset*>> dist_pairs;
    dist_pairs.clear();
    dist_pairs.reserve(active_assets_.size());

    for (auto* a : active_assets_) {
        if (a == player) continue;
        float dx = float(a->pos_X - player->pos_X);
        float dy = float(a->pos_Y - player->pos_Y);
        dist_pairs.emplace_back(dx * dx + dy * dy, a);
    }

    std::size_t count = std::min(max_count, dist_pairs.size());
    if (count > 0) {
        if (count < dist_pairs.size()) {
            std::nth_element(dist_pairs.begin(),
                             dist_pairs.begin() + count,
                             dist_pairs.end(),
                             [](auto& A, auto& B) { return A.first < B.first; });
            dist_pairs.resize(count);
        }
        std::sort(dist_pairs.begin(),
                  dist_pairs.end(),
                  [](auto& A, auto& B) { return A.first < B.first; });
        for (auto& p : dist_pairs) {
            closest_assets_.push_back(p.second);
        }
    }
}

void ActiveAssetsManager::activate(Asset* asset)
{
    if (!asset || asset->active) return;
    asset->active = true;

    auto it = std::lower_bound(
        active_assets_.begin(), active_assets_.end(), asset,
        [](Asset* A, Asset* B) { return A->z_index < B->z_index; });

    active_assets_.insert(it, asset);

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

    // Keep the last frame's active list
    static std::unordered_set<Asset*> prev_active;
    prev_active.clear();
    prev_active.insert(active_assets_.begin(), active_assets_.end());

    // Mark them inactive so activate() can re-add if still visible
    for (Asset* a : prev_active) {
        a->active = false;
    }

    // Start fresh
    active_assets_.clear();

    // Iterate over all assets directly (no chunking)
    for (Asset& a : *all_assets_) {
        if (view_.is_asset_in_bounds(a, cx, cy)) {
            activate(&a);
        }
    }

    // Deactivate ones that were active before but not now
    for (Asset* old_a : prev_active) {
        if (!old_a->active) {
            old_a->deactivate();
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
