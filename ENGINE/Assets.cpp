// assets.cpp

#include "assets.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

Assets::Assets(const std::string& map_json, SDL_Renderer* renderer)
    : renderer(renderer) {

    std::ifstream in(map_json);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open map json: " + map_json);
    }

    nlohmann::json root;
    in >> root;

    if (!root.contains("perimeter") || !root["perimeter"].is_array()) {
        throw std::runtime_error("[Assets] 'perimeter' missing or invalid");
    }

    parse_perimeter(root);
    create_asset_info(root, renderer);
    generate_assets(root);
    sort_assets_by_distance();

    player = nullptr;
    for (auto& asset : all) {
        if (asset.info && asset.info->type == "Player" && asset.info->name == "Davey") {
            player = &asset;
            break;
        }
    }

    if (!player) {
        throw std::runtime_error("[Assets] Player asset not found after sorting.");
    }
}

void Assets::update(const std::unordered_set<SDL_Keycode>& keys) {
    int dx = 0, dy = 0;

    for (SDL_Keycode key : keys) {
        if (key == SDLK_w) dy -= 5;
        else if (key == SDLK_s) dy += 5;
        else if (key == SDLK_a) dx -= 5;
        else if (key == SDLK_d) dx += 5;
    }

    for (auto& asset : all) {
        bool player_is_moving = !keys.empty();
        asset.update((asset.info->type != "Player") || player_is_moving);
    }

    if (player) {
        player->set_position(player->pos_X + dx, player->pos_Y + dy);

        int player_z_global = player->pos_Y + player->info->z_threshold;
        int best_candidate_z = std::numeric_limits<int>::min();
        int best_candidate_index = 0;

        for (const auto& asset : all) {
            if (&asset == player || !asset.info) continue;

            int other_z = asset.pos_Y + asset.info->z_threshold;

            if (other_z < player_z_global && other_z > best_candidate_z) {
                best_candidate_z = other_z;
                best_candidate_index = asset.z_index;
            }
        }

        // Set player behind the one directly beneath it, in z-threshold space
        player->set_z_index(best_candidate_index + 1);
    }


    sort_assets_by_distance();

    for (auto& asset : all) {
        if (asset.info && asset.info->type == "Player" && asset.info->name == "Davey") {
            player = &asset;
            break;
        }
    }
}


void Assets::parse_perimeter(const nlohmann::json& root) {
    std::vector<Point> perimeter;
    for (const auto& point : root["perimeter"]) {
        if (point.is_array() && point.size() == 2 &&
            point[0].is_number_integer() && point[1].is_number_integer()) {
            perimeter.emplace_back(point[0].get<int>(), point[1].get<int>());
        }
    }
    if (perimeter.empty()) {
        throw std::runtime_error("[Assets] Perimeter array is empty or invalid.");
    }

    map_area = Area(perimeter);
}

void Assets::create_asset_info(const nlohmann::json& root, SDL_Renderer* renderer) {
    if (!root.contains("assets") || !root["assets"].is_array()) {
        throw std::runtime_error("[Assets] 'assets' missing or not an array.");
    }

    bool player_info_found = false;

    for (const auto& entry : root["assets"]) {
        if (!entry.contains("name")) {
            throw std::runtime_error("[Assets] Asset entry missing 'name'");
        }

        std::string name = entry["name"];
        if (asset_info_library.find(name) != asset_info_library.end()) continue;

        AssetInfo* info = new AssetInfo(name, renderer);

        if (info->type == "Player") {
            if (player_info_found) {
                throw std::runtime_error("[Assets] Multiple AssetInfo entries with asset_type == 'Player'");
            }
            player_info_found = true;
        }

        asset_info_library[name] = info;
    }

    if (!player_info_found) {
        throw std::runtime_error("[Assets] No AssetInfo with asset_type == 'Player'");
    }
}

void Assets::spawn_child_assets(AssetInfo* parent_info, const Asset& parent) {
    const int CHILD_OFFSET_X = -90;  // adjust for fine tuning
    const int CHILD_OFFSET_Y = -90;

    for (const auto& child : parent_info->child_assets) {
        std::string child_type = child.asset;
        std::string area_file_path = "SRC/" + parent_info->name + "/" + child.area_file;

        if (!fs::exists(area_file_path)) {
            std::cerr << "[ChildSpawner] Missing child area file: " << area_file_path << "\n";
            continue;
        }

        Area spawn_area(area_file_path);
        spawn_area.apply_offset(parent.pos_X, parent.pos_Y);

        if (!asset_info_library.count(child_type)) {
            asset_info_library[child_type] = new AssetInfo(child_type, renderer);
        }

        AssetInfo* child_info = asset_info_library[child_type];

        Area spacing_template;
        spacing_template.points = {
            {0, 5}, {5, 0}, {0, -5}, {-5, 0}
        };

        std::uniform_int_distribution<int> count_dist(child.min, child.max);
        int count = count_dist(rng);

        const int max_attempts = 20 * count;
        int placed = 0, attempts = 0;

        while (placed < count && attempts++ < max_attempts) {
            auto [min_x, min_y, max_x, max_y] = spawn_area.get_bounds();
            std::uniform_int_distribution<int> x_dist(min_x, max_x);
            std::uniform_int_distribution<int> y_dist(min_y, max_y);
            int px = x_dist(rng);
            int py = y_dist(rng);

            Area candidate_spacing = spacing_template;
            candidate_spacing.set_position(px, py);

            if (!spawn_area.intersects(candidate_spacing)) continue;

            bool overlaps = false;
            for (const auto& existing : all) {
                if (!existing.info || !existing.info->has_spacing_area) continue;
                Area existing_spacing = existing.get_global_spacing_area();
                if (existing_spacing.intersects(candidate_spacing)) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                Asset a(child.z_offset, spawn_area);
                a.info = child_info;

                const int final_x = px + CHILD_OFFSET_X;
                const int final_y = py + CHILD_OFFSET_Y;
                a.set_position(final_x, final_y);

                // Compute and apply z-index
                if (child.z_offset == 0) {
                    a.set_z_index(final_y + child_info->z_threshold);
                } else {
                    a.set_z_index(parent.z_index + child.z_offset);
                }

                all.emplace_back(std::move(a));
                ++placed;
            }
        }

        if (placed < count) {
            std::cerr << "[ChildSpawner] Only placed " << placed << "/" << count
                      << " child assets of type: " << child_type << "\n";
        }
    }
}


