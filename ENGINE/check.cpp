#include "check.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>

Check::Check(bool debug)
    : debug_(debug)
{}

void Check::setDebug(bool debug) {
    debug_ = debug;
}

bool Check::check(const std::shared_ptr<AssetInfo>& info,
                  int test_x,
                  int test_y,
                  const std::vector<Area>& exclusion_areas,
                  const std::vector<std::unique_ptr<Asset>>& assets,
                  bool check_spacing,
                  bool check_min_distance,
                  int num_neighbors) const
{
    if (!info) {
        if (debug_) std::cout << "[Check] AssetInfo is null\n";
        return false;
    }

    if (debug_) {
        std::cout << "[Check] Running checks at position ("
                  << test_x << ", " << test_y
                  << ") for asset: " << info->name << "\n";
    }

    if (is_in_exclusion_zone(test_x, test_y, exclusion_areas)) {
        if (debug_) std::cout << "[Check] Point is inside exclusion zone.\n";
        return true;
    }

    if (info->type == "Background") {
        if (debug_) std::cout << "[Check] Background asset; skipping spacing and type distance checks.\n";
        return false;
    }

    auto nearest = get_closest_assets(test_x, test_y, num_neighbors, assets);
    if (debug_) std::cout << "[Check] Found " << nearest.size() << " nearest assets.\n";

    if (check_spacing && info->has_spacing_area && info->spacing_area) {
        if (check_spacing_overlap(info, test_x, test_y, nearest)) {
            if (debug_) std::cout << "[Check] Spacing overlap detected.\n";
            return true;
        }
    }

    if (check_min_distance && info->min_same_type_distance > 0) {
        if (check_min_type_distance(info, { test_x, test_y }, assets)) {
            if (debug_) std::cout << "[Check] Minimum type distance violated.\n";
            return true;
        }
    }

    if (debug_) std::cout << "[Check] All checks passed.\n";
    return false;
}

bool Check::is_in_exclusion_zone(int x, int y, const std::vector<Area>& zones) const {
    for (const auto& area : zones) {
        if (area.contains_point({x, y})) {
            if (debug_) std::cout << "[Check] Point (" << x << ", " << y << ") is inside an exclusion area.\n";
            return true;
        }
    }
    return false;
}

std::vector<Asset*> Check::get_closest_assets(int x, int y, int max_count,
                                              const std::vector<std::unique_ptr<Asset>>& assets) const
{
    std::vector<std::pair<int, Asset*>> pairs;
    pairs.reserve(assets.size());

    for (const auto& uptr : assets) {
        Asset* a = uptr.get();
        if (!a || !a->info) continue;

        int dx = a->pos_X - x;
        int dy = a->pos_Y - y;
        int dist_sq = dx * dx + dy * dy;
        pairs.emplace_back(dist_sq, a);
    }

    if (pairs.size() > static_cast<size_t>(max_count)) {
        std::nth_element(pairs.begin(),
                         pairs.begin() + max_count,
                         pairs.end(),
                         [](auto& a, auto& b) { return a.first < b.first; });
        pairs.resize(max_count);
    }

    std::sort(pairs.begin(), pairs.end(),
              [](auto& a, auto& b) { return a.first < b.first; });

    std::vector<Asset*> closest;
    closest.reserve(pairs.size());
    for (auto& p : pairs) {
        closest.push_back(p.second);
        if (debug_) {
            std::cout << "[Check] Closest asset: " << p.second->info->name
                      << " at (" << p.second->pos_X << ", " << p.second->pos_Y
                      << "), dist_sq=" << p.first << "\n";
        }
    }
    return closest;
}

bool Check::check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                                  int test_pos_X,
                                  int test_pos_Y,
                                  const std::vector<Asset*>& closest_assets) const
{
    if (!info || !info->spacing_area) return false;

    std::unique_ptr<Area> test_area_ptr = std::make_unique<Area>(*info->spacing_area);
    test_area_ptr->align(test_pos_X, test_pos_Y);

    for (Asset* other : closest_assets) {
        if (!other || !other->info) continue;

        std::unique_ptr<Area> other_area_ptr;
        if (other->info->has_spacing_area && other->info->spacing_area) {
            other_area_ptr = std::make_unique<Area>(other->get_global_spacing_area());
        } else {
            other_area_ptr = std::make_unique<Area>("fallback", other->pos_X, other->pos_Y,
                                                    1, 1, "Square", 0,
                                                    std::numeric_limits<int>::max(),
                                                    std::numeric_limits<int>::max());
        }

        if (test_area_ptr->intersects(*other_area_ptr)) {
            if (debug_) std::cout << "[Check] Overlap found between test area and asset: "
                                  << other->info->name << "\n";
            return true;
        }
    }

    return false;
}

bool Check::check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                    const Point& pos,
                                    const std::vector<std::unique_ptr<Asset>>& assets) const
{
    if (!info || info->name.empty() || info->min_same_type_distance <= 0)
        return false;

    int min_dist_sq = info->min_same_type_distance * info->min_same_type_distance;
    for (const auto& uptr : assets) {
        Asset* existing = uptr.get();
        if (!existing || !existing->info) continue;

        if (existing->info->type != info->type)
            continue;

        int dx = existing->pos_X - pos.first;
        int dy = existing->pos_Y - pos.second;
        if (dx * dx + dy * dy < min_dist_sq) {
            if (debug_) {
                std::cout << "[Check] Minimum spacing violated by asset: "
                          << existing->info->name
                          << " at (" << existing->pos_X << ", "
                          << existing->pos_Y << ")\n";
            }
            return true;
        }
    }
    return false;
}
