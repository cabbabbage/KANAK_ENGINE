#include "controls_manager.hpp"
#include <cmath>
#include <iostream>

// -----------------------------------------------------------------------------
// Axis-Aligned Bounding-Box (AABB) helper
// -----------------------------------------------------------------------------
bool ControlsManager::aabb(const Area& A, const Area& B) const {
    auto [a_minx, a_miny, a_maxx, a_maxy] = A.get_bounds();
    auto [b_minx, b_miny, b_maxx, b_maxy] = B.get_bounds();
    return !(a_maxx < b_minx || b_maxx < a_minx ||
             a_maxy < b_miny || b_maxy < a_miny);
}

// -----------------------------------------------------------------------------
// Point-in-AABB helper
// -----------------------------------------------------------------------------
bool ControlsManager::pointInAABB(int x, int y, const Area& B) const {
    auto [b_minx, b_miny, b_maxx, b_maxy] = B.get_bounds();
    return (x >= b_minx && x <= b_maxx &&
            y >= b_miny && y <= b_maxy);
}

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
ControlsManager::ControlsManager(Asset* player, std::vector<Asset*>* closest)
    : player_(player),
      closest_(closest),
      dx_(0),
      dy_(0),
      teleport_set_(false)
{}


// -----------------------------------------------------------------------------
// Handle WASD movement & animation
// -----------------------------------------------------------------------------
void ControlsManager::movement(const std::unordered_set<SDL_Keycode>& keys) {
    dx_ = dy_ = 0;
    if (!player_) return;

    bool up    = keys.count(SDLK_w);
    bool down  = keys.count(SDLK_s);
    bool left  = keys.count(SDLK_a);
    bool right = keys.count(SDLK_d);

    int move_x = (right ? 1 : 0) - (left ? 1 : 0);
    int move_y = (down  ? 1 : 0) - (up    ? 1 : 0);

    bool any_movement = (move_x != 0 || move_y != 0);
    bool diagonal     = (move_x != 0 && move_y != 0);
    const std::string current = player_->get_current_animation();

    if (any_movement) {
        float len = std::sqrt(float(move_x * move_x + move_y * move_y));
        if (len == 0.0f) return;

        float base_speed = player_->player_speed;
        if (keys.count(SDLK_LSHIFT) || keys.count(SDLK_RSHIFT)) {
            base_speed *= 2.0f;  // Sprinting
        }

        float speed = base_speed / len;
        int ox = static_cast<int>(std::round(move_x * speed));
        int oy = static_cast<int>(std::round(move_y * speed));

        if (canMove(ox, oy)) {
            dx_ = ox;
            dy_ = oy;
            player_->set_position(player_->pos_X + dx_,
                                  player_->pos_Y + dy_);
        }

        if (!diagonal) {
            std::string anim;
            if      (move_y < 0) anim = "backward";
            else if (move_y > 0) anim = "forward";
            else if (move_x < 0) anim = "left";
            else if (move_x > 0) anim = "right";

            if (!anim.empty() && anim != current)
                player_->change_animation(anim);
        }
    }
    else {
        if (current != "default")
            player_->change_animation("default");
    }
}

// -----------------------------------------------------------------------------
// Check whether the player can move by (offset_x, offset_y)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Check whether the player can move by (offset_x, offset_y)
// -----------------------------------------------------------------------------
bool ControlsManager::canMove(int offset_x, int offset_y) {
    if (!player_ || !closest_) return false;

    int test_x = player_->pos_X + offset_x;
    int test_y = player_->pos_Y + offset_y - player_->info->z_threshold;

    for (Asset* a : *closest_) {
        if (!a || a == player_ || !a->info || a->info->passable || !a->info->has_passability_area)
            continue;

        Area obstacle = a->get_area("passability");
        if (obstacle.contains_point({ test_x, test_y })) {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Handle 'E' interaction key
// -----------------------------------------------------------------------------
void ControlsManager::interaction() {
    if (!player_ || !player_->info || !closest_) {
        std::cout << "[ControlsManager::interaction] no valid player or info\n";
        return;
    }

    std::cout << "[ControlsManager::interaction] 'E' pressed, checking nearest assets\n";
    int px = player_->pos_X;
    int py = player_->pos_Y - player_->info->z_threshold;
    std::cout << "[ControlsManager::interaction] player position = (" << px << ", " << py << ")\n";

    for (Asset* a : *closest_) {
        if (!a || a == player_ || !a->info || !a->info->has_interaction_area || !a->info->interaction_area) {
            continue;
        }

        Area ia = a->get_area("interaction");
        if (pointInAABB(px, py, ia)) {
            a->change_animation("interaction");
        }
    }
}

// -----------------------------------------------------------------------------
// Handle teleport logic (space to set, ctrl to teleport)
// -----------------------------------------------------------------------------
void ControlsManager::handle_teleport(const std::unordered_set<SDL_Keycode>& keys) {
    if (!player_) return;

    if (keys.count(SDLK_SPACE)) {
        teleport_point_ = { player_->pos_X, player_->pos_Y };
        teleport_set_ = true;
        std::cout << "[Teleport] Point set to (" << teleport_point_.x << ", " << teleport_point_.y << ")\n";
    }

    if (keys.count(SDLK_q)) {
        std::cout << "[Teleport] Q pressed. teleport_set = " << (teleport_set_ ? "true" : "false") << "\n";
    }

    if (keys.count(SDLK_q) && teleport_set_) {
        std::cout << "[Teleport] Attempting teleport\n";
        player_->set_position(teleport_point_.x, teleport_point_.y);
        teleport_point_ = { 0, 0 };
        teleport_set_ = false;
        std::cout << "[Teleport] Teleported to saved point.\n";
    }
}

// -----------------------------------------------------------------------------
// Update both movement and interaction
// -----------------------------------------------------------------------------
void ControlsManager::update(const std::unordered_set<SDL_Keycode>& keys) {

    handle_teleport(keys);
    movement(keys);


    if (keys.count(SDLK_e)) {
        interaction();
    }
}

// -----------------------------------------------------------------------------
// Accessors for movement deltas
// -----------------------------------------------------------------------------
int ControlsManager::get_dx() const { return dx_; }
int ControlsManager::get_dy() const { return dy_; }
