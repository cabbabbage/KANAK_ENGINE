#pragma once
#include <string>
#include <memory>
#include "area.hpp"
#include "asset_info.hpp"

struct SpawnInfo {
    std::string name;
    std::string type;
    std::string spawn_position;  // "Center", "exact", or "random"
    int quantity;
    int x_position;  // -1 if not specified
    int y_position;  // -1 if not specified
    int spacing_min;
    int spacing_max;
    std::shared_ptr<AssetInfo> info;
};
