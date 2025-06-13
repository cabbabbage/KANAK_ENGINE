// distance_transform.cpp
#include "distance_transform.hpp"
#include <limits>
#include <cmath>
#include <algorithm>

namespace dt {

// 1D squared distance transform (Felzenszwalb & Huttenlocher)
static void edt_1d(const std::vector<float>& f, int n, std::vector<float>& d) {
    std::vector<int> v(n);
    std::vector<float> z(n + 1);
    int k = 0;
    v[0] = 0;
    z[0] = -std::numeric_limits<float>::infinity();
    z[1] = std::numeric_limits<float>::infinity();

    for (int q = 1; q < n; q++) {
        float s;
        do {
            int p = v[k];
            s = ((f[q] + q*q) - (f[p] + p*p)) / (2.0f * (q - p));
            if (s <= z[k]) k--;
        } while (s <= z[k]);
        k++;
        v[k] = q;
        z[k] = s;
        z[k+1] = std::numeric_limits<float>::infinity();
    }

    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k+1] < q) k++;
        float diff = q - v[k];
        d[q] = (diff * diff) + f[v[k]];
    }
}

std::vector<float> euclidean_distance_transform(const std::vector<uint8_t>& mask, int w, int h) {
    int sz = w * h;
    std::vector<float> dist(sz);
    const float INF = std::numeric_limits<float>::infinity();

    // Initialize: f = 0 at mask==1, INF at mask==0
    std::vector<float> f(sz);
    for (int i = 0; i < sz; ++i) {
        f[i] = (mask[i] ? 0.0f : INF);
    }

    std::vector<float> tmp(sz);

    // Transform columns
    for (int x = 0; x < w; ++x) {
        std::vector<float> col(h);
        for (int y = 0; y < h; ++y) col[y] = f[y*w + x];
        std::vector<float> dcol(h);
        edt_1d(col, h, dcol);
        for (int y = 0; y < h; ++y) tmp[y*w + x] = dcol[y];
    }

    // Transform rows
    for (int y = 0; y < h; ++y) {
        std::vector<float> row(w);
        for (int x = 0; x < w; ++x) row[x] = tmp[y*w + x];
        std::vector<float> drow(w);
        edt_1d(row, w, drow);
        for (int x = 0; x < w; ++x) {
            dist[y*w + x] = std::sqrt(drow[x]);
        }
    }

    return dist;
}

} // namespace dt
