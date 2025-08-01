// RenderUtils.cpp
#include "render_utils.hpp"
#include "generate_map_light.hpp"
#include "Asset.hpp"
#include <cmath>
#include <algorithm>

RenderUtils::RenderUtils(SDL_Renderer* renderer,
                         int screenWidth,
                         int screenHeight,
                         SDL_Texture* minimapTexture,
                         const std::string& map_path)
    : renderer_(renderer),
      screenWidth_(screenWidth),
      screenHeight_(screenHeight),
      halfWidth_(screenWidth * 0.5f),
      halfHeight_(screenHeight * 0.5f),
      shakeIntensity_(0.5f),
      shakeSpeed_(0.05f),
      shakeTimer_(0.0f),
      lastPx_(0),
      lastPy_(0),
      center_{static_cast<int>(halfWidth_), static_cast<int>(halfHeight_)},
      lightScaleFactor_(0.25f),
      lightRotationFactor_(15.0f),
      lightSpeed_(0.002f),
      map_light_(nullptr),
      minimapTexture_(minimapTexture),
      map_path_(map_path)
{
    trapSettings_ = {false, 0, 0, 0, 0, 1.0f, 1.0f, {255,255,255,255}};
}

Generate_Map_Light* RenderUtils::createMapLight() {
    SDL_Color fallback = {255, 255, 255, 255};
    map_light_ = new Generate_Map_Light(renderer_,
                                        screenWidth_ / 2,
                                        screenHeight_ / 2,
                                        screenWidth_,
                                        fallback,
                                        map_path_);
    return map_light_;
}

void RenderUtils::updateCameraShake(int px, int py) {
    const float MIN_SHAKE_INTENSITY = 0.0f;
    const float MAX_SHAKE_INTENSITY = 0.0f;
    const float MIN_SHAKE_SPEED = 0.0f;
    const float MAX_SHAKE_SPEED = 0.0f;

    if (px != lastPx_ || py != lastPy_) {
        shakeIntensity_ = std::max(MIN_SHAKE_INTENSITY, shakeIntensity_ * 0.97f);
        shakeSpeed_     = std::max(MIN_SHAKE_SPEED,     shakeSpeed_     * 0.9f);
    } else {
        shakeIntensity_ = std::min(MAX_SHAKE_INTENSITY, shakeIntensity_ * 1.03f);
        shakeSpeed_     = std::min(MAX_SHAKE_SPEED,     shakeSpeed_     * 1.05f);
    }

    lastPx_ = px;
    lastPy_ = py;

    shakeTimer_ += shakeSpeed_;
    float raw_x = std::sin(shakeTimer_ * 0.7f) * shakeIntensity_;
    float raw_y = std::sin(shakeTimer_ * 1.05f + 2.0f) * shakeIntensity_;

    int shakeX = static_cast<int>(std::clamp(raw_x, -1.0f, 1.0f));
    int shakeY = static_cast<int>(std::clamp(raw_y, -1.0f, 1.0f));

    center_.x = static_cast<int>(halfWidth_) + shakeX;
    center_.y = static_cast<int>(halfHeight_) + shakeY;
}

SDL_Point RenderUtils::applyParallax(int ax, int ay) const {
    float world_dx = ax - lastPx_;
    float world_dy = ay - lastPy_;
    float ndx = world_dx / halfWidth_;
    float ndy = world_dy / halfHeight_;
    float offX = ndx * parallaxMaxX_;
    float offY = ndy * parallaxMaxY_;

    int screen_x = static_cast<int>(world_dx + center_.x + offX);
    int screen_y = static_cast<int>(world_dy + center_.y + offY);
    return {screen_x, screen_y};
}

void RenderUtils::setLightDistortionRect(const SDL_Rect& rect) {
    lightRect_ = rect;
}

void RenderUtils::setLightDistortionParams(float scaleFactor,
                                           float rotationFactor,
                                           float speedMultiplier) {
    lightScaleFactor_    = scaleFactor;
    lightRotationFactor_ = rotationFactor;
    lightSpeed_          = speedMultiplier;
}

void RenderUtils::renderLightDistorted(SDL_Texture* tex) const {
    if (!tex) return;

    int screenW = screenWidth_;
    int screenH = screenHeight_;
    float norm_x = std::abs(lightRect_.x + lightRect_.w/2 - screenW/2) / halfWidth_;
    float norm_y = std::abs(lightRect_.y + lightRect_.h/2 - screenH/2) / halfHeight_;
    float edge = std::max(norm_x, norm_y);
    float scale = 1.0f + edge * lightScaleFactor_;
    float rot   = std::sin(SDL_GetTicks() * lightSpeed_) * edge * lightRotationFactor_;

    SDL_Rect scaled = lightRect_;
    scaled.w = static_cast<int>(lightRect_.w * scale);
    scaled.h = static_cast<int>(lightRect_.h * scale);
    scaled.x = lightRect_.x - (scaled.w - lightRect_.w)/2;
    scaled.y = lightRect_.y - (scaled.h - lightRect_.h)/2;

    SDL_RenderCopyEx(renderer_, tex, nullptr, &scaled, rot, nullptr, SDL_FLIP_NONE);
}

