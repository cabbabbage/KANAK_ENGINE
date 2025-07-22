// === File: asset_loader.cpp ===
#include "asset_loader.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AssetLoader::AssetLoader(const std::string& map_dir, SDL_Renderer* renderer)
  : map_path_(map_dir),
    renderer_(renderer),
    rng_(std::random_device{}())
{
    load_map_json();

    asset_library_ = std::make_unique<AssetLibrary>();

    // Generate rooms and trails (no renderer passed into generator)
    GenerateRooms generator(map_layers_, map_center_x_, map_center_y_, map_path_);
    auto room_ptrs = generator.build(asset_library_.get(),
                                     map_radius_,
                                     map_boundary_file_);

    for (auto& up : room_ptrs) {
        rooms_.push_back(up.get());
        all_rooms_.push_back(std::move(up));
    }

    // Load all animations from AssetInfo data
    asset_library_->loadAllAnimations(renderer_);

    // Finalize every spawned Asset with lighting/textures
    for (Room* room : rooms_) {
        for (auto& asset_up : room->assets) {
            asset_up->finalize_setup(renderer_);
        }
    }
}

void AssetLoader::load_map_json() {
    std::ifstream f(map_path_ + "/map_info.json");
    if (!f) throw std::runtime_error("Failed to open map_info.json");

    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    if (!j.contains("map_radius") || !j.contains("map_boundary") || !j.contains("map_layers")) {
        throw std::runtime_error("map_info.json missing required fields");
    }

    map_radius_        = j.value("map_radius", 0);
    map_boundary_file_ = j.value("map_boundary", "");
    map_center_x_      = map_center_y_ = map_radius_;

    for (const auto& L : j["map_layers"]) {
        LayerSpec spec;
        spec.level     = L.value("level", 0);
        spec.radius    = L.value("radius", 0);
        spec.min_rooms = L.value("min_rooms", 0);
        spec.max_rooms = L.value("max_rooms", 0);

        if (!L.contains("rooms") || !L["rooms"].is_array()) {
            std::cerr << "[AssetLoader] Warning: Layer missing valid 'rooms' array.\n";
            continue;
        }

        for (const auto& R : L["rooms"]) {
            RoomSpec rs;
            rs.name = R.value("name", "unnamed");
            rs.min_instances = R.value("min_instances", 1);
            rs.max_instances = R.value("max_instances", 1);

            if (R.contains("required_children") && R["required_children"].is_array()) {
                for (const auto& c : R["required_children"]) {
                    if (c.is_string()) rs.required_children.push_back(c.get<std::string>());
                }
            }

            spec.rooms.push_back(std::move(rs));
        }

        map_layers_.push_back(std::move(spec));
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

    for (auto& a : out) {
        a.finalize_setup(renderer_);
    }
    return out;
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
