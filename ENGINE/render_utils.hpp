// === File: render_utils.hpp ===
#ifndef RENDER_UTILS_HPP
#define RENDER_UTILS_HPP

#include <SDL.h>
#include <string>
#include "generate_map_light.hpp"

class Asset;

class RenderUtils {
public:
    struct TrapezoidGeometry {
        SDL_Vertex vertices[4];
        int indices[6];
    };

    RenderUtils(SDL_Renderer* renderer,
                int screenWidth,
                int screenHeight,
                SDL_Texture* minimapTexture,
                const std::string& map_path);

    void updateCameraShake(int px, int py);
    SDL_Point applyParallax(int ax, int ay) const;

    void setLightDistortionRect(const SDL_Rect& rect);
    void setLightDistortionParams(float scaleFactor,
                                  float rotationFactor,
                                  float speedMultiplier);
    void renderLightDistorted(SDL_Texture* tex) const;

    void setAssetTrapezoid(const Asset* asset, int playerX, int playerY);
    // In render_utils.hpp, change the signature to:
    void renderAssetTrapezoid(SDL_Texture* tex,
                            SDL_BlendMode blendMode,
                            bool flipped = false) const;
    RenderUtils::TrapezoidGeometry getTrapezoidGeometry(SDL_Texture* tex, const SDL_FPoint quad[4]) const;

    Generate_Map_Light* createMapLight();
    Generate_Map_Light* getMapLight() const;
    void renderMinimap() const;
    struct TrapSettings {
        bool enabled;
        int screen_x, screen_y;
        int w, h;
        float topScaleX, topScaleY;
        SDL_Color color;
    } trapSettings_;


private:
    SDL_Renderer* renderer_;
    int screenWidth_, screenHeight_;
    float halfWidth_, halfHeight_;
    SDL_Point center_;

    float shakeIntensity_;
    float shakeSpeed_;
    float shakeTimer_;
    int lastPx_, lastPy_;

    static constexpr float parallaxMaxX_ = 0.0f;
    static constexpr float parallaxMaxY_ = 0.0f;

    SDL_Rect lightRect_;
    float lightScaleFactor_;
    float lightRotationFactor_;
    float lightSpeed_;


    SDL_Texture* minimapTexture_;
    std::string map_path_;
    Generate_Map_Light* map_light_;
};

#endif // RENDER_UTILS_HPP
