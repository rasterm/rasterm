/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/sixel/SixelEncoder.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rasterm {

struct ColorAnalyzer {
    static constexpr int analysisLevels = 12;
    static constexpr int analysisBucketCount =
        analysisLevels * analysisLevels * analysisLevels;

    struct ColorBucket {
        std::uint64_t redTotal = 0;
        std::uint64_t greenTotal = 0;
        std::uint64_t blueTotal = 0;
        std::uint32_t frequency = 0;
    };

    std::array<ColorBucket, analysisBucketCount> colorBuckets{};
    std::array<float, 16> luminanceHistogram{};

    void analyzeFrame(const FrameView& image, PixelLayout layout);
    [[nodiscard]] std::vector<uint32_t> getRepresentativeColors(int maxColors) const;
};

struct AdaptivePaletteMapper {
    static constexpr int lookupLevels = 32;

    std::vector<uint32_t> palette;
    std::unordered_map<uint32_t, int> rgbToColorNum;
    std::vector<std::uint8_t> paletteLookup;
    int nextColorNum = 1;
    const int maxColors;
    std::array<float, 16> previousHistogram{};
    bool hasHistogram = false;

    explicit AdaptivePaletteMapper(int maxColors);
    void buildOptimalPalette(const ColorAnalyzer& analyzer);

    inline int getColorNumberReadOnly(const uint8_t r, const uint8_t g,
                                      const uint8_t b) const
    {
        const uint32_t rgb = (r << 16) | (g << 8) | b;
        const auto cached = rgbToColorNum.find(rgb);
        if (cached != rgbToColorNum.end()) return cached->second;

        const int lr = (static_cast<int>(r) * (lookupLevels - 1) + 127) / 255;
        const int lg = (static_cast<int>(g) * (lookupLevels - 1) + 127) / 255;
        const int lb = (static_cast<int>(b) * (lookupLevels - 1) + 127) / 255;
        return paletteLookup[(lr * lookupLevels + lg) * lookupLevels + lb];
    }
};

struct FixedPaletteMapper {
    std::vector<uint32_t> colorNumToRgb;
    std::vector<bool> colorUsed;
    std::vector<uint8_t> rgbLookup;
    std::vector<std::int32_t> rgbLookup32;
    int nextColorNum = 1;
    int usedColorCount = 0;
    const int maxColors;

    explicit FixedPaletteMapper(int maxColors);
    void reset();
};

}
