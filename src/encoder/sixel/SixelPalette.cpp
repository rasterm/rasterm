/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelPalette.hpp>

#include <rasterm/Color.hpp>

#include <algorithm>
#include <cmath>

namespace rasterm {
namespace {

struct Oklab {
    double lightness;
    double greenRed;
    double blueYellow;
};

double colorDistance(const Oklab& first, const Oklab& second)
{
    const double deltaLightness = first.lightness - second.lightness;
    const double deltaGreenRed = first.greenRed - second.greenRed;
    const double deltaBlueYellow = first.blueYellow - second.blueYellow;
    return 1.5 * deltaLightness * deltaLightness +
        deltaGreenRed * deltaGreenRed + deltaBlueYellow * deltaBlueYellow;
}

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

void ColorAnalyzer::analyzeFrame(const FrameView& image, const PixelLayout layout)
{
    colorBuckets.fill({});
    luminanceHistogram.fill(0.0f);
    constexpr double targetSamples = 8192.0;
    const double pixelCount = static_cast<double>(image.width) * image.height;
    const int sampleStep = std::max(1, static_cast<int>(
        std::ceil(std::sqrt(pixelCount / targetSamples))));
    const int sourceStride = pixelStride(layout);
    int samples = 0;
    for (int y = 0; y < image.height; y += sampleStep) {
        const uint8_t* row = image.data + static_cast<std::ptrdiff_t>(y) * image.stride;
        for (int x = 0; x < image.width; x += sampleStep) {
            const std::uint8_t* pixel = row + sourceStride * x;
            const PixelChannels channels = readPixel(pixel, layout);
            const uint8_t r = channels.red;
            const uint8_t g = channels.green;
            const uint8_t b = channels.blue;
            const auto quantize = [](const std::uint8_t channel) {
                return (channel * (analysisLevels - 1) + 127) / 255;
            };
            const int bucketIndex =
                (quantize(r) * analysisLevels + quantize(g)) * analysisLevels + quantize(b);
            ColorBucket& bucket = colorBuckets[static_cast<std::size_t>(bucketIndex)];
            bucket.redTotal += r;
            bucket.greenTotal += g;
            bucket.blueTotal += b;
            ++bucket.frequency;
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

std::vector<uint32_t> ColorAnalyzer::getRepresentativeColors(const int maxColors) const
{
    struct Candidate {
        std::uint32_t rgb;
        std::uint32_t frequency;
        Oklab color;
        double nearestDistance = std::numeric_limits<double>::max();
        double frequencyWeight = 0.0;
        bool selected = false;
    };

    if (maxColors <= 0) return {};

    std::vector<Candidate> candidates;
    candidates.reserve(colorBuckets.size());
    for (const ColorBucket& bucket : colorBuckets) {
        if (bucket.frequency == 0) continue;
        const auto average = [&bucket](const std::uint64_t total) {
            return static_cast<int>((total + bucket.frequency / 2) / bucket.frequency);
        };
        const int red = average(bucket.redTotal);
        const int green = average(bucket.greenTotal);
        const int blue = average(bucket.blueTotal);
        candidates.push_back({
            .rgb = static_cast<std::uint32_t>((red << 16) | (green << 8) | blue),
            .frequency = bucket.frequency,
            .color = toOklab(red, green, blue),
        });
    }

    std::vector<uint32_t> result;
    result.reserve(std::min(maxColors, static_cast<int>(candidates.size())));
    if (candidates.empty()) return result;

    const auto mostFrequent = std::max_element(
        candidates.begin(), candidates.end(),
        [](const Candidate& first, const Candidate& second) {
            return first.frequency < second.frequency;
        });
    const double maximumFrequency = mostFrequent->frequency;
    for (Candidate& candidate : candidates) {
        candidate.frequencyWeight = std::sqrt(candidate.frequency / maximumFrequency);
    }
    std::size_t selectedIndex = static_cast<std::size_t>(
        std::distance(candidates.begin(), mostFrequent));

    while (static_cast<int>(result.size()) < maxColors && result.size() < candidates.size()) {
        Candidate& selected = candidates[selectedIndex];
        selected.selected = true;
        result.push_back(selected.rgb);

        double bestScore = -1.0;
        std::size_t bestIndex = 0;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            Candidate& candidate = candidates[index];
            if (candidate.selected) continue;
            candidate.nearestDistance = std::min(
                candidate.nearestDistance, colorDistance(candidate.color, selected.color));
            const double score = candidate.nearestDistance * candidate.frequencyWeight;
            if (score > bestScore) {
                bestScore = score;
                bestIndex = index;
            }
        }
        selectedIndex = bestIndex;
    }
    return result;
}

AdaptivePaletteMapper::AdaptivePaletteMapper(const int maxColors) :
    palette(maxColors),
    paletteLookup(lookupLevels * lookupLevels * lookupLevels, 1),
    maxColors(maxColors)
{
    palette[0] = 0x000000;
    rgbToColorNum.reserve(512);
}

void AdaptivePaletteMapper::buildOptimalPalette(const ColorAnalyzer& analyzer)
{
    rgbToColorNum.clear();
    nextColorNum = 1;
    const auto representativeColors =
        analyzer.getRepresentativeColors(maxColors - 1);
    for (const uint32_t rgb : representativeColors) {
        if (nextColorNum >= maxColors) {
            break;
        }
        palette[nextColorNum] = rgb;
        rgbToColorNum[rgb] = nextColorNum;
        ++nextColorNum;
    }

    std::vector<Oklab> perceptualPalette(static_cast<std::size_t>(nextColorNum));
    for (int color = 1; color < nextColorNum; ++color) {
        const uint32_t rgb = palette[color];
        perceptualPalette[color] = toOklab((rgb >> 16) & 0xFF,
                                           (rgb >> 8) & 0xFF,
                                           rgb & 0xFF);
    }
    for (int lr = 0; lr < lookupLevels; ++lr) {
        const int red = (lr * 255 + (lookupLevels - 1) / 2) / (lookupLevels - 1);
        for (int lg = 0; lg < lookupLevels; ++lg) {
            const int green = (lg * 255 + (lookupLevels - 1) / 2) / (lookupLevels - 1);
            for (int lb = 0; lb < lookupLevels; ++lb) {
                const int blue = (lb * 255 + (lookupLevels - 1) / 2) / (lookupLevels - 1);
                const Oklab source = toOklab(red, green, blue);
                int best = 1;
                double bestDistance = std::numeric_limits<double>::max();
                for (int color = 1; color < nextColorNum; ++color) {
                    const double distance = colorDistance(source, perceptualPalette[color]);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = color;
                    }
                }
                paletteLookup[(lr * lookupLevels + lg) * lookupLevels + lb] =
                    static_cast<std::uint8_t>(best);
            }
        }
    }
    previousHistogram = analyzer.luminanceHistogram;
    hasHistogram = true;
}

FixedPaletteMapper::FixedPaletteMapper(const int maxColors) : maxColors(maxColors)
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

void FixedPaletteMapper::reset()
{
    std::fill(colorUsed.begin(), colorUsed.end(), false);
    usedColorCount = 0;
}

}
