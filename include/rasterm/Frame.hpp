/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/PixelFormat.hpp>
#include <rasterm/Metadata.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace rasterm {

struct FrameView {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    std::ptrdiff_t stride = 0;
    PixelFormat format = PixelFormat::RGB24;
    FrameMetadata metadata{};

    [[nodiscard]] bool isValid() const noexcept
    {
        const int pixelBytes = bytesPerPixel(format);
        if (data == nullptr || width <= 0 || height <= 0 || pixelBytes == 0 || stride <= 0) {
            return false;
        }
        if (width > (std::numeric_limits<int>::max)() / height) {
            return false;
        }
        constexpr auto maximum = (std::numeric_limits<std::ptrdiff_t>::max)();
        if (width > maximum / pixelBytes) {
            return false;
        }
        const std::ptrdiff_t rowBytes = static_cast<std::ptrdiff_t>(width) * pixelBytes;
        if (stride < rowBytes ||
            (height > 1 && stride > (maximum - rowBytes) / (height - 1))) {
            return false;
        }
        return static_cast<std::size_t>(rowBytes) <=
                (std::numeric_limits<std::size_t>::max)() / static_cast<std::size_t>(height) &&
            metadata.damage.isValidFor(width, height);
    }
};

}