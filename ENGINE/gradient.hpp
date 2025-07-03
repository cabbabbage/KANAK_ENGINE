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

    SDL_Texture* getGradient(size_t index) const;
    void setDirection(int newDirection);
    void setActive(bool value);
    bool active_ = true;
private:
    SDL_Renderer* renderer_;
    std::vector<SDL_Texture*> frames_;
    std::vector<SDL_Texture*> maskTargets_;
    std::vector<SDL_Surface*> masks_;
    std::vector<SDL_Color> colors_;
    float opacity_;
    float midpointPercent_;

    SDL_Surface* raw_gradient_surface_;
    int direction_;
    mutable int last_direction_;


    mutable std::vector<SDL_Texture*> last_images_;
    mutable std::vector<SDL_Texture*> cache_;
    mutable std::vector<int> cacheVersion_;

    SDL_Surface* buildGradientSurface(const std::vector<SDL_Color>& colors,
                                      float opacity,
                                      float midpointPercent,
                                      int direction,
                                      SDL_Surface* mask) const;
};
