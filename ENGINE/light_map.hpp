#ifndef LIGHT_MAP_HPP
#define LIGHT_MAP_HPP

#include <SDL.h>
#include <vector>

struct LightInstance {
    SDL_Texture* texture;
    SDL_Rect     dst_rect;
    Uint8        alpha;

    LightInstance(SDL_Texture* t, const SDL_Rect& d, Uint8 a)
      : texture(t), dst_rect(d), alpha(a) {}
};

class LightMap {
public:
    LightMap(SDL_Renderer* renderer, int w, int h);
    ~LightMap();

    void setBaseColor(SDL_Color color);
    void setLights(const std::vector<LightInstance>& lights);
    void update();
    SDL_Texture* getTexture() const;

private:
    SDL_Renderer*             renderer_;
    int                       width_, height_;
    SDL_Texture*              texture_;
    SDL_Color                 base_color_;
    std::vector<LightInstance> lights_;
};

#endif // LIGHT_MAP_HPP
