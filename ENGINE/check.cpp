// === File: check.cpp ===
#include "check.hpp"
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>

// Constructor: optionally enable debug output
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
                  const std::vector<std::unique_ptr<Asset>>& assets) const
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

    // exclusion zones always apply
    if (is_in_exclusion_zone(test_x, test_y, exclusion_areas)) {
        if (debug_) std::cout << "[Check] Point is inside exclusion zone.\n";
        return true;  // placement invalid
    }

    // background assets skip spacing & min‐distance checks
    if (info->type == "Background") {
        if (debug_) std::cout << "[Check] Background asset; skipping spacing and type distance checks.\n";
        return false;  // placement valid
    }

    // gather nearest neighbors for spacing checks
    auto nearest = get_closest_assets(test_x, test_y, 3, assets);
    if (debug_) std::cout << "[Check] Found " << nearest.size() << " nearest assets.\n";

    // spacing area overlap
    if (info->has_spacing_area) {
        if (check_spacing_overlap(info, test_x, test_y, nearest)) {
            if (debug_) std::cout << "[Check] Spacing overlap detected.\n";
            return true;
        }
    }

    // minimum same‐type distance
    if (info->min_same_type_distance > 0) {
        if (check_min_type_distance(info, { test_x, test_y }, assets)) {
            if (debug_) std::cout << "[Check] Minimum type distance violated.\n";
            return true;
        }
    }

    if (debug_) std::cout << "[Check] All checks passed.\n";
    return false;  // placement valid
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
        if (a->pos_Y >= y) continue;

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
    if (!info) return false;

    const std::string& type = info->type;

    Area test_area = info->has_spacing_area
        ? info->spacing_area
        : Area(test_pos_X, test_pos_Y,
               1, 1,
               "Square", 0,
               std::numeric_limits<int>::max(),
               std::numeric_limits<int>::max());

    test_area.align(test_pos_X, test_pos_Y);

    for (Asset* other : closest_assets) {
        if (!other || !other->info) continue;
        if (type == "Background" &&
            (other->info->type == "Background" || other->info->type == "MAP"))
            continue;

        Area other_area = other->info->has_spacing_area
            ? other->get_global_spacing_area()
            : Area(other->pos_X, other->pos_Y,
                   1, 1,
                   "Square", 0,
                   std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());

        if (test_area.intersects(other_area)) {
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
        if (existing->info->name != info->name) continue;

        int dx = existing->pos_X - pos.first;
        int dy = existing->pos_Y - pos.second;
        if (dx * dx + dy * dy < min_dist_sq) {
            if (debug_) {
                std::cout << "[Check] Type distance violated by asset: "
                          << existing->info->name
                          << " at (" << existing->pos_X << ", "
                          << existing->pos_Y << ")\n";
            }
            return true;
        }
    }
    return false;
}
