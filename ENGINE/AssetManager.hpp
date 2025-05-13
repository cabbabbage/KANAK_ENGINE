// AssetManager.hpp

#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

#include "Boundary.hpp"
#include "Animation.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class AssetManager {
public:
  // ctor loads everything from info.json
  AssetManager(const std::string& infoJsonPath);

  // top-level flags
  bool                  isPassable()    const { return _isPassable; }
  bool                  isCollidable()  const { return _isCollidable; }
  bool                  isInteractable()const { return _isInteractable; }
  bool                  isAttackable()  const { return _isAttackable; }

  const std::string&    name()          const { return _assetName; }
  const std::string&    type()          const { return _assetType; }

  // Boundaries, if present:
  const std::vector<std::unique_ptr<Boundary>>&
                        boundaries()    const { return _boundaries; }

  // All animations keyed by their name
  const std::map<std::string, Animation>&
                        animations()    const { return _animations; }

private:
  void                  loadJson(const std::string& path);

  std::string           _infoJsonPath;
  std::string           _assetFolder;

  // from JSON…
  bool                  _isPassable    = false;
  bool                  _isCollidable  = false;
  bool                  _isInteractable= false;
  bool                  _isAttackable  = false;
  std::string           _assetName;
  std::string           _assetType;

  std::vector<std::unique_ptr<Boundary>>  _boundaries;
  std::map<std::string, Animation>        _animations;
};
