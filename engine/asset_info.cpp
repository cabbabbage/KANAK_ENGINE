// === File: asset_info.cpp ===
#include "asset_info.hpp"
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

static SDL_BlendMode parse_blend_mode(const std::string& mode_str) {
    if (mode_str == "SDL_BLENDMODE_NONE") return SDL_BLENDMODE_NONE;
    if (mode_str == "SDL_BLENDMODE_BLEND") return SDL_BLENDMODE_BLEND;
    if (mode_str == "SDL_BLENDMODE_ADD")   return SDL_BLENDMODE_ADD;
    if (mode_str == "SDL_BLENDMODE_MOD")   return SDL_BLENDMODE_MOD;
    if (mode_str == "SDL_BLENDMODE_MUL")   return SDL_BLENDMODE_MUL;
    return SDL_BLENDMODE_BLEND;
}

AssetInfo::AssetInfo(const std::string& asset_folder_name, SDL_Renderer* renderer) {
    name = asset_folder_name;
    std::string dir_path = "SRC/" + asset_folder_name;
    std::string info_path = dir_path + "/info.json";

    std::ifstream in(info_path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open asset info: " + info_path);
    }
    nlohmann::json data;
    in >> data;

    blendmode = parse_blend_mode(data.value("blend_mode", "SDL_BLENDMODE_BLEND"));

    tags.clear();
    if (data.contains("tags") && data["tags"].is_array()) {
        for (const auto& tag : data["tags"]) {
            if (tag.is_string()) tags.push_back(tag.get<std::string>());
        }
    }

    load_base_properties(data);

    if (data.contains("animations")) {
        const auto& anims = data["animations"];
        interaction = anims.contains("interaction") && !anims["interaction"].is_null();
        hit         = anims.contains("hit")        && !anims["hit"].is_null();
        collision   = anims.contains("collision")  && !anims["collision"].is_null();
    }

    const auto& ss = data.value("size_settings", nlohmann::json::object());
    scale_percentage       = ss.value("scale_percentage", 100.0f);
    variability_percentage = ss.value("variability_percentage", 0.0f);
    scale_factor = scale_percentage / 100.0f;

    original_canvas_width = 0;
    original_canvas_height = 0;
    int scaled_sprite_w = 0;
    int scaled_sprite_h = 0;
    SDL_Texture* base_sprite = nullptr;

    if (data.contains("animations")) {
        load_animations(data["animations"], dir_path, renderer, base_sprite, scaled_sprite_w, scaled_sprite_h);
    }

    if (!animations.count("default") || animations["default"].frames.empty()) {
        std::cerr << "[AssetInfo] WARNING: no valid 'default' animation for '" << name << "'\n";
    }

    load_lighting_info(data);
    load_shading_info(data);

    int scaled_canvas_w = static_cast<int>(original_canvas_width * scale_factor);
    int scaled_canvas_h = static_cast<int>(original_canvas_height * scale_factor);
    int offset_x = (scaled_canvas_w - scaled_sprite_w) / 2;
    int offset_y = (scaled_canvas_h - scaled_sprite_h);

    load_collision_areas(data, dir_path, offset_x, offset_y);
    load_child_assets(data, scale_factor, offset_x, offset_y);
}

AssetInfo::~AssetInfo() {
    std::ostringstream oss;
    oss << "[AssetInfo] Destructor for '" << name << "'\r";
    std::cout << std::left << std::setw(60) << oss.str() << std::flush;


    for (auto& [key, anim] : animations) {
        for (SDL_Texture* tex : anim.frames) {
            if (tex) SDL_DestroyTexture(tex);
        }
        anim.frames.clear();
    }
    animations.clear();
    child_assets.clear();
}


