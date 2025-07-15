// === File: check.hpp ===
#pragma once

#include <memory>
#include <vector>
#include <utility>
#include "Asset.hpp"
#include "Area.hpp"
#include "asset_info.hpp"

class Check {
public:
    using Point = std::pair<int, int>;

    // If debug==true, prints debugging output
    Check(bool debug = false);

    // Enable or disable debugging output
    void setDebug(bool debug);

    bool check(const std::shared_ptr<AssetInfo>& info,
               int test_x,
               int test_y,
               const std::vector<Area>& exclusion_areas,
               const std::vector<std::unique_ptr<Asset>>& assets) const;

private:
    bool debug_ = false;

    bool is_in_exclusion_zone(int x,
                              int y,
                              const std::vector<Area>& zones) const;

    std::vector<Asset*> get_closest_assets(int x,
                                           int y,
                                           int max_count,
                                           const std::vector<std::unique_ptr<Asset>>& assets) const;

    bool check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                               int test_pos_X,
                               int test_pos_Y,
                               const std::vector<Asset*>& closest_assets) const;

    bool check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                 const Point& pos,
                                 const std::vector<std::unique_ptr<Asset>>& assets) const;
};
