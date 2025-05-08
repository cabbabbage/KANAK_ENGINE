#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <nlohmann/json.hpp>
#include "RoomAsset.hpp"

// -----------------------------------------------------------------------------
// Descriptor for each asset type in the room JSON
// -----------------------------------------------------------------------------
struct AssetDescriptor {
    std::string assetPath;
    int         minCount;
    int         maxCount;
    int         renderPriority;
};

// -----------------------------------------------------------------------------
// RoomGenerator
// -----------------------------------------------------------------------------
class RoomGenerator {
public:
    // Parses the room JSON, preloads all images, and instantiates assets
    explicit RoomGenerator(const std::string& roomJsonPath);

    int getRoomLength() const;
    int getRoomHeight() const;
    const std::vector<RoomAsset>& getAssets() const;

private:
    // Room dimensions
    int                     roomLength;
    int                     roomHeight;

    // Raw JSON and descriptors
    nlohmann::json          roomJson;
    std::vector<AssetDescriptor> descriptors;

    // RNG for sizing & placement
    std::mt19937            rng;

    // Image caches (loaded Image objects)
    std::unordered_map<std::string, std::vector<Image>>                                      defaultImageCache;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Image>>>     interactionImageCache;

    // Track placements & resulting assets
    std::vector<ExistingAsset> existingInfos;
    std::vector<RoomAsset>     assets;

    // Initialization steps
    void loadRoomJson(const std::string& path);
    void randomizeDimensions();
    void sortDescriptorsByPriority();
    void preloadAllImages();
    std::vector<Image> loadImagesFromFolder(const std::string& folder);
    void instantiateAssets();
};