void AssetInfo::load_base_properties(const nlohmann::json& data) {
    type = data.value("asset_type", "Object");
    if (type == "Player") {
        std::cout << "[AssetInfo] Player asset '" << name << "' loaded\n\n";
    }

    z_threshold = data.value("z_threshold", 0);
    passable = has_tag("passable");

    min_same_type_distance = data.value("min_same_type_distance", 0);
    can_invert = data.value("can_invert", true);
    max_child_depth = data.value("max_child_depth", 0);
    min_child_depth = data.value("min_child_depth", 0);
    duplicatable = data.value("duplicatable", false);
    duplication_interval_min = data.value("duplication_interval_min", 0);
    duplication_interval_max = data.value("duplication_interval_max", 0);

    std::mt19937 rng{ std::random_device{}() };
    if (min_child_depth <= max_child_depth) {
        child_depth = std::uniform_int_distribution<int>(min_child_depth, max_child_depth)(rng);
    } else {
        child_depth = 0;
    }




    if (duplication_interval_min <= duplication_interval_max && duplicatable) {
        duplication_interval = std::uniform_int_distribution<int>(duplication_interval_min, duplication_interval_max)(rng);
    } else {
        duplication_interval = 0;
    }
}



void AssetInfo::load_lighting_info(const nlohmann::json& data) {
    lights.clear();
    has_light_source = false;

    if (!data.contains("lighting_info"))
        return;

    const auto& linfo = data["lighting_info"];

    // Handle backward-compatible single light object
    if (linfo.is_object()) {
        if (!linfo.value("has_light_source", false)) return;
        has_light_source = true;

        LightSource light;
        light.intensity   = linfo.value("light_intensity", 0);
        light.radius      = linfo.value("radius", 100);
        light.fall_off    = linfo.value("fall_off", 0);
        light.jitter_min  = linfo.value("jitter_min", 0);
        light.jitter_max  = linfo.value("jitter_max", 0);
        light.flicker     = linfo.value("flicker", false);
        light.offset_x    = linfo.value("offset_x", 0);
        light.offset_y    = linfo.value("offset_y", 0);

        // Load light color
        light.color = {0, 0, 0, 255};
        if (linfo.contains("light_color") && linfo["light_color"].is_array() &&
            linfo["light_color"].size() == 3 &&
            linfo["light_color"][0].is_number_integer() &&
            linfo["light_color"][1].is_number_integer() &&
            linfo["light_color"][2].is_number_integer()) {
            light.color.r = linfo["light_color"][0].get<int>();
            light.color.g = linfo["light_color"][1].get<int>();
            light.color.b = linfo["light_color"][2].get<int>();
        }

        lights.push_back(light);
    }

    // Handle list of multiple lights
    else if (linfo.is_array()) {
        for (const auto& l : linfo) {
            if (!l.is_object() || !l.value("has_light_source", false))
                continue;

            has_light_source = true;

            LightSource light;
            light.intensity   = l.value("light_intensity", 0);
            light.radius      = l.value("radius", 100);
            light.fall_off    = l.value("fall_off", 0);
            light.jitter_min  = l.value("jitter_min", 0);
            light.jitter_max  = l.value("jitter_max", 0);
            light.flicker     = l.value("flicker", false);
            light.offset_x    = l.value("offset_x", 0);
            light.offset_y    = l.value("offset_y", 0);

            light.color = {0, 0, 0, 255};
            if (l.contains("light_color") && l["light_color"].is_array() &&
                l["light_color"].size() == 3 &&
                l["light_color"][0].is_number_integer() &&
                l["light_color"][1].is_number_integer() &&
                l["light_color"][2].is_number_integer()) {
                light.color.r = l["light_color"][0].get<int>();
                light.color.g = l["light_color"][1].get<int>();
                light.color.b = l["light_color"][2].get<int>();
            }

            lights.push_back(light);
        }
    }
}


