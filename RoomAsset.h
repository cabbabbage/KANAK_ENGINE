// RoomAsset.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "Image.hpp"        
#include "ExistingAsset.hpp"
#include "MaskUtils.hpp"   

struct Interaction {
    std::string trigger;
    std::string audioPath;
    std::string framesPath;
    std::string postFramesPath;
};

class RoomAsset {
public:
    RoomAsset(
        const std::string&                                         code,
        const nlohmann::json&                                      cfg,
        const std::vector<Image>&                                  defaultFrames,
        const std::unordered_map<std::string, std::vector<Image>>& interactionFrames,
        const std::vector<ExistingAsset>&                          existingAssets,
        int                                                        roomLength,
        int                                                        roomHeight
    );

    // Advance frame in the active sequence
    RoomAsset& operator++();   
    RoomAsset  operator++(int);

    // Switch to a named interaction sequence
    void setActiveList(const std::string& trigger);

    // Once you’ve placed in room‐space, map it to screen‐space:
    void setScreenPosition(double x, double y);

    // Accessors
    double            getRoomPosX()    const { return roomPosX; }
    double            getRoomPosY()    const { return roomPosY; }
    double            getScreenPosX()  const { return screenPosX; }
    double            getScreenPosY()  const { return screenPosY; }
    const Image&      getCurrentImage()const;

private:
    // JSON‐loaded
    std::string                                   assetCode;
    bool                                          hasBoundary;
    double                                        volume;
    double                                        boundaryThreshold;
    int                                           peerSpacing;
    int                                           globalSpacing;
    int                                           zIndex;
    bool                                          isInteractable;
    int                                           interactionRange;
    std::vector<Interaction>                      interactions;
    int                                           renderQualityPercent;
    int                                           renderPriority;
    bool                                          positionedOffWall;
    std::string                                   wallDirection;
    int                                           wallOffset;

    // Frame sequences (injected)
    const std::vector<Image>&                     defaultFrames;
    const std::unordered_map<std::string, std::vector<Image>>& interactionFrames;
    const std::vector<Image>*                     activeFrames;
    size_t                                        currentFrameIndex;

    // Position
    int                                           roomLength, roomHeight;
    double                                        roomPosX, roomPosY;     // fixed by placeAtValidPosition
    double                                        screenPosX, screenPosY; // set later

    // Mask/contours
    AssetMask                                     assetMask;
    std::unordered_map<std::string, std::vector<cv::Point>> interactionContours;
    std::vector<cv::Point>                        collisionContour;

    // Helpers
    void placeAtValidPosition(const std::vector<ExistingAsset>& existingAssets);
};
