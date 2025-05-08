#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "RoomAsset.hpp"

// Placeholder Image type; replace with your engine's image/texture class
struct Image {
    std::string path;
    // Actual pixel data or texture handle goes here
    Image(const std::string& p) : path(p) {}
};

// Loads an image from disk; implement using your chosen library
inline Image loadImage(const std::string& filePath) {
    return Image(filePath);
}

// Descriptor for each asset type
struct AssetDescriptor {
    std::string assetPath;
    int minCount;
    int maxCount;
    int renderPriority;
};

class RoomGenerator {
public:
    RoomGenerator(const std::string& roomJsonPath) {
        loadRoomJson(roomJsonPath);
        randomizeDimensions();
        sortDescriptorsByPriority();
        preloadAllImages();
        instantiateAssets();
    }

    int getRoomLength() const { return roomLength; }
    int getRoomHeight() const { return roomHeight; }
    const std::vector<RoomAsset>& getAssets() const { return assets; }

private:
    int roomLength;
    int roomHeight;
    nlohmann::json roomJson;
    std::vector<AssetDescriptor> descriptors;
    std::mt19937 rng{ std::random_device{}() };

    // Caches of loaded images (not just paths)
    std::unordered_map<std::string, std::vector<Image>> defaultImageCache;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::vector<Image>>
    > interactionImageCache;

    // Track existing assets for spacing
    std::vector<ExistingAsset> existingInfos;
    std::vector<RoomAsset> assets;

    void loadRoomJson(const std::string& path) {
        std::ifstream in(path);
        in >> roomJson;
        for (auto& it : roomJson.at("assetList")) {
            descriptors.push_back({
                it.at("assetPath").get<std::string>(),
                it.at("minCount").get<int>(),
                it.at("maxCount").get<int>(),
                it.at("renderPriority").get<int>()
            });
        }
    }

    void randomizeDimensions() {
        int hMin = roomJson.at("height").at("min").get<int>();
        int hMax = roomJson.at("height").at("max").get<int>();
        int lMin = roomJson.at("length").at("min").get<int>();
        int lMax = roomJson.at("length").at("max").get<int>();
        std::uniform_int_distribution<int> distH(hMin, hMax);
        std::uniform_int_distribution<int> distL(lMin, lMax);
        roomHeight = distH(rng);
        roomLength = distL(rng);
    }

    void sortDescriptorsByPriority() {
        std::sort(descriptors.begin(), descriptors.end(),
            [](auto& a, auto& b) {
                return a.renderPriority < b.renderPriority; // 0 = highest
            }
        );
    }

    // Preload default frames + each interaction's frames for each asset type
    void preloadAllImages() {
        for (auto& desc : descriptors) {
            // Load asset JSON config
            nlohmann::json assetJson;
            std::ifstream in(desc.assetPath);
            in >> assetJson;

            // Default frames
            std::string defaultPath = assetJson.at("defaultFramesPath");
            defaultImageCache[desc.assetPath] = loadImagesFromFolder(defaultPath);

            // Interaction frames per trigger
            for (auto& it : assetJson.at("interactions")) {
                std::string trigger = it.at("trigger").get<std::string>();
                std::string framesPath = it.at("framesPath").get<std::string>();
                interactionImageCache[desc.assetPath][trigger] =
                    loadImagesFromFolder(framesPath);
            }
        }
    }

    std::vector<Image> loadImagesFromFolder(const std::string& folder) {
        std::vector<Image> images;
        for (auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".png") {
                images.push_back(loadImage(entry.path().string()));
            }
        }
        // Sort by filename to maintain correct order
        std::sort(images.begin(), images.end(),
            [](auto& a, auto& b){ return a.path < b.path; }
        );
        return images;
    }

    void instantiateAssets() {
        for (auto& desc : descriptors) {
            std::uniform_int_distribution<int> countDist(desc.minCount, desc.maxCount);
            int count = countDist(rng);

            // Reload asset JSON once per type for Asset constructor
            nlohmann::json assetJson;
            std::ifstream in(desc.assetPath);
            in >> assetJson;
            std::string code = assetJson.at("assetCode").get<std::string>();

            for (int i = 0; i < count; ++i) {
                RoomAsset asset(
                    code,
                    assetJson,
                    defaultImageCache[desc.assetPath],
                    interactionImageCache[desc.assetPath],
                    existingInfos,
                    roomLength,
                    roomHeight
                );

                existingInfos.push_back({ code, asset.getDisplayX(), asset.getDisplayY() });
                assets.push_back(std::move(asset));
            }
        }
    }
};
