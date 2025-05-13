#include "AssetManager.hpp"
#include "Boundary.hpp"
#include "AnimationSet.hpp"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

void AssetManager::loadJson(const std::string& infoJsonPath) {
    _infoPath = infoJsonPath;
    _assetFolder = fs::path(infoJsonPath).parent_path().string();

    // 1) read info.json
    std::ifstream in(infoJsonPath);
    if (!in.is_open()) throw std::runtime_error("Cannot open " + infoJsonPath);
    json j;  
    in >> j;

    // 2) scalars
    types       = j.at("types").get<std::vector<std::string>>();
    childOnly   = j.value("child_only", false);
    isPassable  = j.value("is_passable", false);
    zThreshold  = j.value("z_threshold", 0);
    isInteractable = j.value("is_interactable", false);
    isAttackable   = j.value("is_attackable", false);
    isCollidable   = j.value("is_collidable", false);
    assetName   = j.value("asset_name", "");
    assetType   = j.value("asset_type", "");
    centerX     = j["center"].value("x", 0);
    centerY     = j["center"].value("y", 0);

    // 3) size settings
    auto ss = j["size_settings"];
    scalePercentage       = ss.value("scale_percentage", 100.0f);
    variabilityPercentage = ss.value("variability_percentage", 0.0f);

    // 4) default / on-end animation block
    defaultAnim.onStart = j["default_animation"].value("on_start", "");
    defaultAnim.onEnd   = j["default_animation"].value("on_end", "");
    defaultAnim.loop    = j["default_animation"].value("loop", true);
    defaultAnim.audioPath = j["default_animation"].value("audio_path", "");
    defaultAnim.volume    = j["default_animation"].value("volume", 0);

    // 5) build an AnimationSet (your own wrapper around multiple named animations)
    animations = std::make_unique<AnimationSet>(j, _assetFolder);

    // 6) scan for *any* JSON file in this folder (except info.json) → boundaries
    boundaries.clear();
    for (auto& ent : fs::directory_iterator(_assetFolder)) {
        auto p = ent.path();
        if (p.extension() == ".json" && p.filename() != "info.json") {
            std::string type = p.stem().string();  // e.g. "collision_area"
            boundaries.emplace_back(std::make_unique<Boundary>(p.string(), type));
        }
    }


    // 8) child assets
    childAssets.clear();
    for (auto& c : j["child_assets"]) {
        ChildAsset ca;
        ca.assetName = c.value("asset", "");
        ca.areaFile  = fs::path(c.value("area_file","")).string();
        ca.zOffset   = c.value("z_offset", 0);
        ca.minCount  = c.value("min", 1);
        ca.maxCount  = c.value("max", 1);
        ca.skew      = c.value("skew", 0.0f);
        ca.terminateWithParent = c.value("terminate_with_parent", false);
        childAssets.push_back(std::move(ca));
    }

    // 9) fixed children
    fixedChildren.clear();
    for (auto& f : j["fixed_children"]) {
        FixedChild fc;
        fc.assetName = f.value("asset",""); 
        fc.zOffset   = f.value("z_offset",0);
        fc.offsetX   = f.value("offset_x",0);
        fc.offsetY   = f.value("offset_y",0);
        fixedChildren.push_back(std::move(fc));
    }

    // 10) spacing area
    spacingAreaFile = j.value("spacing_area","");

    // done!  You can now call animations->setCurrent(defaultAnim.onStart)
    animations->setCurrent(defaultAnim.onStart);
}
