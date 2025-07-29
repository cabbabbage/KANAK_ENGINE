#include "active_assets_manager.hpp"
#include <cmath>

ActiveAssetsManager::ActiveAssetsManager(int screen_width, int screen_height)
  : all_assets_(nullptr),
    screen_width_(screen_width),
    screen_height_(screen_height)
{}

void ActiveAssetsManager::initialize(std::vector<Asset>& all_assets,
                                     Asset* player,
                                     int screen_center_x,
                                     int screen_center_y)
{
    all_assets_ = &all_assets;
    active_assets_.clear();
    closest_assets_.clear();
    // Activate any in-range assets in z-sorted order
    sortByDistance(screen_center_x, screen_center_y);
    // Ensure player is active
    if (player && !player->active) activate(player);
}

void ActiveAssetsManager::updateVisibility(Asset* player,
                                           int screen_center_x,
                                           int screen_center_y)
{
    // Re-evaluate which assets are in range without clearing
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
    // Insert into active_assets_ sorted by z_index
    auto it = std::lower_bound(
        active_assets_.begin(), active_assets_.end(), asset,
        [](Asset* a, Asset* b) {
            return a->z_index < b->z_index;
        }
    );
    active_assets_.insert(it, asset);
    // Recursively activate children
    for (auto& c : asset->children) {
        activate(&c);
    }
}

void ActiveAssetsManager::remove(Asset* asset)
{
    if (!asset || !asset->active) return;
    asset->active = false;
    auto it = std::remove(active_assets_.begin(),
                          active_assets_.end(), asset);
    active_assets_.erase(it, active_assets_.end());
}

void ActiveAssetsManager::sortByDistance(int cx, int cy)
{
    if (!all_assets_) return;

    const float aw = 2.0f * float(screen_width_);
    const float ah = 2.0f * float(screen_height_);

    for (auto& a : *all_assets_) {
        float dx = float(a.pos_X - cx);
        float dy = float(a.pos_Y - cy);
        bool in_range = std::abs(dx) < aw && std::abs(dy) < ah;

        if (in_range && !a.active) {
            activate(&a);
        } else if (!in_range && a.active) {
            remove(&a);
        }
    }
}