void Assets::generate_assets(const nlohmann::json& root) {
    bool player_instance_spawned = false;

    for (const auto& entry : root["assets"]) {
        std::string type = entry["name"];
        int min_n = entry.value("min_number", 1);
        int max_n = entry.value("max_number", 1);
        std::uniform_int_distribution<int> count_dist(min_n, max_n);
        int spawn_count = count_dist(rng);

        if (!asset_info_library.count(type)) {
            throw std::runtime_error("[Assets] No AssetInfo for type: " + type);
        }

        AssetInfo* info = asset_info_library[type];

        for (int i = 0; i < spawn_count; ++i) {
            bool placed = false;

            for (int attempt = 0; attempt < 3 && !placed; ++attempt) {
                Point pos = get_random_position_within_perimeter();
                Asset candidate(0, map_area);
                candidate.info = info;
                candidate.set_position(pos.first, pos.second);

                if (info->type == "Object" && candidate.current_animation == "default") {
                    auto it = info->animations.find("default");
                    if (it != info->animations.end() && !it->second.frames.empty()) {
                        std::uniform_int_distribution<int> frame_dist(0, static_cast<int>(it->second.frames.size()) - 1);
                        candidate.current_frame_index = frame_dist(rng);
                    }
                }

                // Construct spacing area
                Area spacing = info->has_spacing_area
                    ? candidate.get_global_spacing_area()
                    : Area({{0,5}, {5,0}, {0,-5}, {-5,0}});
                if (!info->has_spacing_area) spacing.set_position(pos.first, pos.second);

                // Check for overlap with all previous non-child assets
                bool overlaps = false;
                for (const auto& existing : all) {
                    if (!existing.info || existing.info->child_only) continue;

                    Area other = existing.info->has_spacing_area
                        ? existing.get_global_spacing_area()
                        : Area({{0,5}, {5,0}, {0,-5}, {-5,0}});
                    if (!existing.info->has_spacing_area)
                        other.set_position(existing.pos_X, existing.pos_Y);

                    if (spacing.intersects(other)) {
                        overlaps = true;
                        break;
                    }
                }

                if (!overlaps) {
                    all.emplace_back(std::move(candidate));
                    Asset& just_added = all.back();
                    just_added.set_z_index(just_added.pos_Y + info->z_threshold);
                    placed = true;

                    if (info->type == "Player" && type == "Davey") {
                        if (player_instance_spawned)
                            throw std::runtime_error("[Assets] Multiple Player asset instances");
                        player = &just_added;
                        player_instance_spawned = true;
                    }

                    spawn_child_assets(info, just_added);
                }
            }

            if (!placed) {
                std::cerr << "[Spawner] Failed to place asset: " << info->name << " after 3 attempts\n";
                if (info->type == "Player" && type == "Davey")
                    std::cerr << "[Spawner] Failed to place player asset Davey after all attempts.\n";
            }
        }
    }

    if (!player_instance_spawned) {
        throw std::runtime_error("[Assets] No Asset instance with asset_type == 'Player'");
    }
}


void Assets::sort_assets_by_distance() {
    std::sort(all.begin(), all.end(), [](const Asset& a, const Asset& b) {
        return (a.pos_Y + a.z_offset) < (b.pos_Y + b.z_offset);
    });
}

Assets::Point Assets::get_random_position_within_perimeter() {
    if (map_area.points.size() != 4) return { 0, 0 };
    int min_x = map_area.points[0].first;
    int min_y = map_area.points[0].second;
    int max_x = map_area.points[2].first;
    int max_y = map_area.points[2].second;

    std::uniform_int_distribution<int> x_dist(min_x, max_x);
    std::uniform_int_distribution<int> y_dist(min_y, max_y);
    return { x_dist(rng), y_dist(rng) };
}

Assets::Point Assets::get_random_position_within_area(const Area& area) {
    auto [min_x, min_y, max_x, max_y] = area.get_bounds();
    std::uniform_int_distribution<int> x_dist(min_x, max_x);
    std::uniform_int_distribution<int> y_dist(min_y, max_y);
    return { x_dist(rng), y_dist(rng) };
}

bool areas_overlap(const Area& a, const Area& b) {
    return a.intersects(b);
}