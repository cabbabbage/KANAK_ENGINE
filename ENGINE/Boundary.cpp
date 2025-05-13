// Boundary.cpp

#include "Boundary.hpp"

Boundary::Boundary(const std::string& jsonPath,
                   const std::string& boundaryType)
  : _type(boundaryType)
{
    loadFromJson(jsonPath);
    computeBounds();
}

void Boundary::loadFromJson(const std::string& path)
{
    std::ifstream in{path};
    if(!in)
        throw std::runtime_error("Cannot open boundary JSON: " + path);

    json j;
    in >> j;

    if(!j.is_array())
        throw std::runtime_error("Boundary JSON must be an array of [x,y] pairs");

    _localPoints.clear();
    for(auto& elem : j)
    {
        if(elem.is_array() && elem.size()==2)
        {
            _localPoints.push_back({ elem[0].get<float>(),
                                     elem[1].get<float>() });
        }
    }
}

void Boundary::computeBounds()
{
    if(_localPoints.empty()) {
        _minX = _maxX = _minY = _maxY = 0;
        return;
    }
    _minX = _maxX = _localPoints[0].x;
    _minY = _maxY = _localPoints[0].y;
    for(auto& p : _localPoints) {
        if(p.x < _minX) _minX = p.x;
        if(p.x > _maxX) _maxX = p.x;
        if(p.y < _minY) _minY = p.y;
        if(p.y > _maxY) _maxY = p.y;
    }
}

const std::vector<Point>& Boundary::getLocalPoints() const
{
    return _localPoints;
}

std::vector<Point> Boundary::getWorldPoints(float wx, float wy) const
{
    std::vector<Point> world;
    world.reserve(_localPoints.size());
    for(auto& p : _localPoints) {
        world.push_back({ p.x + wx, p.y + wy });
    }
    return world;
}

bool Boundary::contains(float wx, float wy,
                        float px, float py) const
{
    // First do a quick AABB check
    float minWX = _minX + wx,
          maxWX = _maxX + wx,
          minWY = _minY + wy,
          maxWY = _maxY + wy;
    if(px < minWX || px > maxWX ||
       py < minWY || py > maxWY)
        return false;

    // Then do a winding‐number or ray‐cast point‐in‐polygon test:
    // (here’s a simple ray‐cast implementation)
    bool inside = false;
    auto pts = getWorldPoints(wx,wy);
    size_t n = pts.size();
    for(size_t i=0, j=n-1; i<n; j=i++) {
        auto& pi = pts[i];
        auto& pj = pts[j];
        bool intersect = ((pi.y>py) != (pj.y>py)) &&
                         (px < (pj.x-pi.x)*(py-pi.y)/(pj.y-pi.y)+pi.x);
        if(intersect) inside = !inside;
    }
    return inside;
}
