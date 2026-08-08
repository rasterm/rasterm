/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Metadata.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace rasterm {

struct RgbColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend constexpr bool operator==(const RgbColor&, const RgbColor&) noexcept = default;
};

struct PaletteView {
    const RgbColor* colors = nullptr;
    std::size_t size = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return colors != nullptr && size > 0 && size <= 256;
    }
};

struct IndexedFrameView {
    const std::uint8_t* indices = nullptr;
    int width = 0;
    int height = 0;
    std::ptrdiff_t stride = 0;
    PaletteView palette{};
    FrameMetadata metadata{};

    [[nodiscard]] bool isValid() const noexcept
    {
        if (indices == nullptr || width <= 0 || height <= 0 || stride <= 0 ||
            stride < width || !palette.isValid()) {
            return false;
        }
        if (width > (std::numeric_limits<int>::max)() / height) {
            return false;
        }
        constexpr auto maximum = (std::numeric_limits<std::ptrdiff_t>::max)();
        if (height > 1 && stride > (maximum - width) / (height - 1)) {
            return false;
        }
        return static_cast<std::size_t>(width) <=
                (std::numeric_limits<std::size_t>::max)() / static_cast<std::size_t>(height) &&
            metadata.damage.isValidFor(width, height);
    }

    [[nodiscard]] bool hasValidIndices() const noexcept
    {
        if (!isValid()) {
            return false;
        }
        for (int row = 0; row < height; ++row) {
            const std::uint8_t* source = indices + static_cast<std::ptrdiff_t>(row) * stride;
            for (int column = 0; column < width; ++column) {
                if (source[column] >= palette.size) {
                    return false;
                }
            }
        }
        return true;
    }
};

}