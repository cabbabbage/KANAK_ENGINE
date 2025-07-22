// === File: generate_trails.cpp ===
#include "generate_trails.hpp"
#include "trail_geometry.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <algorithm>

using json = nlohmann::json;
namespace fs = std::filesystem;

GenerateTrails::GenerateTrails(const std::string& trail_dir)
    : rng_(std::random_device{}())
{
    for (const auto& entry : fs::directory_iterator(trail_dir)) {
        if (entry.path().extension() == ".json") {
            available_assets_.push_back(entry.path().string());
        }
    }

    if (testing) {
        std::cout << "[GenerateTrails] Loaded " << available_assets_.size() << " trail assets\n";
    }

    if (available_assets_.empty()) {
        throw std::runtime_error("[GenerateTrails] No JSON trail assets found");
    }
}

void GenerateTrails::set_all_rooms_reference(const std::vector<Room*>& rooms) {
    all_rooms_reference = rooms;
}

std::vector<std::unique_ptr<Room>> GenerateTrails::generate_trails(
    const std::vector<std::pair<Room*, Room*>>& room_pairs,
    const std::vector<Area>& existing_areas,
    const std::string& map_dir,
    AssetLibrary* asset_lib)
{
    trail_areas_.clear();
    std::vector<std::unique_ptr<Room>> trail_rooms;
    std::vector<Area> all_areas = existing_areas;

    for (const auto& [a, b] : room_pairs) {
        if (testing) {
            std::cout << "[GenerateTrails] Connecting: " << a->room_name
                      << " <--> " << b->room_name << "\n";
        }

        bool success = false;
        for (int attempts = 0; attempts < 1000 && !success; ++attempts) {
            std::string path = pick_random_asset();
            success = TrailGeometry::attempt_trail_connection(
                a, b, all_areas, map_dir, asset_lib, trail_rooms,
                /*allowed_intersections=*/1,
                path, testing, rng_
            );
        }

        if (!success && testing) {
            std::cout << "[TrailGen] Failed to place trail between "
                      << a->room_name << " and " << b->room_name << "\n";
        }
    }

    find_and_connect_isolated(map_dir, asset_lib, all_areas, trail_rooms);

    // Add circular connection pass
    circular_connection(trail_rooms, map_dir, asset_lib, all_areas);

    // Get max layer for heuristic
    int max_layer = 0;
    for (Room* room : all_rooms_reference) {
        if (room && room->layer > max_layer) {
            max_layer = room->layer;
        }
    }

    int passes = static_cast<int>(max_layer / 3);
    for (int i = 0; i < passes; ++i) {
        remove_random_connection(trail_rooms);
        remove_and_connect(trail_rooms, illegal_connections, map_dir, asset_lib, all_areas);
    }


    if (testing) {
        std::cout << "[TrailGen] Total trail rooms created: " << trail_rooms.size() << "\n";
    }

    return trail_rooms;
}

std::string GenerateTrails::pick_random_asset() {
    std::uniform_int_distribution<size_t> dist(0, available_assets_.size() - 1);
    return available_assets_[dist(rng_)];
}