void AssetInfo::load_shading_info(const nlohmann::json& data) {
    if (!data.contains("shading_info") || !data["shading_info"].is_object()) return;
    const auto& s = data["shading_info"];

    if (s.contains("has_shading") && s["has_shading"].is_boolean())
        has_shading = s["has_shading"].get<bool>();
    else
        has_shading = false;

    if (s.contains("has_base_shadow") && s["has_base_shadow"].is_boolean())
        has_base_shadow = s["has_base_shadow"].get<bool>();
    else
        has_base_shadow = false;

    if (s.contains("base_shadow_intensity") && s["base_shadow_intensity"].is_number())
        base_shadow_intensity = s["base_shadow_intensity"].get<int>();
    else
        base_shadow_intensity = 0;

    if (s.contains("has_gradient_shadow") && s["has_gradient_shadow"].is_boolean())
        has_gradient_shadow = s["has_gradient_shadow"].get<bool>();
    else
        has_gradient_shadow = false;

    if (s.contains("number_of_gradient_shadows") && s["number_of_gradient_shadows"].is_number())
        number_of_gradient_shadows = s["number_of_gradient_shadows"].get<int>();
    else
        number_of_gradient_shadows = 0;

    if (s.contains("gradient_shadow_intensity") && s["gradient_shadow_intensity"].is_number())
        gradient_shadow_intensity = s["gradient_shadow_intensity"].get<int>();
    else
        gradient_shadow_intensity = 0;

    if (s.contains("has_casted_shadows") && s["has_casted_shadows"].is_boolean())
        has_casted_shadows = s["has_casted_shadows"].get<bool>();
    else
        has_casted_shadows = false;

    if (s.contains("number_of_casted_shadows") && s["number_of_casted_shadows"].is_number())
        number_of_casted_shadows = s["number_of_casted_shadows"].get<int>();
    else
        number_of_casted_shadows = 0;

    if (s.contains("cast_shadow_intensity") && s["cast_shadow_intensity"].is_number())
        cast_shadow_intensity = s["cast_shadow_intensity"].get<int>();
    else
        cast_shadow_intensity = 0;
}





void AssetInfo::load_collision_areas(const nlohmann::json& data,
                                     const std::string& dir_path,
                                     int offset_x,
                                     int offset_y) {
    try_load_area(data, "impassable_area", dir_path, passability_area, has_passability_area, scale_factor, offset_x, offset_y);
    try_load_area(data, "spacing_area", dir_path, spacing_area, has_spacing_area, scale_factor, offset_x, offset_y);
    try_load_area(data, "collision_area", dir_path, collision_area, has_collision_area, scale_factor, offset_x, offset_y);
    try_load_area(data, "interaction_area", dir_path, interaction_area, has_interaction_area, scale_factor, offset_x, offset_y);
    try_load_area(data, "hit_area", dir_path, attack_area, has_attack_area, scale_factor, offset_x, offset_y);
}

void AssetInfo::load_child_assets(const nlohmann::json& data,
                                  float scale_factor,
                                  int offset_x,
                                  int offset_y)
{
    if (!data.contains("child_assets")) return;

    int anchor_x = static_cast<int>(std::round(offset_x * scale_factor));
    int anchor_y = static_cast<int>(std::round(offset_y * scale_factor));

    for (const auto& c : data["child_assets"]) {
        if (!c.contains("point") || !c["point"].is_object()) continue;

        const auto& point = c["point"];
        int raw_x = point.value("x", 0);
        int raw_y = point.value("y", 0);
        int raw_r = point.value("radius", 0);

        int scaled_x = static_cast<int>(std::round(anchor_x + raw_x * scale_factor));
        int scaled_y = static_cast<int>(std::round(anchor_y + raw_y * scale_factor));
        int scaled_r = static_cast<int>(std::round(raw_r * scale_factor));

        Area child_area;
        child_area.generate_circle(scaled_x, scaled_y, scaled_r, 100, 99999, 99999);

        ChildAsset ca;
        ca.asset                 = c.value("asset", "");
        ca.point_x               = scaled_x;
        ca.point_y               = scaled_y;
        ca.radius                = scaled_r;
        ca.z_offset              = c.value("z_offset", 0);
        ca.min                   = c.value("min", 0);
        ca.max                   = c.value("max", 0);
        ca.terminate_with_parent = c.value("terminate_with_parent", false);
        ca.area                  = std::move(child_area);

        child_assets.push_back(std::move(ca));
    }
}

