#pragma once

#include <vector>
#include <string>
#include <SDL.h>
#include "render_utils.hpp"
#include "asset_info.hpp"
#include "Area.hpp"

class AreaDebugRenderer {
public:
    AreaDebugRenderer(SDL_Renderer* renderer, RenderUtils& util);

    void setTestAreas(const std::vector<std::string>& areas);
    void render(const AssetInfo* info, int world_x, int world_y);

private:
    SDL_Renderer* renderer_;
    RenderUtils& util_;
    std::vector<std::string> test_areas_;
};
