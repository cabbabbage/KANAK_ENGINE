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
        // assume Room has a method to get its assets without moving them
        for (auto& asset_up : room->assets) {
            asset_up->finalize_setup(renderer_);
        }
    }
}

void AssetLoader::load_map_json() {
    std::ifstream f(map_path_ + "/map_info.json");
    if (!f) throw std::runtime_error("Failed to open map_info.json");

    json j; f >> j;

    map_radius_        = j.at("map_radius").get<int>();
    map_boundary_file_ = j.at("map_boundary").get<std::string>();
    map_center_x_      = map_center_y_ = map_radius_;

    for (auto& L : j.at("map_layers")) {
        LayerSpec spec;
        spec.level     = L.at("level").get<int>();
        spec.radius    = L.at("radius").get<int>();
        spec.min_rooms = L.at("min_rooms").get<int>();
        spec.max_rooms = L.at("max_rooms").get<int>();
        for (auto& R : L.at("rooms")) {
            RoomSpec rs;
            rs.name              = R.at("name").get<std::string>();
            rs.min_instances     = R.at("min_instances").get<int>();
            rs.max_instances     = R.at("max_instances").get<int>();
            for (auto& c : R.at("required_children"))
                rs.required_children.push_back(c.get<std::string>());
            spec.rooms.push_back(std::move(rs));
        }
        map_layers_.push_back(std::move(spec));
    }
}

std::vector<Asset> AssetLoader::extract_all_assets() {
    std::vector<Asset> out;
    out.reserve(rooms_.size() * 4);

    for (Room* room : rooms_) {
        // move each Asset into the output vector
        for (auto& aup : room->assets) {
            out.push_back(std::move(*aup));
        }
    }

    // finalize_setup on each
    for (auto& a : out) {
        a.finalize_setup(renderer_);
    }
    return out;
}

std::vector<Area> AssetLoader::getAllRoomAndTrailAreas() const {
    std::vector<Area> areas;
    areas.reserve(rooms_.size());

    for (Room* r : rooms_) {
        areas.push_back(r->room_area);
    }
    return areas;
}

SDL_Texture* AssetLoader::createMinimap(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) return nullptr;

    SDL_Texture* minimap = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width, height
    );
    if (!minimap) {
        std::cerr << "[Minimap] SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        return nullptr;
    }
    SDL_SetTextureBlendMode(minimap, SDL_BLENDMODE_BLEND);

    SDL_Texture* prev = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, minimap);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
    float scaleX = float(width)  / float(map_radius_ * 2);
    float scaleY = float(height) / float(map_radius_ * 2);

    for (Room* room : rooms_) {
        auto [minx, miny, maxx, maxy] = room->room_area.get_bounds();
        SDL_Rect r{ int(std::round(minx * scaleX)),
                    int(std::round(miny * scaleY)),
                    int(std::round((maxx - minx) * scaleX)),
                    int(std::round((maxy - miny) * scaleY)) };
        SDL_RenderFillRect(renderer_, &r);
    }

    SDL_SetRenderTarget(renderer_, prev);
    return minimap;
}
