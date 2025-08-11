// === File: asset_loader.cpp ===
#include "asset_loader.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <queue>
#include <numeric>

using json = nlohmann::json;

namespace {
    size_t countAssets(const std::vector<Room*>& rooms) {
        size_t total = 0;
        for (auto* r : rooms) total += r->assets.size();
        return total;
    }

    Asset* findCenterAsset(const std::vector<Asset*>& group) {
        if (group.empty()) return nullptr;
        double avgX = std::accumulate(group.begin(), group.end(), 0.0,
            [](double sum, Asset* a) { return sum + a->pos_X; }) / group.size();
        double avgY = std::accumulate(group.begin(), group.end(), 0.0,
            [](double sum, Asset* a) { return sum + a->pos_Y; }) / group.size();

        Asset* center = group.front();
        double bestDistSq = std::numeric_limits<double>::infinity();
        for (auto* a : group) {
            double dx = a->pos_X - avgX;
            double dy = a->pos_Y - avgY;
            double distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                center = a;
            }
        }
        return center;
    }

    void logCountChange(const std::string& label, size_t before, size_t after) {
        std::cout << "[AssetLoader] " << label
                  << " before: " << before
                  << ", after: " << after
                  << " (" << (before - after) << " removed)\n";
    }
}

AssetLoader::AssetLoader(const std::string& map_dir, SDL_Renderer* renderer)
    : map_path_(map_dir),
      renderer_(renderer),
      rng_(std::random_device{}())
{
    const int fade_start_distance = 0;
    const int fade_end_distance   = 1400;

    load_map_json();
    asset_library_ = std::make_unique<AssetLibrary>();

    loadRooms();
    asset_library_->loadAllAnimations(renderer_);
    finalizeAssets();

    validateAndRemoveInvalidTextures(); 

    size_t before_merge_count = countAssets(rooms_);
    auto distant_assets       = collectDistantAssets(fade_start_distance, fade_end_distance);
    auto grouped_distant      = group_neighboring_assets(distant_assets, 1000, 1000, "Distant Boundary");
    mergeDistantAssets(grouped_distant);
    size_t after_merge_count  = countAssets(rooms_);
    logCountChange("Merge", before_merge_count, after_merge_count);

    // Child linking
    std::vector<Asset*> non_merged_assets;
    for (Room* room : rooms_) {
        for (auto& asset_up : room->assets) {
            if (auto* asset = asset_up.get();
                asset && asset->get_merge() && asset->info && asset->info->type != "Player")
            {
                non_merged_assets.push_back(asset);
            }

        }
    }

    //size_t before_link_count = after_merge_count;
    auto neighbor_assets     = group_neighboring_assets(non_merged_assets, 2000, 2000, "Child Linking");
    link_by_child(neighbor_assets);
    //size_t after_link_count  = countAssets(rooms_);
    //logCountChange("Linking", before_link_count, after_link_count);
}


size_t AssetLoader::validateAndRemoveInvalidTextures() {
    size_t removed_count = 0;

    auto validateTexture = [](SDL_Texture* tex) -> bool {
        if (!tex) return false;
        int w = 0, h = 0;
        return (SDL_QueryTexture(tex, nullptr, nullptr, &w, &h) == 0 && w > 0 && h > 0);
    };

    for (Room* room : rooms_) {
        auto& assets_vec = room->assets;
        auto it = assets_vec.begin();
        while (it != assets_vec.end()) {
            Asset* asset = it->get();
            bool valid = false;

            if (asset) {
                SDL_Texture* frame = asset->get_current_frame();
                if (frame && validateTexture(frame)) {
                    valid = true;
                }
            }

            if (!valid) {
                std::cerr << "[AssetLoader] Removing asset with invalid texture: "
                          << (asset && asset->info ? asset->info->name : "unknown") << "\n";
                it = assets_vec.erase(it);
                removed_count++;
            } else {
                ++it;
            }
        }
    }

    std::cout << "[AssetLoader] Removed " << removed_count
              << " assets due to invalid textures.\n";
    return removed_count;
}


