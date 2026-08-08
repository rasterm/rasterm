/* SPDX-License-Identifier: Apache-2.0 */

#include <damage/DamageTracker.hpp>

#include <algorithm>
#include <cstring>
#include <emmintrin.h>

namespace rasterm {
namespace {

bool rowsEqual(const std::uint8_t* current, const std::uint8_t* previous,
               const std::size_t bytes) noexcept
{
    std::size_t offset = 0;
    for (; offset + sizeof(__m128i) <= bytes; offset += sizeof(__m128i)) {
        const __m128i left = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(current + offset));
        const __m128i right = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(previous + offset));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(left, right)) != 0xFFFF) {
            return false;
        }
    }
    return offset == bytes ||
        std::memcmp(current + offset, previous + offset, bytes - offset) == 0;
}

}

DamageTracker::DamageTracker(DamageOptions options) : options(options)
{
    this->options.tileWidth = std::max(1, this->options.tileWidth);
    this->options.tileHeight = std::max(1, this->options.tileHeight);
    this->options.fullFrameThreshold = std::clamp(this->options.fullFrameThreshold, 0.0f, 1.0f);
}

DamageResult DamageTracker::compareAndUpdate(const FrameView& frame)
{
    if (!frame.isValid()) {
        return { .isFullFrame = false, .hasChanges = false,
                 .changedAreaRatio = 0.0f, .regions = {} };
    }
    return compareAndUpdate(frame.data, frame.width, frame.height, frame.stride,
                            rasterm::bytesPerPixel(frame.format),
                            static_cast<int>(frame.format), {});
}

DamageResult DamageTracker::compareAndUpdate(const IndexedFrameView& frame)
{
    if (!frame.isValid()) {
        return { .isFullFrame = false, .hasChanges = false,
                 .changedAreaRatio = 0.0f, .regions = {} };
    }
    return compareAndUpdate(frame.indices, frame.width, frame.height, frame.stride,
                            1, 100, frame.palette);
}

void DamageTracker::update(const FrameView& frame)
{
    if (frame.isValid()) {
        copyFrame(frame.data, frame.width, frame.height, frame.stride,
                  rasterm::bytesPerPixel(frame.format), static_cast<int>(frame.format), {});
    }
}

void DamageTracker::update(const IndexedFrameView& frame)
{
    if (frame.isValid()) {
        copyFrame(frame.indices, frame.width, frame.height, frame.stride, 1, 100, frame.palette);
    }
}

DamageResult DamageTracker::compareAndUpdate(const std::uint8_t* data, const int width,
                                             const int height, const std::ptrdiff_t stride,
                                             const int pixelBytes, const int formatTag,
                                             const PaletteView palette)
{
    if (!hasSameShape(width, height, pixelBytes, formatTag, palette)) {
        copyFrame(data, width, height, stride, pixelBytes, formatTag, palette);
        return { .isFullFrame = true, .hasChanges = true,
                 .changedAreaRatio = 1.0f, .regions = {} };
    }

    const int tilesX = (width - 1) / options.tileWidth + 1;
    const int tilesY = (height - 1) / options.tileHeight + 1;
    auto& regions = scratchRegions;
    regions.clear();
    const std::size_t tileCount = static_cast<std::size_t>(tilesX) *
        static_cast<std::size_t>(tilesY);
    regions.reserve(tileCount / 4);
    size_t changedPixels = 0;

    for (int tileY = 0; tileY < tilesY; ++tileY) {
        const int y = tileY * options.tileHeight;
        const int tileHeight = std::min(options.tileHeight, height - y);
        DamageRegion active{};

        for (int tileX = 0; tileX < tilesX; ++tileX) {
            const int x = tileX * options.tileWidth;
            const int tileWidth = std::min(options.tileWidth, width - x);
            bool changed = false;

            for (int row = 0; row < tileHeight; ++row) {
                const auto* current = data + static_cast<std::ptrdiff_t>(y + row) * stride + x * pixelBytes;
                const auto* previous = previousFrame.data() +
                    (static_cast<size_t>(y + row) * width + x) * pixelBytes;
                if (!rowsEqual(current, previous,
                               static_cast<size_t>(tileWidth) * pixelBytes)) {
                    changed = true;
                    break;
                }
            }

            if (!changed) {
                if (active.isValid()) {
                    regions.push_back(active);
                    active = {};
                }
                continue;
            }

            changedPixels += static_cast<size_t>(tileWidth) * tileHeight;
            if (active.isValid()) {
                active.width += tileWidth;
            }
            else {
                active = { x, y, tileWidth, tileHeight };
            }
        }

        if (active.isValid()) {
            regions.push_back(active);
        }
    }

    copyFrame(data, width, height, stride, pixelBytes, formatTag, palette);
    if (regions.empty()) {
        return { .isFullFrame = false, .hasChanges = false,
                 .changedAreaRatio = 0.0f, .regions = {} };
    }

    const float changedAreaRatio = static_cast<float>(changedPixels) /
        static_cast<float>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    if (changedAreaRatio >= options.fullFrameThreshold) {
        return { .isFullFrame = true, .hasChanges = true,
                 .changedAreaRatio = changedAreaRatio, .regions = {} };
    }

    /* merge vertically adjacent strips with the same horizontal bounds */

    for (size_t i = 0; i < regions.size(); ++i) {
        for (size_t j = i + 1; j < regions.size();) {
            if (regions[i].x == regions[j].x && regions[i].width == regions[j].width &&
                regions[i].y + regions[i].height == regions[j].y) {
                regions[i].height += regions[j].height;
                regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(j));
            }
            else {
                ++j;
            }
        }
    }

    return { .isFullFrame = false, .hasChanges = true,
             .changedAreaRatio = changedAreaRatio, .regions = regions };
}

void DamageTracker::reset()
{
    previousFrame.clear();
    previousPalette.clear();
    previousWidth = 0;
    previousHeight = 0;
    previousBytesPerPixel = 0;
    previousFormatTag = -1;
}

bool DamageTracker::hasSameShape(const int width, const int height, const int pixelBytes,
                                 const int formatTag, const PaletteView palette) const noexcept
{
    if (width != previousWidth || height != previousHeight || pixelBytes != previousBytesPerPixel ||
        formatTag != previousFormatTag ||
        previousFrame.size() != static_cast<size_t>(width) * height * pixelBytes ||
        previousPalette.size() != palette.size) {
        return false;
    }
    for (std::size_t index = 0; index < palette.size; ++index) {
        if (previousPalette[index] != palette.colors[index]) {
            return false;
        }
    }
    return true;
}

void DamageTracker::copyFrame(const std::uint8_t* data, const int width, const int height,
                              const std::ptrdiff_t stride, const int pixelBytes,
                              const int formatTag, const PaletteView palette)
{
    const std::size_t rowBytes = static_cast<std::size_t>(width) * pixelBytes;
    previousFrame.resize(rowBytes * height);
    for (int row = 0; row < height; ++row) {
        const auto* source = data + static_cast<std::ptrdiff_t>(row) * stride;
        auto* destination = previousFrame.data() + static_cast<size_t>(row) * rowBytes;
        std::memcpy(destination, source, rowBytes);
    }
    if (palette.size == 0) {
        previousPalette.clear();
    }
    else {
        previousPalette.assign(palette.colors, palette.colors + palette.size);
    }
    previousWidth = width;
    previousHeight = height;
    previousBytesPerPixel = pixelBytes;
    previousFormatTag = formatTag;
}

}