// === File: ControlsManager.hpp ===

#pragma once

#include <unordered_set>
#include <vector>
#include <SDL2/SDL.h>
#include "asset.hpp"
#include "area.hpp"

class ControlsManager {
public:
    ControlsManager(Asset* player, std::vector<Asset*>* closest);

    void update(const std::unordered_set<SDL_Keycode>& keys);
    void movement(const std::unordered_set<SDL_Keycode>& keys);
    void interaction();
    void handle_teleport(const std::unordered_set<SDL_Keycode>& keys);
    bool canMove(int offset_x, int offset_y);

    int get_dx() const;
    int get_dy() const;

private:
    bool aabb(const Area& A, const Area& B) const;
    bool pointInAABB(int x, int y, const Area& B) const;

    Asset* player_;
    std::vector<Asset*>* closest_;

    int dx_;
    int dy_;

    // ─── Teleport state ─────────────────────────────────────
    SDL_Point teleport_point_;
    bool teleport_set_ = false;
};