void GenerateTrails::find_and_connect_isolated(
    const std::string& map_dir,
    AssetLibrary* asset_lib,
    std::vector<Area>& existing_areas,
    std::vector<std::unique_ptr<Room>>& trail_rooms)
{
    const int max_passes = 1000000;
    int allowed_intersections = 0;

    for (int pass = 0; pass < max_passes; ++pass) {
        std::unordered_set<Room*> visited;
        std::unordered_set<Room*> connected_to_spawn;
        std::vector<std::vector<Room*>> isolated_groups;

        auto mark_connected = [&](Room* room, auto&& self) -> void {
            if (!room || connected_to_spawn.count(room)) return;
            connected_to_spawn.insert(room);
            for (Room* neighbor : room->connected_rooms) {
                self(neighbor, self);
            }
        };

        auto collect_group = [&](Room* room, std::vector<Room*>& group, auto&& self) -> void {
            if (!room || visited.count(room) || connected_to_spawn.count(room)) return;
            visited.insert(room);
            group.push_back(room);
            for (Room* neighbor : room->connected_rooms) {
                self(neighbor, group, self);
            }
        };

        for (Room* room : all_rooms_reference) {
            if (room && room->layer == 0) {
                mark_connected(room, mark_connected);
                break;
            }
        }

        for (Room* room : all_rooms_reference) {
            if (!visited.count(room) && !connected_to_spawn.count(room)) {
                std::vector<Room*> group;
                collect_group(room, group, collect_group);
                if (!group.empty()) {
                    isolated_groups.push_back(std::move(group));
                }
            }
        }

        if (isolated_groups.empty()) {
            if (testing) {
                std::cout << "[ConnectIsolated] All rooms connected after " << pass << " passes.\n";
            }
            break;
        }

        if (testing) {
            std::cout << "[ConnectIsolated] Pass " << pass + 1 << " - " << isolated_groups.size()
                      << " disconnected groups found | allowed intersections: "
                      << allowed_intersections << "\n";
        }

        bool any_connection_made = false;

        for (const auto& group : isolated_groups) {
            if (group.empty()) continue;

            std::vector<Room*> sorted_group = group;
            std::sort(sorted_group.begin(), sorted_group.end(), [](Room* a, Room* b) {
                return a->connected_rooms.size() < b->connected_rooms.size();
            });

            for (Room* roomA : sorted_group) {
                std::vector<Room*> candidates;

                for (Room* candidate : all_rooms_reference) {
                    if (candidate == roomA || connected_to_spawn.count(candidate)) continue;

                    // Check if (roomA, candidate) is an illegal pair
                    bool illegal = std::any_of(illegal_connections.begin(), illegal_connections.end(),
                        [&](const std::pair<Room*, Room*>& p) {
                            return (p.first == roomA && p.second == candidate) ||
                                (p.first == candidate && p.second == roomA);
                        });
                    if (illegal) continue;

                    std::unordered_set<Room*> check_visited;
                    std::function<bool(Room*)> dfs = [&](Room* current) -> bool {
                        if (!current || check_visited.count(current)) return false;
                        if (current->layer == 0) return true;
                        check_visited.insert(current);
                        for (Room* neighbor : current->connected_rooms) {
                            if (dfs(neighbor)) return true;
                        }
                        return false;
                    };

                    if (dfs(candidate)) {
                        candidates.push_back(candidate);
                    }
                }


                if (candidates.empty()) continue;

                std::sort(candidates.begin(), candidates.end(), [](Room* a, Room* b) {
                    return a->connected_rooms.size() < b->connected_rooms.size();
                });

                if (candidates.size() > 5) candidates.resize(5);

                for (Room* roomB : candidates) {
                    for (int attempt = 0; attempt < 100; ++attempt) {
                        std::string path = pick_random_asset();
                        if (TrailGeometry::attempt_trail_connection(
                                roomA, roomB, existing_areas, map_dir,
                                asset_lib, trail_rooms,
                                allowed_intersections,
                                path, testing, rng_)) {
                            any_connection_made = true;
                            goto next_group;
                        }
                    }
                }
            }
        next_group:;
        }

        if (!any_connection_made && testing) {
            std::cout << "[ConnectIsolated] No connections made on pass " << pass + 1 << "\n";
        }

        if ((pass + 1) % 5 == 0) {
            ++allowed_intersections;
            if (testing) {
                std::cout << "[ConnectIsolated] Increasing allowed intersections to " << allowed_intersections << "\n";
            }
        }
    }
}





void GenerateTrails::remove_connection(Room* a, Room* b, std::vector<std::unique_ptr<Room>>& trail_rooms) {
    if (!a || !b) return;

    // Remove bidirectional connections
    a->remove_connecting_room(b);
    b->remove_connecting_room(a);

    // Remove the connecting trail_room, if it exists
    trail_rooms.erase(
        std::remove_if(trail_rooms.begin(), trail_rooms.end(),
            [&](const std::unique_ptr<Room>& trail) {
                if (!trail) return false;
                bool connects_a = false, connects_b = false;
                for (Room* r : trail->connected_rooms) {
                    if (r == a) connects_a = true;
                    if (r == b) connects_b = true;
                }
                return connects_a && connects_b;
            }),
        trail_rooms.end()
    );
}


