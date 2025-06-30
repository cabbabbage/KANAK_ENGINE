#include "check.hpp"
#include <algorithm>
#include <limits>

Check::Check(const std::vector<std::unique_ptr<Asset>>& assets)
    : assets_(assets) {}

std::vector<Asset*> Check::get_closest_assets(int x, int y, int max_count) const {
    std::vector<std::pair<int, Asset*>> pairs;
    pairs.reserve(assets_.size());

    for (const auto& uptr : assets_) {
        Asset* a = uptr.get();
        if (!a || !a->info) continue;
        if (a->pos_Y >= y) continue;  // only consider assets higher on the screen

        int dx = a->pos_X - x;
        int dy = a->pos_Y - y;
        int dist_sq = dx * dx + dy * dy;
        pairs.emplace_back(dist_sq, a);
    }

    if (pairs.size() > static_cast<size_t>(max_count)) {
        std::nth_element(pairs.begin(), pairs.begin() + max_count, pairs.end(),
                         [](auto& a, auto& b) { return a.first < b.first; });
        pairs.resize(max_count);
    }

    std::sort(pairs.begin(), pairs.end(), [](auto& a, auto& b) { return a.first < b.first; });

    std::vector<Asset*> closest;
    closest.reserve(pairs.size());
    for (auto& p : pairs) closest.push_back(p.second);
    return closest;
}


bool Check::check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                                  int test_pos_X,
                                  int test_pos_Y,
                                  const std::vector<Asset*>& closest_assets) const {
    if (!info) return false;

    const std::string& type = info->type;

    Area test_area;
    if (info->has_spacing_area) {
        test_area = info->spacing_area;
    } else {
        test_area.generate_square(test_pos_X, test_pos_Y, 1, 1, 0,
                                  std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }
    test_area.align(test_pos_X, test_pos_Y);

    for (Asset* other : closest_assets) {
        if (!other || !other->info) continue;
        if (type == "Background" && other->info->type == "Background") continue;
        if (type == "Background" && other->info->type == "MAP") continue;

        Area other_area;
        if (other->info->has_spacing_area) {
            other_area = other->get_global_spacing_area();
        } else {
            other_area.generate_square(other->pos_X, other->pos_Y, 1, 1, 0,
                                       std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
        }

        if (test_area.intersects(other_area)) return true;
    }
    return false;
}

bool Check::check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                    const Point& pos) const {
    if (!info || info->name.empty() || info->min_same_type_distance <= 0) return false;
    int min_dist_sq = info->min_same_type_distance * info->min_same_type_distance;
    for (const auto& uptr : assets_) {
        Asset* existing = uptr.get();
        if (!existing || !existing->info) continue;
        if (existing->info->name != info->name) continue;
        int dx = existing->pos_X - pos.first;
        int dy = existing->pos_Y - pos.second;
        if (dx * dx + dy * dy < min_dist_sq) return true;
    }
    return false;
}
