/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <algorithm>
#include <cmath>

namespace rasterm {

struct Extent {
    int width = 0;
    int height = 0;
};

[[nodiscard]] inline Extent fitWithin(const Extent source, const Extent bounds) noexcept
{
    if (source.width <= 0 || source.height <= 0 || bounds.width <= 0 || bounds.height <= 0) {
        return {};
    }
    const double scale = std::min(static_cast<double>(bounds.width) / source.width,
                                  static_cast<double>(bounds.height) / source.height);
    return {
        std::max(1, static_cast<int>(std::lround(source.width * scale))),
        std::max(1, static_cast<int>(std::lround(source.height * scale))),
    };
}

}