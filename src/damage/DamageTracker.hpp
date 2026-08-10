/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rasterm {

struct DamageOptions {
    int tileWidth = 40;
    int tileHeight = 20;
    float fullFrameThreshold = 0.45f;
};

struct DamageResult {
    bool isFullFrame = true;
    bool hasChanges = true;
    float changedAreaRatio = 1.0f;
    std::span<const DamageRegion> regions;
};

class DamageTracker {
public:
    explicit DamageTracker(DamageOptions options = {});

    DamageResult compareAndUpdate(const FrameView& frame);
    DamageResult compareAndUpdate(const IndexedFrameView& frame);
    void reset();

private:
    DamageResult compareAndUpdate(const std::uint8_t* data, int width, int height,
                                  std::ptrdiff_t stride, int bytesPerPixel,
                                  int formatTag, PaletteView palette);
    [[nodiscard]] bool hasSameShape(int width, int height, int bytesPerPixel,
                                    int formatTag, PaletteView palette) const noexcept;
    void copyFrame(const std::uint8_t* data, int width, int height,
                   std::ptrdiff_t stride, int bytesPerPixel, int formatTag,
                   PaletteView palette);

    DamageOptions options;
    std::vector<std::uint8_t> previousFrame;
    std::vector<RgbColor> previousPalette;
    std::vector<DamageRegion> scratchRegions;
    int previousWidth = 0;
    int previousHeight = 0;
    int previousBytesPerPixel = 0;
    int previousFormatTag = -1;
};

}
