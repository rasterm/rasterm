/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/SixelEncoder.hpp>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <unordered_map>

namespace rasterm {

struct FastColorAnalyzer {
    std::unordered_map<uint32_t, int> colorFreq;
    std::array<float, 16> luminanceHistogram{};

    void analyzeFrameFast(const FrameView& image, PixelLayout layout);
    [[nodiscard]] std::vector<uint32_t> getMostFrequentColors(int maxColors) const;
    [[nodiscard]] float sceneDifference(const FastColorAnalyzer& other) const noexcept;
};

struct FastAdaptiveColorMapper {
    std::vector<uint32_t> palette;
    std::unordered_map<uint32_t, int> rgbToColorNum;
    std::vector<int> quantizedLookup;
    int nextColorNum = 1;
    const int maxColors;
    std::array<float, 16> previousHistogram{};
    bool hasHistogram = false;

    explicit FastAdaptiveColorMapper(int maxColors);
    void buildOptimalPalette(const FastColorAnalyzer& analyzer);

    static inline int q6(const uint8_t channel)
    {
        return (channel * 5 + 127) / 255;
    }

    inline int getColorNumber(const uint8_t r, const uint8_t g, const uint8_t b, const bool allowCacheWrite = true)
    {
        const uint32_t rgb = (r << 16) | (g << 8) | b;
        const auto cached = rgbToColorNum.find(rgb);
        if (cached != rgbToColorNum.end()) [[likely]] {
            return cached->second;
        }

        const int quantizedIndex = (q6(r) * 6 + q6(g)) * 6 + q6(b);
        if (quantizedLookup[quantizedIndex] != -1) [[likely]] {
            const int colorNumber = quantizedLookup[quantizedIndex];
            if (allowCacheWrite) {
                rgbToColorNum[rgb] = colorNumber;
            }
            return colorNumber;
        }

        int bestMatch = 1;
        int bestDifference = INT_MAX;
        const int searchColors = std::min(32, nextColorNum);
        for (int i = 1; i < searchColors && bestDifference > 256; ++i) {
            const uint32_t paletteRgb = palette[i];
            const int difference =
                std::abs(static_cast<int>(r) - static_cast<int>((paletteRgb >> 16) & 0xFF)) +
                std::abs(static_cast<int>(g) - static_cast<int>((paletteRgb >> 8) & 0xFF)) +
                std::abs(static_cast<int>(b) - static_cast<int>(paletteRgb & 0xFF));
            if (difference < bestDifference) {
                bestDifference = difference;
                bestMatch = i;
            }
        }

        if (allowCacheWrite) {
            rgbToColorNum[rgb] = bestMatch;
        }
        return bestMatch;
    }
};

struct FastColorMapper {
    std::vector<uint32_t> colorNumToRgb;
    std::vector<bool> colorUsed;
    std::vector<uint8_t> rgbLookup;
    std::vector<std::int32_t> rgbLookup32;
    int nextColorNum = 1;
    int usedColorCount = 0;
    const int maxColors;

    FastColorMapper(int maxColors, int levels);
    void reset();

    inline int getColorNumber(const uint8_t r, const uint8_t g, const uint8_t b)
    {
        const int lr = (static_cast<int>(r) * 31 + 127) / 255;
        const int lg = (static_cast<int>(g) * 31 + 127) / 255;
        const int lb = (static_cast<int>(b) * 31 + 127) / 255;
        const int colorNumber = rgbLookup[(lr * 32 + lg) * 32 + lb];
        if (!colorUsed[colorNumber]) {
            colorUsed[colorNumber] = true;
            ++usedColorCount;
        }
        return colorNumber;
    }
};

}