// gradient.hpp
#pragma once
#include <vector>
#include <SDL.h>

class Gradient {
public:
    Gradient(SDL_Renderer* renderer,
             const std::vector<SDL_Texture*>& frames,
             const std::vector<SDL_Color>& colors,
             int direction,
             float opacity,
             float midpointPercent);
    ~Gradient();

    void updateParameters(const std::vector<SDL_Color>& colors,
                          int direction,
                          float opacity,
                          float midpointPercent);

    SDL_Texture* getGradient(size_t index) const;
    bool active_ = false;
    void setActive(bool value);

private:
    SDL_Renderer* renderer_;
    std::vector<SDL_Texture*> frames_;
    std::vector<SDL_Color> colors_;
    int direction_;
    float opacity_;
    float midpointPercent_;
    mutable std::vector<SDL_Texture*> cache_;
    mutable std::vector<int> cacheVersion_;
    std::vector<SDL_Texture*> maskTargets_;
    std::vector<SDL_Surface*> masks_;

    mutable int version_ = 0;

    SDL_Surface* buildGradientSurface(const std::vector<SDL_Color>& colors,
                                      float opacity,
                                      float midpointPercent,
                                      int direction,
                                      SDL_Surface* mask) const;
};