void AssetInfo::load_animations(const nlohmann::json& anims_json,
                                const std::string& dir_path,
                                SDL_Renderer* renderer,
                                SDL_Texture*& base_sprite,
                                int& scaled_sprite_w,
                                int& scaled_sprite_h)
{
    for (auto it = anims_json.begin(); it != anims_json.end(); ++it) {
        const std::string& trigger = it.key();
        const auto& anim_json = it.value();
        if (anim_json.is_null() || !anim_json.contains("frames_path")) continue;

        std::string folder = anim_json["frames_path"];
        std::string frame_path = dir_path + "/" + folder;

        Animation anim;
        anim.on_end = anim_json.value("on_end", "");
        anim.randomize = anim_json.value("randomize", false);
        anim.loop = (anim.on_end == trigger);

        std::vector<SDL_Surface*> loaded_surfaces;
        int frame_count = 0;

        for (int i = 0;; ++i) {
            std::string frame_file = frame_path + "/" + std::to_string(i) + ".png";
            if (!fs::exists(frame_file)) break;

            SDL_Surface* surface = IMG_Load(frame_file.c_str());
            if (!surface) {
                std::cerr << "[AssetInfo] Failed to load: " << frame_file << "\n";
                continue;
            }
            SDL_SetSurfaceBlendMode(surface, blendmode);

            if (frame_count == 0) {
                original_canvas_width = surface->w;
                original_canvas_height = surface->h;
            }

            int new_w = static_cast<int>(surface->w * scale_factor + 0.5f);
            int new_h = static_cast<int>(surface->h * scale_factor + 0.5f);
            if (frame_count == 0) {
                scaled_sprite_w = new_w;
                scaled_sprite_h = new_h;
            }

            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, new_w, new_h, 32, SDL_PIXELFORMAT_RGBA32);
            SDL_BlitScaled(surface, nullptr, scaled_surf, nullptr);
            SDL_FreeSurface(surface);

            loaded_surfaces.push_back(scaled_surf);
            ++frame_count;
        }

        for (SDL_Surface* surface : loaded_surfaces) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            if (!tex) {
                std::cerr << "[AssetInfo] Failed to create texture\n";
                continue;
            }
            SDL_SetTextureBlendMode(tex, blendmode);
            anim.frames.push_back(tex);
        }

        if (trigger == "default" && !anim.frames.empty()) {
            base_sprite = anim.frames[0];
        }

        {
            const std::vector<SDL_Color> grad_colors = {
                SDL_Color{0, 0, 0,   0},
                SDL_Color{0, 0, 0, 255}
            };
                // inside your setup block, replace the loop with just one gradient:
// …inside your asset-info setup, after you pick base_sprite…

            // clear out any old gradients
            anim.gradients.clear();

            // define one fade from opaque black → transparent
            static const std::vector<SDL_Color> fade_colors = {
                {  0,   0,   0, 255},  // black opaque
                {  0,   0,   0,   0}   // black transparent
            };

            // build one gradient: bottom 20% black fading up
            auto fade = std::make_unique<Gradient>(
                renderer,       // SDL_Renderer*
                anim.frames,    // the frame-textures
                fade_colors,    // our two-stop gradient
                180,            // direction: 180° = bottom → top
                0.80f,           // global opacity
                110.0f           // midpointPercent = 20%
            );
            if(has_base_shadow){
                fade->setActive(true);
            }
            anim.gradients.push_back(std::move(fade));


            }
        
        

        std::cout << "[AssetInfo] Loaded " << frame_count
                  << " frames (and gradients) for animation '" << trigger << "'\n";

        animations[trigger] = std::move(anim);
    }

}

void AssetInfo::try_load_area(const nlohmann::json& data,
                              const std::string& key,
                              const std::string& dir,
                              Area& area_ref,
                              bool& flag_ref,
                              float scale,
                              int offset_x,
                              int offset_y)
{
    if (data.contains(key) && data[key].is_string()) {
        try {
            std::string path = dir + "/" + data[key].get<std::string>();
            area_ref = Area(path, scale);
            flag_ref = true;
        } catch (const std::exception& e) {
            std::cerr << "[AssetInfo] warning: failed to load area '"
                      << key << "': " << e.what() << std::endl;
        }
    }
}

bool AssetInfo::has_tag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}
