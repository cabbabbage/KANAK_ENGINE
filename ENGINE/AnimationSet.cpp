#include "AnimationSet.hpp"
#include <filesystem>
#include <fstream>
#include <SDL_image.h>
using json = nlohmann::json;

AnimationSet::AnimationPlayer(SDL_Renderer* renderer, const std::string& assetDir)
 : renderer_(renderer), baseDir_(assetDir)
{}

SDL_Texture* AnimationPlayer::loadTexture(const std::string& path) {
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) throw std::runtime_error("IMG_Load failed: " + std::string(IMG_GetError()));
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) throw std::runtime_error("CreateTexture failed");
    return tex;
}

std::vector<SDL_Texture*> AnimationPlayer::loadTexturesFromFolder(const std::string& folder) {
    std::vector<SDL_Texture*> out;
    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension()==".png") {
            out.push_back(loadTexture(entry.path().string()));
        }
    }
    // sort by filename
    std::sort(out.begin(), out.end(), [&](SDL_Texture* a, SDL_Texture* b){
        return a < b; // textures themselves don't sort; you probably want to sort paths before loading
    });
    return out;
}

void AnimationPlayer::load() {
    // 1) parse info.json
    std::ifstream in(baseDir_ + "/info.json");
    if (!in) throw std::runtime_error("Cannot open info.json");
    json j; in >> j;

    // 2) load each named animation
    for (auto& name : j["available_animations"]) {
        std::string animName = name.get<std::string>();
        auto& cfg = j[animName + "_animation"];
        Anim A;
        A.loop  = cfg.value("loop", false);
        A.onEnd = cfg.value("on_end", "");
        std::string framesPath = baseDir_ + "/" + cfg.value("frames_path","");
        A.frames = loadTexturesFromFolder(framesPath);
        anims_.emplace(animName, std::move(A));
    }
    // default animation (on_start)
    std::string def = j["default_animation"].value("on_start","default");
    updateAnimation(def);

    // 3) load overlay sequence (take first overlay entry)
    auto overlays = j.value("overlays", json::array());
    if (!overlays.empty()) {
        auto& ov = overlays[0];
        std::string folder = baseDir_ + "/" + ov.value("folder","");
        overlayFrames_ = loadTexturesFromFolder(folder);
        overlayOffsetX = ov.value("offset_x", 0);
        overlayOffsetY = ov.value("offset_y", 0);
        overlayScale   = ov.value("scale_pct", 100.0)/100.0;
        overlayAlpha   = int(ov.value("alpha_pct",100));
    }
}

void AnimationPlayer::updateAnimation(const std::string& name) {
    auto it = anims_.find(name);
    if (it == anims_.end()) return;
    currentAnim_   = name;
    currentFrame_  = 0;
    finished_      = false;
}

void AnimationPlayer::tick() {
    if (finished_) return;
    auto& A = anims_.at(currentAnim_);
    if (A.frames.empty()) return;
    // advance base
    currentFrame_++;
    if (currentFrame_ >= A.frames.size()) {
        if (A.loop) {
            currentFrame_ = 0;
        } else {
            finished_ = true;
            if (!A.onEnd.empty())
                updateAnimation(A.onEnd);
            return;
        }
    }
    // advance overlay
    if (!overlayFrames_.empty()) {
        overlayFrame_ = (overlayFrame_+1) % overlayFrames_.size();
    }
}

bool AnimationPlayer::isFinished() const {
    return finished_;
}

void AnimationPlayer::render(int x, int y) {
    // draw base
    auto& A = anims_.at(currentAnim_);
    SDL_Texture* baseTex = A.frames.at(currentFrame_);
    SDL_Rect dst;
    dst.x = x; dst.y = y;
    SDL_QueryTexture(baseTex, nullptr,nullptr,&dst.w,&dst.h);
    SDL_RenderCopy(renderer_, baseTex,nullptr,&dst);

    // draw overlay
    if (!overlayFrames_.empty()) {
        SDL_Texture* ovTex = overlayFrames_.at(overlayFrame_);
        SDL_Rect odv; SDL_QueryTexture(ovTex,nullptr,nullptr,&odv.w,&odv.h);
        odv.w = int(odv.w * overlayScale);
        odv.h = int(odv.h * overlayScale);
        odv.x = x + overlayOffsetX - odv.w/2;
        odv.y = y + overlayOffsetY - odv.h/2;
        SDL_SetTextureAlphaMod(ovTex, Uint8(overlayAlpha));
        SDL_RenderCopy(renderer_, ovTex, nullptr, &odv);
        SDL_SetTextureAlphaMod(ovTex, 255);
    }
}
