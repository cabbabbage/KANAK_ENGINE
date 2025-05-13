#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <SDL.h>
#include <nlohmann/json.hpp>

class AnimationSet {
public:
    /// @param renderer    your SDL_Renderer*
    /// @param assetDir    directory containing all frame‐folders and info.json
    AnimationPlayer(SDL_Renderer* renderer, const std::string& assetDir);

    /// load everything from info.json (throws on failure)
    void load();

    /// switch to a new animation by name (must exist in info.json)
    void updateAnimation(const std::string& name);

    /// advance one frame in both base & overlay
    void tick();

    /// draw at world‐coords (x,y)
    void render(int x, int y);

    /// has the current base animation finished?
    bool isFinished() const;

private:
    struct Anim {
        std::vector<SDL_Texture*> frames;
        bool loop = false;
        std::string onEnd;
    };

    SDL_Renderer*            renderer_;
    std::string              baseDir_;
    std::unordered_map<std::string,Anim> anims_;
    std::vector<SDL_Texture*>           overlayFrames_;

    std::string currentAnim_;
    size_t      currentFrame_   = 0;
    size_t      overlayFrame_   = 0;
    bool        finished_       = false;

    // overlay placement
    int  overlayOffsetX = 0, overlayOffsetY = 0;
    double overlayScale = 1.0;
    int  overlayAlpha  = 255;

    // helpers
    SDL_Texture* loadTexture(const std::string& path);
    std::vector<SDL_Texture*> loadTexturesFromFolder(const std::string& folder);
};
