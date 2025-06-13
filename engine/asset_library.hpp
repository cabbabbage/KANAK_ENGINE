#pragma once

#include "asset_info.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <SDL.h>

class AssetLibrary {
public:
    explicit AssetLibrary(SDL_Renderer* renderer);

    void load_all_from_SRC();
    std::shared_ptr<AssetInfo> get(const std::string& name) const;
    const std::unordered_map<std::string, std::shared_ptr<AssetInfo>>& all() const;

private:
    SDL_Renderer* renderer_;
    std::unordered_map<std::string, std::shared_ptr<AssetInfo>> info_by_name_;
};
