// === File: ControlsManager.hpp ===
#pragma once

#ifndef CONTROLS_MANAGER_HPP
#define CONTROLS_MANAGER_HPP

#include <vector>
#include <unordered_set>
#include <SDL.h>
#include "asset.hpp"
#include "area.hpp"

class ControlsManager {
public:
    ControlsManager(Asset* player, std::vector<Asset*>& closest);

    // Process both movement (WASD) and interaction (E)
    void update(const std::unordered_set<SDL_Keycode>& keys);

    // Retrieve the computed movement deltas
    int get_dx() const;
    int get_dy() const;

private:
    // WASD movement & animation
    void movement(const std::unordered_set<SDL_Keycode>& keys);

    // "E" key interaction
    void interaction();

    // Collision test for movement
    bool canMove(int offset_x, int offset_y);

    // Axis-Aligned Bounding Box overlap
    bool aabb(const Area& A, const Area& B) const;

    // Point-in-AABB test
    bool pointInAABB(int x, int y, const Area& B) const;

    Asset*                player_;
    std::vector<Asset*>&  closest_;
    int                   dx_;
    int                   dy_;
};

#endif // CONTROLS_MANAGER_HPP
