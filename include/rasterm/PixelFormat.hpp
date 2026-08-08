/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>

namespace rasterm {

enum class PixelFormat : std::int32_t {
    RGB24 = 0,
    BGR24 = 1,
    RGBA32 = 2,
    BGRA32 = 3,
    RGB565 = 4,
    XRGB1555 = 5,
    RGBA4444 = 6,
};

[[nodiscard]] constexpr bool isValidPixelFormat(const PixelFormat format) noexcept
{
    switch (format) {
    case PixelFormat::RGB24:
    case PixelFormat::BGR24:
    case PixelFormat::RGBA32:
    case PixelFormat::BGRA32:
    case PixelFormat::RGB565:
    case PixelFormat::XRGB1555:
    case PixelFormat::RGBA4444:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr int bytesPerPixel(const PixelFormat format) noexcept
{
    switch (format) {
    case PixelFormat::RGB24:
    case PixelFormat::BGR24:
        return 3;
    case PixelFormat::RGBA32:
    case PixelFormat::BGRA32:
        return 4;
    case PixelFormat::RGB565:
    case PixelFormat::XRGB1555:
    case PixelFormat::RGBA4444:
        return 2;
    }
    return 0;
}

}