/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/IndexedFrame.hpp>

#include <cmath>

namespace rasterm::test {

struct Oklab {
    double lightness;
    double greenRed;
    double blueYellow;
};

inline double srgbToLinear(const std::uint8_t channel) noexcept
{
    const double value = channel / 255.0;
    return value <= 0.04045 ? value / 12.92
                            : std::pow((value + 0.055) / 1.055, 2.4);
}

inline Oklab toOklab(const RgbColor color) noexcept
{
    const double red = srgbToLinear(color.red);
    const double green = srgbToLinear(color.green);
    const double blue = srgbToLinear(color.blue);
    const double l = std::cbrt(0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue);
    const double m = std::cbrt(0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue);
    const double s = std::cbrt(0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue);
    return {
        0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
    };
}

inline double oklabError(const RgbColor left, const RgbColor right) noexcept
{
    const Oklab a = toOklab(left);
    const Oklab b = toOklab(right);
    const double dl = a.lightness - b.lightness;
    const double da = a.greenRed - b.greenRed;
    const double db = a.blueYellow - b.blueYellow;
    return std::sqrt(dl * dl + da * da + db * db);
}

inline RgbColor sixelRegisterColor(const RgbColor color) noexcept
{
    const auto channel = [](const std::uint8_t value) {
        const int percent = (static_cast<int>(value) * 100 + 127) / 255;
        return static_cast<std::uint8_t>((percent * 255 + 50) / 100);
    };
    return { channel(color.red), channel(color.green), channel(color.blue) };
}

inline constexpr double indexedMaximumOklabError = 0.015;
inline constexpr double indexedMeanOklabError = 0.005;
inline constexpr double realtimeChartMaximumOklabError = 0.160;
inline constexpr double realtimeChartMeanOklabError = 0.075;

}