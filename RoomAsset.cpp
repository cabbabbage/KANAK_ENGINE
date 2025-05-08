// RoomAsset.cpp
#include "RoomAsset.hpp"
#include <random>
#include <cmath>

RoomAsset::RoomAsset(
    const std::string&                                         code,
    const nlohmann::json&                                      cfg,
    const std::vector<Image>&                                  defaultFrames_,
    const std::unordered_map<std::string, std::vector<Image>>& interactionFrames_,
    const std::vector<ExistingAsset>&                          existingAssets,
    int                                                        roomLength_,
    int                                                        roomHeight_
)
  : assetCode          (code)
  , hasBoundary        (cfg.at("hasBoundary").get<bool>())
  , volume             (cfg.at("volume").get<double>())
  , boundaryThreshold  (cfg.at("boundaryThreshold").get<double>())
  , peerSpacing        (cfg.at("peerSpacing").get<int>())
  , globalSpacing      (cfg.at("globalSpacing").get<int>())
  , zIndex             (cfg.at("zIndex").get<int>())
  , isInteractable     (cfg.at("isInteractable").get<bool>())
  , interactionRange   (cfg.at("interaction_range").get<int>())
  , renderQualityPercent(cfg.at("renderQualityPercent").get<int>())
  , renderPriority     (cfg.at("renderPriority").get<int>())
  , positionedOffWall  (cfg.at("positionedOffWall").get<bool>())
  , wallDirection      (cfg.at("wallDirection").get<std::string>())
  , wallOffset         (cfg.at("wallOffset").get<int>())
  , defaultFrames      (defaultFrames_)
  , interactionFrames  (interactionFrames_)
  , activeFrames       (&defaultFrames)
  , currentFrameIndex  (0)
  , roomLength         (roomLength_)
  , roomHeight         (roomHeight_)
  , roomPosX           (0)
  , roomPosY           (0)
  , screenPosX         (0)
  , screenPosY         (0)
{
    // Load Interaction structs
    for (auto& it : cfg.at("interactions")) {
        interactions.push_back({
            it.at("trigger").get<std::string>(),
            it.at("audioPath").get<std::string>(),
            it.at("framesPath").get<std::string>(),
            it.at("postFramesPath").get<std::string>()
        });
    }

    // 1) Place in room‐space
    placeAtValidPosition(existingAssets);

    // 2) Build mask & contours from the defaultFrames' paths
    std::vector<std::string> paths;
    paths.reserve(defaultFrames.size());
    for (auto &img : defaultFrames) paths.push_back(img.path);

    assetMask = computeAssetMask(paths);

    // Interaction areas
    for (auto &inter : interactions) {
        interactionContours[inter.trigger] =
            computeInteractionArea(assetMask, paths, interactionRange);
    }

    // Collision boundary
    collisionContour =
        computeCollisionBoundary(assetMask, paths, boundaryThreshold);
}

RoomAsset& RoomAsset::operator++() {
    if (activeFrames && !activeFrames->empty()) {
        currentFrameIndex = (currentFrameIndex + 1) % activeFrames->size();
    }
    return *this;
}

RoomAsset RoomAsset::operator++(int) {
    RoomAsset tmp = *this;
    ++(*this);
    return tmp;
}

void RoomAsset::setActiveList(const std::string& trigger) {
    auto it = interactionFrames.find(trigger);
    if (it != interactionFrames.end()) {
        activeFrames = &it->second;
        currentFrameIndex = 0;
    }
}

void RoomAsset::setScreenPosition(double x, double y) {
    screenPosX = x;
    screenPosY = y;
}

const Image& RoomAsset::getCurrentImage() const {
    return (*activeFrames)[currentFrameIndex];
}

void RoomAsset::placeAtValidPosition(const std::vector<ExistingAsset>& existingAssets) {
    std::mt19937           rng(std::random_device{}());
    std::uniform_int_distribution<int> distX(0, roomLength);
    std::uniform_int_distribution<int> distY(0, roomHeight);

    while (true) {
        double x = distX(rng);
        double y = distY(rng);
        bool ok = true;
        for (auto& e : existingAssets) {
            double dx = x - e.x, dy = y - e.y;
            double d  = std::sqrt(dx*dx + dy*dy);
            if ((e.code == assetCode && d < peerSpacing) || (d < globalSpacing)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            roomPosX = x;
            roomPosY = y;
            screenPosX = x;
            screenPosY = y;
            return;
        }
    }
}