void RenderUtils::setAssetTrapezoid(const Asset* asset, int playerX, int playerY) {
    trapSettings_.enabled = false;
    if (!asset) return;

    SDL_Texture* tex = asset->get_current_frame();
    if (!tex) return;

    trapSettings_.enabled = true;
    SDL_QueryTexture(tex, nullptr, nullptr, &trapSettings_.w, &trapSettings_.h);

    SDL_Point p = applyParallax(asset->pos_X, asset->pos_Y);
    trapSettings_.screen_x = p.x;
    trapSettings_.screen_y = p.y;

    struct EdgeScales { float L,R,T,B; };
    // Original values (commented out):
    // const EdgeScales top{0.96f, 0.96f, 0.96f, 0.91f},
    //                  mid{0.93f, 0.93f, 0.93f, 0.91f},
    //                  bot{0.88f, 0.88f, 0.88f, 0.91f};

    // Uniform values for testing (all set to 1.0f):
    const EdgeScales top{1.0f, 1.0f, 1.0f, 1.0f},
                    mid{1.0f, 1.0f, 1.0f, 1.0f},
                    bot{1.0f, 1.0f, 1.0f, 1.0f};


    auto lerp = [](float a,float b,float t){return a+(b-a)*t;};
    auto lerpE = [&](auto A, auto B, float t){
        return EdgeScales{
            lerp(A.L,B.L,t), lerp(A.R,B.R,t),
            lerp(A.T,B.T,t), lerp(A.B,B.B,t)
        };
    };
    auto ease = [](float t){return t*t*(3.0f-2.0f*t);};

    float dy = (asset->pos_Y - playerY)/1000.0f;
    float dx = (asset->pos_X - playerX)/1000.0f;
    float ny = std::clamp(dy,-1.0f,1.0f);
    float nx = std::clamp(dx,-1.0f,1.0f);

    EdgeScales L,I;
    if(ny<0){float e=ease(ny+1); L=lerpE(top, mid,e); I=lerpE(top,mid,e);} 
    else     {float e=ease(ny);   L=lerpE(mid, bot,e); I=lerpE(mid,bot,e);}  
    float tx=(nx+1)*0.5f, ex=ease(tx);
    EdgeScales S=lerpE(L,I,ex);
    if(nx>0) std::swap(S.L,S.R);

    trapSettings_.topScaleX = (S.L+S.R)*0.5f;
    trapSettings_.topScaleY = (S.T+S.B)*0.5f;

    float c = std::pow(asset->gradient_opacity,1.2f);
    int d = static_cast<int>(255*c);
    if(asset->info->type=="Player") d=std::min(255,d*3);
    trapSettings_.color={Uint8(d),Uint8(d),Uint8(d),255};
}

void RenderUtils::renderAssetTrapezoid(SDL_Texture* tex) const {
    if(!trapSettings_.enabled||!tex) return;
    SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
    SDL_Vertex V[4];
    int hb=trapSettings_.w/2;
    int tW=int(trapSettings_.w*trapSettings_.topScaleX);
    int tH=int(trapSettings_.h*trapSettings_.topScaleY);
    int ht=tW/2;
    int by=trapSettings_.screen_y;
    int ty=by-tH;
    V[0].position={float(trapSettings_.screen_x-ht),float(ty)}; V[0].tex_coord={0,0};
    V[1].position={float(trapSettings_.screen_x+ht),float(ty)}; V[1].tex_coord={1,0};
    V[2].position={float(trapSettings_.screen_x+hb),float(by)};  V[2].tex_coord={1,1};
    V[3].position={float(trapSettings_.screen_x-hb),float(by)};  V[3].tex_coord={0,1};
    for(int i=0;i<4;++i) V[i].color=trapSettings_.color;
    int idx[]={0,1,2,2,3,0};
    SDL_RenderGeometry(renderer_,tex,V,4,idx,6);
}


// === File: RenderUtils.hpp (additions needed) ===
// Add inside the class declaration:
// SDL_FPoint trapezoid_[4];
// void renderTrapezoid(SDL_Texture* tex, const SDL_FPoint quad[4]);

// === File: RenderUtils.cpp (add to end of file) ===

// === File: render_utils.cpp ===
RenderUtils::TrapezoidGeometry RenderUtils::getTrapezoidGeometry(SDL_Texture* tex, const SDL_FPoint quad[4]) const {
    TrapezoidGeometry geom;

    geom.vertices[0].position = quad[0];
    geom.vertices[1].position = quad[1];
    geom.vertices[2].position = quad[2];
    geom.vertices[3].position = quad[3];

    geom.vertices[0].tex_coord = {0, 0};
    geom.vertices[1].tex_coord = {1, 0};
    geom.vertices[2].tex_coord = {1, 1};
    geom.vertices[3].tex_coord = {0, 1};

    for (int i = 0; i < 4; ++i)
        geom.vertices[i].color = SDL_Color{255, 255, 255, 255};

    geom.indices[0] = 0;
    geom.indices[1] = 1;
    geom.indices[2] = 2;
    geom.indices[3] = 2;
    geom.indices[4] = 3;
    geom.indices[5] = 0;

    return geom;
}



void RenderUtils::renderMinimap() const {
    if (!minimapTexture_) return;
    int mw, mh;
    SDL_QueryTexture(minimapTexture_, nullptr, nullptr, &mw, &mh);
    int w = mw * 2;
    int h = mh * 2;
    SDL_Rect d = { screenWidth_ - w - 10, screenHeight_ - h - 10, w, h };
    SDL_SetTextureBlendMode(minimapTexture_, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer_, minimapTexture_, nullptr, &d);
}

Generate_Map_Light* RenderUtils::getMapLight() const {
    return map_light_;
}
