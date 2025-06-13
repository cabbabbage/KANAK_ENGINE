// distance_transform.hpp
#pragma once

#include <vector>
#include <cstdint>

namespace dt {
    /**
     * Compute the Euclidean Distance Transform of a binary mask.
     * @param mask A flat vector of size w*h: 1 indicates inside the region, 0 outside.
     * @param w Width of the mask (number of columns).
     * @param h Height of the mask (number of rows).
     * @return A vector<float> of size w*h, where each value is the Euclidean distance
     *         from the pixel to the nearest "inside" pixel.
     */
    std::vector<float>
    euclidean_distance_transform(const std::vector<uint8_t>& mask,
                                 int w,
                                 int h);
} // namespace dt
