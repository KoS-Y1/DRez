//
// Created by y1 on 2026-05-16.
//

#include "Utils.h"

#include <cmath>

namespace drez::utils {
float Halton(uint32_t i, uint32_t b) {
    float f = 1.0f;
    float r = 0.0f;

    while (i > 0) {
        f /= static_cast<float>(b);
        r  = r + f * static_cast<float>(i % b);
        i  = static_cast<uint32_t>(std::floorf(static_cast<float>(i) / static_cast<float>(b)));
    }

    return r;
}
} // namespace drez::utils