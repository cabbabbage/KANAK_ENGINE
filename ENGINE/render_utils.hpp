// RenderUtils.hpp
#ifndef RENDER_UTILS_HPP
#define RENDER_UTILS_HPP

#include <SDL.h>
#include "generate_map_light.hpp"

class Asset;  // forward declaration

class RenderUtils {
public:
    // ctor: renderer, viewport size, and minimap texture
    RenderUtils(SDL_Renderer* renderer,
                int screenWidth,
                int screenHeight,
                SDL_Texture* minimapTexture);

    // camera shake
    void updateCameraShake(int px, int py);

    // parallax world→screen
    SDL_Point applyParallax(int ax, int ay) const;

    // light distortion
    void setLightDistortionRect(const SDL_Rect& rect);
    void setLightDistortionParams(float scaleFactor,
                                  float rotationFactor,
                                  float speedMultiplier);
    void renderLightDistorted(SDL_Texture* tex) const;

    // trapezoid‐projected asset
    // moves all previous inline math into utils
    void setAssetTrapezoid(const Asset* asset,
                           int playerX,
                           int playerY);
    void renderAssetTrapezoid(SDL_Texture* tex) const;

    // map‐wide light generator
    Generate_Map_Light* createMapLight();

    // draw minimap in bottom‐right
    void renderMinimap() const;

    Generate_Map_Light* getMapLight() const;

private:
    SDL_Renderer* renderer_;
    int           screenWidth_;
    int           screenHeight_;
    float         halfWidth_;
    float         halfHeight_;

    // camera shake state
    float   shakeIntensity_;
    float   shakeSpeed_;
    float   shakeTimer_;
    int     lastPx_;
    int     lastPy_;
    SDL_Point center_;

    // parallax max offsets
    static constexpr float parallaxMaxX_ = 40.0f;
    static constexpr float parallaxMaxY_ = 20.0f;

    // light distortion
    SDL_Rect lightRect_;
    float    lightScaleFactor_;
    float    lightRotationFactor_;
    float    lightSpeed_;

    // trapezoid settings
    struct TrapSettings {
        bool       enabled;
        int        screen_x;
        int        screen_y;
        int        w;
        int        h;
        float      topScaleX;
        float      topScaleY;
        SDL_Color  color;
    } trapSettings_;

    // stored Generate_Map_Light
    Generate_Map_Light* map_light_;

    // minimap texture
    SDL_Texture* minimapTexture_;
};

#endif // RENDER_UTILS_HPP
