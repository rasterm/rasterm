/* SPDX-License-Identifier: Apache-2.0 */

#include <color/ColorConverter.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace rasterm {
namespace {

float srgbToLinear(const float value) noexcept
{
    const float channel = std::clamp(value, 0.0f, 1.0f);
    return channel <= 0.04045f ? channel / 12.92f
                               : std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(const float value) noexcept
{
    const float channel = std::max(value, 0.0f);
    return channel <= 0.0031308f ? channel * 12.92f
                                 : 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
}

float decodeTransfer(const float encoded, const TransferFunction transfer) noexcept
{
    const float value = std::clamp(encoded, 0.0f, 1.0f);
    switch (transfer) {
    case TransferFunction::Linear: return value;
    case TransferFunction::Gamma22: return std::pow(value, 2.2f);
    case TransferFunction::Bt709:
        return value < 0.081f ? value / 4.5f : std::pow((value + 0.099f) / 1.099f, 1.0f / 0.45f);
    case TransferFunction::Pq: {
        constexpr float m1 = 2610.0f / 16384.0f;
        constexpr float m2 = 2523.0f / 32.0f;
        constexpr float c1 = 3424.0f / 4096.0f;
        constexpr float c2 = 2413.0f / 128.0f;
        constexpr float c3 = 2392.0f / 128.0f;
        const float powered = std::pow(value, 1.0f / m2);
        return std::pow(std::max(powered - c1, 0.0f) / (c2 - c3 * powered), 1.0f / m1);
    }
    case TransferFunction::Hlg:
        return value <= 0.5f ? value * value / 3.0f
            : (std::exp((value - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f;
    case TransferFunction::Unspecified:
    case TransferFunction::Srgb:
    default: return srgbToLinear(value);
    }
}

float toneMap(const float linear, const ToneMapOperator operation,
              const float sourcePeak, const float outputPeak) noexcept
{
    float value = std::max(linear, 0.0f) * std::max(sourcePeak, 1.0f) /
        std::max(outputPeak, 1.0f);
    switch (operation) {
    case ToneMapOperator::Reinhard: value = value / (1.0f + value); break;
    case ToneMapOperator::Hable: {
        const auto curve = [](const float x) {
            return ((x * (0.15f * x + 0.05f) + 0.004f) /
                    (x * (0.15f * x + 0.50f) + 0.06f)) - 0.0666667f;
        };
        value = curve(value * 2.0f) / curve(11.2f);
        break;
    }
    case ToneMapOperator::Aces:
        value = std::clamp((value * (2.51f * value + 0.03f)) /
                           (value * (2.43f * value + 0.59f) + 0.14f), 0.0f, 1.0f);
        break;
    case ToneMapOperator::None: break;
    }
    return value;
}

void convertPrimaries(float& red, float& green, float& blue,
                      const ColorPrimaries primaries) noexcept
{
    if (primaries == ColorPrimaries::Bt2020) {
        const float r = 1.6605f * red - 0.5876f * green - 0.0728f * blue;
        const float g = -0.1246f * red + 1.1329f * green - 0.0083f * blue;
        const float b = -0.0182f * red - 0.1006f * green + 1.1187f * blue;
        red = r; green = g; blue = b;
    }
    else if (primaries == ColorPrimaries::DisplayP3) {
        const float r = 1.2249f * red - 0.2247f * green;
        const float g = -0.0420f * red + 1.0419f * green;
        const float b = -0.0197f * green + 1.0198f * blue;
        red = r; green = g; blue = b;
    }
}

}

FrameView ColorConverter::toSrgb(const FrameView& frame, const ColorOptions& options)
{
    const ColorMetadata color = frame.metadata.color;
    const bool hdr = color.transfer == TransferFunction::Pq || color.transfer == TransferFunction::Hlg;
    const bool alreadySrgb = color.transfer == TransferFunction::Srgb &&
        color.primaries == ColorPrimaries::Bt709 && color.range == ColorRange::Full;
    if (!options.convertToSrgb || alreadySrgb) {
        return frame;
    }

    const int channels = bytesPerPixel(frame.format);
    const std::size_t stride = static_cast<std::size_t>(frame.width) * channels;
    pixels.resize(stride * frame.height);
    const bool rgb = frame.format == PixelFormat::RGB24 || frame.format == PixelFormat::RGBA32;
    const float rangeScale = color.range == ColorRange::Limited ? 255.0f / 219.0f : 1.0f;
    const float rangeOffset = color.range == ColorRange::Limited ? 16.0f / 255.0f : 0.0f;
    const bool channelIndependent = !hdr &&
        (color.primaries == ColorPrimaries::Bt709 ||
         color.primaries == ColorPrimaries::Unspecified);
    std::array<std::uint8_t, 256> transferLookup{};
    if (channelIndependent) {
        for (int encoded = 0; encoded < 256; ++encoded) {
            const float normalized = (encoded / 255.0f - rangeOffset) * rangeScale;
            transferLookup[encoded] = static_cast<std::uint8_t>(std::clamp(
                std::lround(linearToSrgb(decodeTransfer(normalized, color.transfer)) * 255.0f),
                0l, 255l));
        }
    }

    for (int y = 0; y < frame.height; ++y) {
        const auto* source = frame.data + static_cast<std::ptrdiff_t>(y) * frame.stride;
        auto* destination = pixels.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < frame.width; ++x) {
            const int base = x * channels;
            if (channelIndependent) {
                destination[base] = transferLookup[source[base]];
                destination[base + 1] = transferLookup[source[base + 1]];
                destination[base + 2] = transferLookup[source[base + 2]];
                if (channels == 4) destination[base + 3] = source[base + 3];
                continue;
            }
            float red = (source[base + (rgb ? 0 : 2)] / 255.0f - rangeOffset) * rangeScale;
            float green = (source[base + 1] / 255.0f - rangeOffset) * rangeScale;
            float blue = (source[base + (rgb ? 2 : 0)] / 255.0f - rangeOffset) * rangeScale;
            red = decodeTransfer(red, color.transfer);
            green = decodeTransfer(green, color.transfer);
            blue = decodeTransfer(blue, color.transfer);
            convertPrimaries(red, green, blue, color.primaries);
            if (hdr) {
                red = toneMap(red, options.toneMap, color.masteringPeakNits, options.outputPeakNits);
                green = toneMap(green, options.toneMap, color.masteringPeakNits, options.outputPeakNits);
                blue = toneMap(blue, options.toneMap, color.masteringPeakNits, options.outputPeakNits);
            }
            const auto channel = [](const float value) {
                return static_cast<std::uint8_t>(std::clamp(std::lround(linearToSrgb(value) * 255.0f), 0l, 255l));
            };
            destination[base + (rgb ? 0 : 2)] = channel(red);
            destination[base + 1] = channel(green);
            destination[base + (rgb ? 2 : 0)] = channel(blue);
            if (channels == 4) {
                destination[base + 3] = source[base + 3];
            }
        }
    }
    FrameView converted = frame;
    converted.data = pixels.data();
    converted.stride = static_cast<std::ptrdiff_t>(stride);
    converted.metadata.color = {};
    return converted;
}

}