void AssetLoader::link_by_child(const std::vector<std::vector<Asset*>>& groups) {
    size_t total_linked = 0;
    for (const auto& group : groups) {
        if (group.empty()) continue;
        Asset* center_asset = findCenterAsset(group);
        if (!center_asset) continue;

        for (auto* a : group) {
            if (a != center_asset) {
                center_asset->add_child(a);
                total_linked++;
            }
        }
        removeMergedAssets({group.begin(), group.end()}, center_asset);
    }
    std::cout << "[link_by_child] Linked " << total_linked << " assets as children.\n";
}

void AssetLoader::removeMergedAssets(const std::vector<Asset*>& to_remove, Asset* skip) {
    for (Asset* a : to_remove) {
        if (a == skip) continue;
        for (Room* room : rooms_) {
            auto& vec = room->assets;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                        [&](auto& up) { return up.get() == a; }),
                    vec.end());
        }
    }
}


std::vector<std::vector<Asset*>> AssetLoader::group_neighboring_assets(
    const std::vector<Asset*>& assets, 
    int tile_width, 
    int tile_height, 
    const std::string& group_type)
{
    std::unordered_map<long long, std::vector<Asset*>> tile_map;

    auto make_tile_key = [&](int tx, int ty) -> long long {
        return (static_cast<long long>(tx) << 32) ^ static_cast<unsigned long long>(ty);
    };

    // Assign each asset to a tile
    for (Asset* a : assets) {
        if (!a) continue;
        int tx = (a->pos_X >= 0) ? (a->pos_X / tile_width)
                                 : ((a->pos_X - (tile_width - 1)) / tile_width);
        int ty = (a->pos_Y >= 0) ? (a->pos_Y / tile_height)
                                 : ((a->pos_Y - (tile_height - 1)) / tile_height);

        tile_map[make_tile_key(tx, ty)].push_back(a);
    }

    // Move non-empty groups into final vector
    std::vector<std::vector<Asset*>> groups;
    groups.reserve(tile_map.size());
    for (auto& [key, group] : tile_map) {
        if (!group.empty()) {
            groups.push_back(std::move(group));
        }
    }

    // Stats output
    size_t total_assets = 0;
    size_t largest_group = 0;
    for (const auto& g : groups) {
        total_assets += g.size();
        largest_group = std::max(largest_group, g.size());
    }
    double avg_group_size = groups.empty() ? 0.0 : (double)total_assets / groups.size();

    std::cout << "[" << group_type << "] Created " << groups.size()
              << " tile groups, total assets: " << total_assets
              << ", avg group size: " << avg_group_size
              << ", largest group: " << largest_group << "\n";

    return groups;
}
void AssetLoader::mergeDistantAssets(const std::vector<std::vector<Asset*>>& groups) {
    std::vector<Asset*> to_remove;

    try {
        for (size_t gi = 0; gi < groups.size(); ++gi) {
            const auto& group = groups[gi];
            if (group.empty()) continue;

            // Find the asset lowest on the screen (largest pos_Y)
            Asset* base = *std::max_element(group.begin(), group.end(),
                [](Asset* a, Asset* b) {
                    if (!a) return true;
                    if (!b) return false;
                    return a->pos_Y < b->pos_Y; // lower screen = higher Y
                });

            if (!base) {
                std::cerr << "[mergeDistantAssets] Group " << gi
                          << " has no valid base asset.\n";
                continue;
            }

            // Remove all other assets in the group
            for (Asset* other : group) {
                if (other == base) continue;
                if (!other) continue;
                other->set_remove();
                to_remove.push_back(other);
            }

            // Change animation to "group"
            try {
                base->change_animation("group");
            } catch (const std::exception& e) {
                std::cerr << "[mergeDistantAssets] Exception changing animation for base: "
                          << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[mergeDistantAssets] Fatal exception in merge loop: "
                  << e.what() << "\n";
    } catch (...) {
        std::cerr << "[mergeDistantAssets] Unknown fatal error in merge loop.\n";
    }

    try {
        removeMergedAssets(to_remove);
    } catch (const std::exception& e) {
        std::cerr << "[mergeDistantAssets] Exception during removeMergedAssets: "
                  << e.what() << "\n";
    } catch (...) {
        std::cerr << "[mergeDistantAssets] Unknown error during removeMergedAssets.\n";
    }
}






std::vector<Asset*> AssetLoader::collectDistantAssets(int fade_start_distance, int fade_end_distance) {
    std::vector<Asset*> distant_assets;
    distant_assets.reserve(rooms_.size() * 4);

    auto allZones = getAllRoomAndTrailAreas();

    for (Room* room : rooms_) {
        for (auto& asset_up : room->assets) {
            Asset* asset = asset_up.get();
            if (!asset->info || asset->info->type != "boundary") {
                asset->alpha_percentage = 1.0;
                asset->has_base_shadow  = false;
                asset->gradient_shadow  = true;
                continue;
            }

            bool is_inside = false;
            for (const Area& zone : allZones) {
                if (zone.contains_point({asset->pos_X, asset->pos_Y})) {
                    is_inside = true;
                    break;
                }
            }

            if (!is_inside) {
                double minDist = std::numeric_limits<double>::infinity();
                for (const Area& zone : allZones) {
                    const auto& pts = zone.get_points();
                    for (size_t i = 0; i + 1 < pts.size(); ++i) {
                        auto [x1, y1] = pts[i];
                        auto [x2, y2] = pts[(i + 1) % pts.size()];
                        double vx = x2 - x1, vy = y2 - y1;
                        double wx = asset->pos_X - x1, wy = asset->pos_Y - y1;
                        double len2 = vx * vx + vy * vy;
                        double t = len2 > 0.0 ? (vx * wx + vy * wy) / len2 : 0.0;
                        t = std::clamp(t, 0.0, 1.0);
                        double projx = x1 + t * vx;
                        double projy = y1 + t * vy;
                        double dx = projx - asset->pos_X;
                        double dy = projy - asset->pos_Y;
                        minDist = std::min(minDist, std::hypot(dx, dy));
                    }
                }

                double alpha = 0.0;
                if (minDist <= fade_start_distance) alpha = 1.0;
                else if (minDist >= fade_end_distance) alpha = 0.0;
                else {
                    double t = (minDist - fade_start_distance) /
                               (fade_end_distance - fade_start_distance);
                    alpha = std::pow(1.0 - t, 2.0);
                }

                asset->alpha_percentage = alpha * 1.2;
                bool distant = !(alpha > 0.3);
                asset->static_frame = distant;
                if (distant) distant_assets.push_back(asset);
            }
        }
    }

    return distant_assets;
}

void AssetLoader::loadRooms() {
    GenerateRooms generator(map_layers_, map_center_x_, map_center_y_, map_path_);
    auto room_ptrs = generator.build(asset_library_.get(), map_radius_, map_boundary_file_);

    for (auto& up : room_ptrs) {
        rooms_.push_back(up.get());
        all_rooms_.push_back(std::move(up));
    }
}

void AssetLoader::finalizeAssets() {
    for (Room* room : rooms_) {
        for (auto& asset_up : room->assets) {
            asset_up->finalize_setup(renderer_);
        }
    }
}

std::vector<Asset> AssetLoader::extract_all_assets() {
    std::vector<Asset> out;
    out.reserve(rooms_.size() * 4);
    for (Room* room : rooms_) {
        for (auto& aup : room->assets) {
            out.push_back(std::move(*aup));
        }
    }
    return out;
}

std::unique_ptr<Assets> AssetLoader::createAssets(int screen_width, int screen_height) {
    std::cout << "[AssetLoader] createAssets() start\n";
    auto assetsVec = extract_all_assets();
    std::cout << "[AssetLoader] extracted " << assetsVec.size() << " assets\n";

    Asset* player_ptr = nullptr;
    for (size_t i = 0; i < assetsVec.size(); ++i) {
        auto& a = assetsVec[i];
        if (a.info && a.info->type == "Player") {
            player_ptr = &a;
            std::cout << "[AssetLoader] found player asset '" << a.info->name
                      << "' at index " << i << "\n";
            break;
        }
    }
    if (!player_ptr) {
        std::cerr << "[AssetLoader] ERROR: no player asset found\n";
        throw std::runtime_error("No player asset found in extracted assets");
    }

    std::cout << "[AssetLoader] constructing Assets manager\n";
    auto assetsPtr = std::make_unique<Assets>(
        std::move(assetsVec),
        player_ptr,
        screen_width,
        screen_height,
        player_ptr->pos_X,
        player_ptr->pos_Y, 
        static_cast<int>(map_radius_ * 1.2)

    );
    std::cout << "[AssetLoader] createAssets() complete\n";
    return assetsPtr;
}

std::vector<Area> AssetLoader::getAllRoomAndTrailAreas() const {
    std::vector<Area> areas;
    areas.reserve(rooms_.size());
    for (Room* r : rooms_) {
        areas.push_back(*r->room_area);
    }
    return areas;
}

SDL_Texture* AssetLoader::createMinimap(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) return nullptr;

    int scaleFactor = 2;
    int render_width  = width  * scaleFactor;
    int render_height = height * scaleFactor;

    SDL_Texture* highres = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        render_width, render_height
    );
    if (!highres) {
        std::cerr << "[Minimap] Failed to create high-res texture: " << SDL_GetError() << "\n";
        return nullptr;
    }

    SDL_SetTextureBlendMode(highres, SDL_BLENDMODE_BLEND);
    SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, highres);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    float scaleX = float(render_width)  / float(map_radius_ * 2);
    float scaleY = float(render_height) / float(map_radius_ * 2);

    for (Room* room : rooms_) {
        try {
            auto [minx, miny, maxx, maxy] = room->room_area->get_bounds();
            SDL_Rect r{ int(std::round(minx * scaleX)),
                        int(std::round(miny * scaleY)),
                        int(std::round((maxx - minx) * scaleX)),
                        int(std::round((maxy - miny) * scaleY)) };

            if (room->room_name.find("trail") != std::string::npos) {
                SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 255);
                int cx = int(std::round((minx + maxx) * 0.5 * scaleX));
                int cy = int(std::round((miny + maxy) * 0.5 * scaleY));
                for (Room* connected : room->connected_rooms) {
                    auto [tx1, ty1, tx2, ty2] = connected->room_area->get_bounds();
                    int tcx = int(std::round((tx1 + tx2) * 0.5 * scaleX));
                    int tcy = int(std::round((ty1 + ty2) * 0.5 * scaleY));
                    SDL_RenderDrawLine(renderer_, cx, cy, tcx, tcy);
                }
            } else {
                SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer_, &r);
            }
        } catch (const std::exception& e) {
            std::cerr << "[Minimap] Skipping room with invalid bounds: " << e.what() << "\n";
        }
    }

    SDL_SetRenderTarget(renderer_, prev);

    SDL_Texture* final = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width, height
    );
    if (!final) {
        std::cerr << "[Minimap] Failed to create final texture: " << SDL_GetError() << "\n";
        SDL_DestroyTexture(highres);
        return nullptr;
    }

    SDL_SetRenderTarget(renderer_, final);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    SDL_Rect src{ 0, 0, render_width, render_height };
    SDL_Rect dst{ 0, 0, width, height };
    SDL_RenderCopy(renderer_, highres, &src, &dst);

    SDL_SetRenderTarget(renderer_, prev);
    SDL_DestroyTexture(highres);

    return final;
}

void AssetLoader::load_map_json() {
    std::ifstream f(map_path_ + "/map_info.json");
    if (!f) throw std::runtime_error("Failed to open map_info.json");
    json j;
    f >> j;

    map_radius_        = j.value("map_radius", 0);
    map_boundary_file_ = j.value("map_boundary", "");
    map_center_x_ = map_center_y_ = map_radius_;

    for (const auto& L : j["map_layers"]) {
        LayerSpec spec;
        spec.level     = L.value("level", 0);
        spec.radius    = L.value("radius", 0);
        spec.min_rooms = L.value("min_rooms", 0);
        spec.max_rooms = L.value("max_rooms", 0);
        for (const auto& R : L["rooms"]) {
            RoomSpec rs;
            rs.name = R.value("name", "unnamed");
            rs.min_instances = R.value("min_instances", 1);
            rs.max_instances = R.value("max_instances", 1);
            if (R.contains("required_children")) {
                for (const auto& c : R["required_children"])
                    rs.required_children.push_back(c.get<std::string>());
            }
            spec.rooms.push_back(std::move(rs));
        }
        map_layers_.push_back(std::move(spec));
    }
}