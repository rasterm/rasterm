/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Frame.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace rasterm::test {

struct QualityFixture {
    std::string_view name;
    int width = 96;
    int height = 60;
    double maximumMeanChannelError = 0.0;
    double maximumMeanChannelBias = 0.0;
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] FrameView frame() const noexcept
    {
        return { pixels.data(), width, height, width * 3, PixelFormat::RGB24 };
    }
};

inline QualityFixture fixture(std::string_view name, const double maximumError,
                              const double maximumBias = 2.0,
                              const int width = 96, const int height = 60)
{
    QualityFixture result{ name, width, height, maximumError, maximumBias };
    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height * 3);
    return result;
}

inline void setPixel(QualityFixture& item, const int x, const int y,
                     const RgbColor color) noexcept
{
    const std::size_t offset = static_cast<std::size_t>(y * item.width + x) * 3;
    item.pixels[offset] = color.red;
    item.pixels[offset + 1] = color.green;
    item.pixels[offset + 2] = color.blue;
}

inline std::vector<QualityFixture> makeQualityFixtures()
{
    std::vector<QualityFixture> result;
    result.reserve(6);

    auto gradient = fixture("gradient", 8.0);
    for (int y = 0; y < gradient.height; ++y) {
        for (int x = 0; x < gradient.width; ++x) {
            setPixel(gradient, x, y, {
                static_cast<std::uint8_t>(x * 255 / (gradient.width - 1)),
                static_cast<std::uint8_t>(y * 255 / (gradient.height - 1)),
                static_cast<std::uint8_t>((x + y) * 255 /
                    (gradient.width + gradient.height - 2)),
            });
        }
    }
    result.push_back(std::move(gradient));

    constexpr std::array skinTones{
        RgbColor{ 255, 224, 196 }, RgbColor{ 238, 193, 160 },
        RgbColor{ 218, 160, 120 }, RgbColor{ 190, 128, 92 },
        RgbColor{ 156, 98, 69 }, RgbColor{ 112, 68, 48 },
    };
    auto skin = fixture("skin-tones", 6.0);
    for (int y = 0; y < skin.height; ++y) {
        for (int x = 0; x < skin.width; ++x) {
            const auto base = skinTones[static_cast<std::size_t>(x * skinTones.size() /
                                                                 skin.width)];
            const int shade = y * 32 / (skin.height - 1) - 16;
            setPixel(skin, x, y, {
                static_cast<std::uint8_t>(std::clamp(static_cast<int>(base.red) + shade, 0, 255)),
                static_cast<std::uint8_t>(std::clamp(static_cast<int>(base.green) + shade, 0, 255)),
                static_cast<std::uint8_t>(std::clamp(static_cast<int>(base.blue) + shade, 0, 255)),
            });
        }
    }
    result.push_back(std::move(skin));

    auto dark = fixture("dark-scene", 6.0);
    for (int y = 0; y < dark.height; ++y) {
        for (int x = 0; x < dark.width; ++x) {
            const int glow = std::max(0, 28 - (std::abs(x - 60) + std::abs(y - 26)) / 2);
            setPixel(dark, x, y, {
                static_cast<std::uint8_t>(4 + x * 12 / dark.width + glow),
                static_cast<std::uint8_t>(6 + y * 10 / dark.height + glow / 2),
                static_cast<std::uint8_t>(10 + (x + y) * 14 /
                    (dark.width + dark.height) + glow),
            });
        }
    }
    result.push_back(std::move(dark));

    auto ui = fixture("ui-text", 2.0);
    for (int y = 0; y < ui.height; ++y) {
        for (int x = 0; x < ui.width; ++x) {
            RgbColor color{ 18, 20, 24 };
            if (x >= 8 && x < 88 && y >= 8 && y < 52) color = { 30, 34, 41 };
            if (x >= 14 && x < 78 && (y == 18 || y == 19 || y == 28 || y == 29)) {
                color = { 224, 228, 235 };
            }
            if (x >= 14 && x < 48 && y >= 38 && y < 47) color = { 58, 145, 245 };
            setPixel(ui, x, y, color);
        }
    }
    result.push_back(std::move(ui));

    auto motion = fixture("high-motion", 18.0);
    std::uint32_t noise = 0x13579bdfu;
    for (int y = 0; y < motion.height; ++y) {
        for (int x = 0; x < motion.width; ++x) {
            noise = noise * 1664525u + 1013904223u;
            setPixel(motion, x, y, {
                static_cast<std::uint8_t>(noise >> 24),
                static_cast<std::uint8_t>(noise >> 16),
                static_cast<std::uint8_t>(noise >> 8),
            });
        }
    }
    result.push_back(std::move(motion));

    auto photograph = fixture("photograph", 10.0, 2.0, 240, 144);
    for (int y = 0; y < photograph.height; ++y) {
        for (int x = 0; x < photograph.width; ++x) {
            const int horizontal = x * 255 / (photograph.width - 1);
            const int vertical = y * 255 / (photograph.height - 1);
            const int center = std::max(0, 96 -
                (std::abs(x - photograph.width / 2) +
                 std::abs(y - photograph.height / 2)));
            setPixel(photograph, x, y, {
                static_cast<std::uint8_t>(std::clamp(24 + horizontal * 3 / 5 + center / 2,
                                                     0, 255)),
                static_cast<std::uint8_t>(std::clamp(28 + vertical / 2 + center / 3,
                                                     0, 255)),
                static_cast<std::uint8_t>(std::clamp(36 + (255 - horizontal) / 3 +
                                                     vertical / 4, 0, 255)),
            });
        }
    }
    result.push_back(std::move(photograph));
    return result;
}

}
