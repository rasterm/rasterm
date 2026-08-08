/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelPalette.hpp>

#include <rasterm/Color.hpp>

#include <cmath>

namespace rasterm {
namespace {

struct Oklab {
    double lightness;
    double greenRed;
    double blueYellow;
};

Oklab toOklab(const int red, const int green, const int blue)
{
    const auto linear = [](const int channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    const double r = linear(red);
    const double g = linear(green);
    const double b = linear(blue);
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {
        .lightness = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        .greenRed = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        .blueYellow = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
    };
}

uint32_t hslColor(const double hue, const double saturation, const double lightness)
{
    const double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
    const double hueSection = hue / 60.0;
    const double secondary = chroma * (1.0 - std::abs(std::fmod(hueSection, 2.0) - 1.0));
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;

    if (hueSection < 1.0) {
        r = chroma;
        g = secondary;
    }
    else if (hueSection < 2.0) {
        r = secondary;
        g = chroma;
    }
    else if (hueSection < 3.0) {
        g = chroma;
        b = secondary;
    }
    else if (hueSection < 4.0) {
        g = secondary;
        b = chroma;
    }
    else if (hueSection < 5.0) {
        r = secondary;
        b = chroma;
    }
    else {
        r = chroma;
        b = secondary;
    }

    const double offset = lightness - chroma / 2.0;
    const auto channel = [offset](const double value) {
        return static_cast<uint32_t>(std::clamp(std::lround((value + offset) * 255.0), 0l, 255l));
    };
    return (channel(r) << 16) | (channel(g) << 8) | channel(b);
}

}

void FastColorAnalyzer::analyzeFrameFast(const FrameView& image, const PixelLayout layout)
{
    colorFreq.clear();
    luminanceHistogram.fill(0.0f);
    colorFreq.reserve(1024);
    constexpr int sampleStep = 12;
    int samples = 0;
    for (int y = 0; y < image.height; y += sampleStep) {
        const uint8_t* row = image.data + static_cast<std::ptrdiff_t>(y) * image.stride;
        for (int x = 0; x < image.width; x += sampleStep) {
            const uint8_t r = layout == PixelLayout::RGB ? row[3 * x] : row[3 * x + 2];
            const uint8_t g = row[3 * x + 1];
            const uint8_t b = layout == PixelLayout::RGB ? row[3 * x + 2] : row[3 * x];
            ++colorFreq[(r << 16) | (g << 8) | b];
            const int luminance = (54 * r + 183 * g + 19 * b) >> 8;
            ++luminanceHistogram[std::min(luminance >> 4, 15)];
            ++samples;
        }
    }
    if (samples > 0) {
        for (float& value : luminanceHistogram) {
            value /= samples;
        }
    }
}

float FastColorAnalyzer::sceneDifference(const FastColorAnalyzer& other) const noexcept
{
    float difference = 0.0f;
    for (std::size_t index = 0; index < luminanceHistogram.size(); ++index) {
        difference += std::abs(luminanceHistogram[index] - other.luminanceHistogram[index]);
    }
    return difference * 0.5f;
}

std::vector<uint32_t> FastColorAnalyzer::getMostFrequentColors(const int maxColors) const
{
    std::vector<std::pair<int, uint32_t>> frequencies;
    frequencies.reserve(colorFreq.size());
    for (const auto& [rgb, frequency] : colorFreq) {
        frequencies.emplace_back(frequency, rgb);
    }
    std::sort(frequencies.begin(), frequencies.end(), std::greater<>());

    const int resultSize = std::min(maxColors, static_cast<int>(frequencies.size()));
    std::vector<uint32_t> result;
    result.reserve(resultSize);
    for (int i = 0; i < resultSize; ++i) {
        result.push_back(frequencies[i].second);
    }
    return result;
}

FastAdaptiveColorMapper::FastAdaptiveColorMapper(const int maxColors) :
    palette(maxColors),
    quantizedLookup(216, -1),
    maxColors(maxColors)
{
    palette[0] = 0x000000;
    rgbToColorNum.reserve(512);
}

void FastAdaptiveColorMapper::buildOptimalPalette(const FastColorAnalyzer& analyzer)
{
    rgbToColorNum.clear();
    std::fill(quantizedLookup.begin(), quantizedLookup.end(), -1);
    nextColorNum = 1;
    const auto frequentColors = analyzer.getMostFrequentColors(std::min(128, maxColors - 1));
    for (const uint32_t rgb : frequentColors) {
        if (nextColorNum >= maxColors) {
            break;
        }
        palette[nextColorNum] = rgb;
        rgbToColorNum[rgb] = nextColorNum;
        const int quantizedIndex =
            (q6(static_cast<uint8_t>(rgb >> 16)) * 6 + q6(static_cast<uint8_t>(rgb >> 8))) * 6 +
            q6(static_cast<uint8_t>(rgb));
        if (quantizedLookup[quantizedIndex] == -1) {
            quantizedLookup[quantizedIndex] = nextColorNum;
        }
        ++nextColorNum;
    }

    for (int qr = 0; qr < 6; ++qr) {
        for (int qg = 0; qg < 6; ++qg) {
            for (int qb = 0; qb < 6; ++qb) {
                const Oklab source = toOklab(qr * 51, qg * 51, qb * 51);
                int best = 1;
                double bestDistance = std::numeric_limits<double>::max();
                for (int color = 1; color < nextColorNum; ++color) {
                    const uint32_t rgb = palette[color];
                    const Oklab candidate = toOklab((rgb >> 16) & 0xFF,
                                                     (rgb >> 8) & 0xFF,
                                                     rgb & 0xFF);
                    const double dl = source.lightness - candidate.lightness;
                    const double da = source.greenRed - candidate.greenRed;
                    const double db = source.blueYellow - candidate.blueYellow;
                    const double distance = dl * dl + da * da + db * db;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = color;
                    }
                }
                quantizedLookup[(qr * 6 + qg) * 6 + qb] = best;
            }
        }
    }
    previousHistogram = analyzer.luminanceHistogram;
    hasHistogram = true;
}

FastColorMapper::FastColorMapper(const int maxColors, const int /*levels*/) : maxColors(maxColors)
{
    colorNumToRgb.resize(maxColors);
    colorUsed.resize(maxColors);
    colorNumToRgb[0] = 0x000000;

    const auto appendColor = [this](const uint32_t rgb) {
        if (std::find(colorNumToRgb.begin() + 1, colorNumToRgb.begin() + nextColorNum, rgb) ==
            colorNumToRgb.begin() + nextColorNum && nextColorNum < this->maxColors) {
            colorNumToRgb[nextColorNum++] = rgb;
        }
    };

    constexpr int grayscaleLevels = 24;
    for (int level = 0; level < grayscaleLevels; ++level) {
        const uint32_t value = static_cast<uint32_t>(
            (level * 255 + (grayscaleLevels - 1) / 2) / (grayscaleLevels - 1));
        appendColor((value << 16) | (value << 8) | value);
    }

    /* a hue/lightness palette includes muted chroma explicitly. this prevents
       low saturation browns and shadows from collapsing into the neutral ramp. */

    constexpr double saturations[] = { 0.22, 0.55, 1.0 };
    constexpr double lightnesses[] = { 0.18, 0.35, 0.50, 0.70, 0.85 };
    for (int hue = 0; hue < 360; hue += 30) {
        for (const double saturation : saturations) {
            for (const double lightness : lightnesses) {
                appendColor(hslColor(hue, saturation, lightness));
            }
        }
    }

    std::vector<Oklab> perceptualPalette(nextColorNum);
    for (int color = 1; color < nextColorNum; ++color) {
        const uint32_t rgb = colorNumToRgb[color];
        perceptualPalette[color] = toOklab((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    constexpr int lookupLevels = 32;
    rgbLookup.resize(lookupLevels * lookupLevels * lookupLevels);
    rgbLookup32.resize(lookupLevels * lookupLevels * lookupLevels);
    for (int lr = 0; lr < lookupLevels; ++lr) {
        const int r = (lr * 255 + 15) / 31;
        for (int lg = 0; lg < lookupLevels; ++lg) {
            const int g = (lg * 255 + 15) / 31;
            for (int lb = 0; lb < lookupLevels; ++lb) {
                const int b = (lb * 255 + 15) / 31;
                const Oklab source = toOklab(r, g, b);
                int bestColor = 1;
                double bestDistance = std::numeric_limits<double>::max();
                for (int color = 1; color < nextColorNum; ++color) {
                    const Oklab candidate = perceptualPalette[color];
                    const double deltaLightness = source.lightness - candidate.lightness;
                    const double deltaGreenRed = source.greenRed - candidate.greenRed;
                    const double deltaBlueYellow = source.blueYellow - candidate.blueYellow;
                    const double distance = 1.5 * deltaLightness * deltaLightness +
                        deltaGreenRed * deltaGreenRed + deltaBlueYellow * deltaBlueYellow;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestColor = color;
                    }
                }
                rgbLookup[(lr * lookupLevels + lg) * lookupLevels + lb] =
                    static_cast<uint8_t>(bestColor);
                rgbLookup32[(lr * lookupLevels + lg) * lookupLevels + lb] = bestColor;
            }
        }
    }
}

void FastColorMapper::reset()
{
    std::fill(colorUsed.begin(), colorUsed.end(), false);
    usedColorCount = 0;
}

}