void GenerateTrails::remove_random_connection(std::vector<std::unique_ptr<Room>>& trail_rooms) {
    if (trail_rooms.empty()) return;

    std::uniform_int_distribution<size_t> dist(0, trail_rooms.size() - 1);
    size_t index = dist(rng_);
    Room* trail = trail_rooms[index].get();
    if (!trail || trail->connected_rooms.size() < 2) return;

    Room* a = trail->connected_rooms[0];
    Room* b = trail->connected_rooms[1];

    if (a && b) {
        a->remove_connecting_room(b);
        b->remove_connecting_room(a);
    }

    trail_rooms.erase(trail_rooms.begin() + index);
}


void GenerateTrails::remove_and_connect(std::vector<std::unique_ptr<Room>>& trail_rooms,
                                        std::vector<std::pair<Room*, Room*>>& illegal_connections,
                                        const std::string& map_dir,
                                        AssetLibrary* asset_lib,
                                        std::vector<Area>& existing_areas) 
{
    Room* target = nullptr;

    // Step 1: Find the room with layer > 2 and the most connections
    for (Room* room : all_rooms_reference) {
        if (room && room->layer > 2 && room->connected_rooms.size() > 3) {
            if (!target || room->connected_rooms.size() > target->connected_rooms.size()) {
                target = room;
            }
        }
    }

    if (!target || target->connected_rooms.size() < 1) return;

    Room* most_connected = nullptr;

    // Step 2: Among target's connections, find the one with the most connections
    for (Room* neighbor : target->connected_rooms) {
        if (neighbor->connected_rooms.size() <= 3) continue;
        if (!most_connected || neighbor->connected_rooms.size() > most_connected->connected_rooms.size()) {
            most_connected = neighbor;
        }
    }

    // If no eligible connection was found, skip
    if (!most_connected) return;

    // Step 3: Remove the connection and mark it as illegal
    remove_connection(target, most_connected, trail_rooms);
    illegal_connections.emplace_back(target, most_connected);

    // Step 4: Attempt to reconnect any isolated components
    find_and_connect_isolated(map_dir, asset_lib, existing_areas, trail_rooms);
}



void GenerateTrails::circular_connection(std::vector<std::unique_ptr<Room>>& trail_rooms,
                                         const std::string& map_dir,
                                         AssetLibrary* asset_lib,
                                         std::vector<Area>& existing_areas)
{
    if (all_rooms_reference.empty()) return;

    // Step 1: Find a room on the outermost layer
    Room* outermost = nullptr;
    int max_layer = -1;
    for (Room* room : all_rooms_reference) {
        if (room && room->layer > max_layer) {
            max_layer = room->layer;
            outermost = room;
        }
    }

    if (!outermost) return;

    // Step 2: Build direct lineage (parent chain up to spawn)
    std::vector<Room*> direct_lineage;
    for (Room* r = outermost; r; r = r->parent) {
        direct_lineage.push_back(r);
        if (r->layer == 0) break;
    }

    if (direct_lineage.empty()) return;

    // Step 3: Walk around the circle toward reconnecting to lineage
    Room* current = outermost;

    while (std::find(direct_lineage.begin(), direct_lineage.end(), current) == direct_lineage.end()) {
        std::vector<Room*> candidates;

        Room* right = current->right_sibling;
        if (right && right->layer > 1) {
            candidates.push_back(right);
            if (right->parent && right->parent->layer > 1)
                candidates.push_back(right->parent);

            for (Room* child : right->connected_rooms) {
                if (child->parent == right && child->layer > 1) {
                    candidates.push_back(child);
                    break;
                }
            }
        }

        // Filter out already connected or invalid candidates
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [&](Room* c) {
                    return !c || std::find(current->connected_rooms.begin(),
                                           current->connected_rooms.end(), c) != current->connected_rooms.end();
                }),
            candidates.end()
        );

        if (candidates.empty()) break;

        // Pick a random candidate
        Room* next = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(rng_)];

        // Pick a trail asset and attempt connection
        for (int attempt = 0; attempt < 100; ++attempt) {
            std::string path = pick_random_asset();
            if (TrailGeometry::attempt_trail_connection(current, next, existing_areas, map_dir,
                                                        asset_lib, trail_rooms,
                                                        /*allowed_intersections=*/1,
                                                        path, testing, rng_)) {
                current = next;
                break;
            }
        }
    }
}
