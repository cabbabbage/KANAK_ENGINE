// Boundary.hpp

#pragma once

#include <string>
#include <vector>
#include <utility>    // for std::pair
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// A simple 2D point
struct Point
{
    float x, y;
};

// The Boundary class
class Boundary {
public:
    // boundaryType is just a label (e.g. "collision", "interaction", etc.)
    Boundary(const std::string& jsonPath, const std::string& boundaryType);

    // Returns the local points as‐loaded
    const std::vector<Point>&  getLocalPoints() const;

    // Given an asset at world position (wx,wy), returns transformed world points
    std::vector<Point>         getWorldPoints(float wx, float wy) const;

    // Simple bounding‐box test in world space
    bool                       contains(float wx, float wy,
                                        float px, float py) const;

private:
    std::string               _type;
    std::vector<Point>        _localPoints;
    float                      _minX, _maxX, _minY, _maxY;

    void                       loadFromJson(const std::string& path);
    void                       computeBounds();
};
