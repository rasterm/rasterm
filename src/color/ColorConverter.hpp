/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Color.hpp>
#include <rasterm/Frame.hpp>

#include <cstddef>
#include <vector>

namespace rasterm {

class ColorConverter {
public:
    FrameView toSrgb(const FrameView& frame, const ColorOptions& options);
    void reset() noexcept;
    [[nodiscard]] std::size_t scratchCapacity() const noexcept { return pixels.capacity(); }

private:
    std::vector<std::uint8_t> pixels;
    ColorMetadata previousColor{};
    ToneMapOperator previousToneMap = ToneMapOperator::None;
    float previousOutputPeakNits = 0.0f;
    int previousWidth = 0;
    int previousHeight = 0;
    PixelFormat previousFormat = PixelFormat::RGB24;
    bool previousDamageSupplied = false;
    bool hasConvertedFrame = false;
};

}
