/*
 * check.hpp
 */
#pragma once

#include <vector>
#include <memory>
#include <utility>
#include "asset.hpp"
#include "asset_info.hpp"
#include "area.hpp"

class Check {
public:
    using Point = std::pair<int,int>;

    explicit Check(const std::vector<std::unique_ptr<Asset>>& assets);

    std::vector<Asset*> get_closest_assets(int x, int y, int max_count) const;
    bool check_spacing_overlap(const std::shared_ptr<AssetInfo>& info,
                               int test_pos_X,
                               int test_pos_Y,
                               const std::vector<Asset*>& closest_assets) const;
    bool check_min_type_distance(const std::shared_ptr<AssetInfo>& info,
                                  const Point& pos) const;

private:
    const std::vector<std::unique_ptr<Asset>>& assets_;
};

