/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelEncoder.hpp>
#include <encoder/SixelPalette.hpp>
#include <encoder/SixelSimd.hpp>
#include <encoder/SixelWriter.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <windows.h>

namespace rasterm {
namespace {

std::atomic<int> avx2Mode{ -1 };

int clampChannel(const int value) noexcept
{
    return std::clamp(value, 0, 255);
}

int quantizeChannel(const std::uint8_t channel, const int levels) noexcept
{
    if (levels <= 1) {
        return 0;
    }
    return (channel * (levels - 1) + 127) / 255;
}

void applyFloydSteinberg(std::vector<std::uint8_t>& pixels, const int width, const int height,
                         const int levels)
{
    int quantizedValues[256];
    for (int value = 0; value < 256; ++value) {
        const int quantized = quantizeChannel(static_cast<std::uint8_t>(value), levels);
        quantizedValues[value] = levels > 1 ? quantized * 255 / (levels - 1) : 0;
    }

    const int stride = width * 3;
    for (int y = 0; y < height; ++y) {
        std::uint8_t* row = pixels.data() + static_cast<std::size_t>(y) * stride;
        std::uint8_t* next = y + 1 < height ? row + stride : nullptr;
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const int offset = x * 3 + channel;
                const int oldValue = row[offset];
                const int newValue = quantizedValues[oldValue];
                const int error = oldValue - newValue;
                row[offset] = static_cast<std::uint8_t>(newValue);

                const int error7 = error * 7 / 16;
                const int error5 = error * 5 / 16;
                const int error3 = error * 3 / 16;
                const int error1 = error - error7 - error5 - error3;
                if (x + 1 < width) {
                    row[offset + 3] = static_cast<std::uint8_t>(clampChannel(row[offset + 3] + error7));
                }
                if (next != nullptr) {
                    if (x > 0) {
                        next[offset - 3] = static_cast<std::uint8_t>(clampChannel(next[offset - 3] + error3));
                    }
                    next[offset] = static_cast<std::uint8_t>(clampChannel(next[offset] + error5));
                    if (x + 1 < width) {
                        next[offset + 3] = static_cast<std::uint8_t>(clampChannel(next[offset + 3] + error1));
                    }
                }
            }
        }
    }
}

void applyOrderedDither(std::vector<std::uint8_t>& pixels, const int width, const int height,
                        const int levels)
{
    static constexpr int bayer[4][4] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 },
    };
    const int stride = width * 3;
    const float step = levels > 1 ? 255.0f / (levels - 1) : 255.0f;
    for (int y = 0; y < height; ++y) {
        auto* row = pixels.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const float offset = ((bayer[y & 3][x & 3] + 0.5f) / 16.0f - 0.5f) * step;
            for (int channel = 0; channel < 3; ++channel) {
                row[x * 3 + channel] = static_cast<std::uint8_t>(
                    clampChannel(static_cast<int>(std::lround(row[x * 3 + channel] + offset))));
            }
        }
    }
}

}

bool sixelAvx2Supported() noexcept
{
    return IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE) != FALSE;
}

bool sixelAvx2Enabled() noexcept
{
    return avx2Mode.load(std::memory_order_relaxed) != 0 && sixelAvx2Supported();
}

void setSixelAvx2ModeForTesting(const int mode) noexcept
{
    avx2Mode.store(std::clamp(mode, -1, 0), std::memory_order_relaxed);
}

VideoSixelEncoder::VideoSixelEncoder(const SixelOptions& opts) : options(opts)
{
    options.maxColors = std::clamp(options.maxColors, 2, 256);
    options.maxFrameColors = std::clamp(options.maxFrameColors, 1, options.maxColors - 1);
    outputBuffer.reserve(1024 * 1024);
    paletteIndices.reserve(1920 * 1080);

    const int activeColorSlots = options.maxFrameColors + 1;
    analyzer = std::make_unique<FastColorAnalyzer>();
    colorMapper = std::make_unique<FastAdaptiveColorMapper>(activeColorSlots);
    fastColorMapper = std::make_unique<FastColorMapper>(activeColorSlots, options.paletteLevelsPerChannel);
}

std::string_view VideoSixelEncoder::encodeFrame(const FrameView& frame)
{
    if (!frame.isValid() || bytesPerPixel(frame.format) != 3) {
        return {};
    }
    const PixelLayout layout = frame.format == PixelFormat::RGB24 ? PixelLayout::RGB : PixelLayout::BGR;
    return encodeView(frame, layout);
}

std::string_view VideoSixelEncoder::encodeFrame(const IndexedFrameView& frame)
{
    return frame.isValid() ? encodeView(frame) : std::string_view{};
}

std::string_view VideoSixelEncoder::encodeRegion(const FrameView& frame, const DamageRegion& region)
{
    if (!frame.isValid() || bytesPerPixel(frame.format) != 3 || !region.isValid() ||
        region.x < 0 || region.y < 0 || region.x + region.width > frame.width ||
        region.y + region.height > frame.height) {
        return {};
    }

    const FrameView view{
        .data = frame.data + static_cast<std::ptrdiff_t>(region.y) * frame.stride + region.x * 3,
        .width = region.width,
        .height = region.height,
        .stride = frame.stride,
        .format = frame.format,
    };
    const PixelLayout layout = frame.format == PixelFormat::RGB24 ? PixelLayout::RGB : PixelLayout::BGR;
    return encodeView(view, layout);
}

std::string_view VideoSixelEncoder::encodeRegion(const IndexedFrameView& frame,
                                                 const DamageRegion& region)
{
    if (!frame.isValid() || !region.isValid() || region.x < 0 || region.y < 0 ||
        region.x + region.width > frame.width || region.y + region.height > frame.height) {
        return {};
    }

    return encodeView({
        .indices = frame.indices + static_cast<std::ptrdiff_t>(region.y) * frame.stride + region.x,
        .width = region.width,
        .height = region.height,
        .stride = frame.stride,
        .palette = frame.palette,
    });
}

std::string_view VideoSixelEncoder::encodeView(const FrameView& frame, const PixelLayout layout)
{
    FrameView source = frame;
    if (options.dither != DitherMode::None) {
        const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 3;
        workingBuffer.resize(rowBytes * frame.height);
        for (int row = 0; row < frame.height; ++row) {
            std::memcpy(workingBuffer.data() + static_cast<std::size_t>(row) * rowBytes,
                        frame.data + static_cast<std::ptrdiff_t>(row) * frame.stride,
                        rowBytes);
        }
        if (options.dither == DitherMode::FloydSteinberg) {
            applyFloydSteinberg(workingBuffer, frame.width, frame.height,
                                options.paletteLevelsPerChannel);
        }
        else {
            applyOrderedDither(workingBuffer, frame.width, frame.height,
                               options.paletteLevelsPerChannel);
        }
        source = {
            .data = workingBuffer.data(),
            .width = frame.width,
            .height = frame.height,
            .stride = static_cast<std::ptrdiff_t>(rowBytes),
            .format = frame.format,
        };
    }

    const int totalPixels = source.width * source.height;
    if (paletteIndices.size() < static_cast<std::size_t>(totalPixels)) {
        paletteIndices.resize(totalPixels);
    }

    int colorCount = 1;
    if (options.useAdaptivePalette) {
        analyzer->analyzeFrameFast(source, layout);
        float sceneDifference = 1.0f;
        if (colorMapper->hasHistogram) {
            sceneDifference = 0.0f;
            for (std::size_t index = 0; index < analyzer->luminanceHistogram.size(); ++index) {
                sceneDifference += std::abs(analyzer->luminanceHistogram[index] -
                                            colorMapper->previousHistogram[index]);
            }
            sceneDifference *= 0.5f;
        }
        if (!hasAdaptivePalette || paletteAge >= options.adaptivePaletteLockFrames ||
            sceneDifference >= options.sceneCutThreshold) {
            colorMapper->buildOptimalPalette(*analyzer);
            paletteAge = 0;
            hasAdaptivePalette = true;
        }
        else {
            ++paletteAge;
        }
        colorCount = colorMapper->nextColorNum;
        for (int y = 0; y < source.height; ++y) {
            const std::uint8_t* row = source.data + static_cast<std::ptrdiff_t>(y) * source.stride;
            for (int x = 0; x < source.width; ++x) {
                const std::uint8_t red = layout == PixelLayout::RGB ? row[x * 3] : row[x * 3 + 2];
                const std::uint8_t green = row[x * 3 + 1];
                const std::uint8_t blue = layout == PixelLayout::RGB ? row[x * 3 + 2] : row[x * 3];
                paletteIndices[y * source.width + x] =
                    static_cast<std::uint8_t>(colorMapper->getColorNumber(red, green, blue, false));
            }
        }
    }
    else {
        fastColorMapper->reset();
        const bool useAvx2 = source.width >= 64 && sixelAvx2Enabled();
        for (int y = 0; y < source.height; ++y) {
            const std::uint8_t* row = source.data + static_cast<std::ptrdiff_t>(y) * source.stride;
            std::uint8_t* mapped = paletteIndices.data() + static_cast<std::size_t>(y) * source.width;
            if (useAvx2) {
                mapPaletteRowAvx2(row, source.width, layout,
                                  fastColorMapper->rgbLookup32.data(), mapped);
                for (int x = 0; x < source.width; ++x) {
                    const int color = mapped[x];
                    if (!fastColorMapper->colorUsed[color]) {
                        fastColorMapper->colorUsed[color] = true;
                        ++fastColorMapper->usedColorCount;
                    }
                }
            }
            else {
                for (int x = 0; x < source.width; ++x) {
                    const std::uint8_t red = layout == PixelLayout::RGB ? row[x * 3] : row[x * 3 + 2];
                    const std::uint8_t green = row[x * 3 + 1];
                    const std::uint8_t blue = layout == PixelLayout::RGB ? row[x * 3 + 2] : row[x * 3];
                    mapped[x] = static_cast<std::uint8_t>(
                        fastColorMapper->getColorNumber(red, green, blue));
                }
            }
        }
        colorCount = fastColorMapper->nextColorNum;
    }

    outputBuffer.clear();
    outputBuffer.reserve(std::max(outputBuffer.capacity(), static_cast<std::size_t>(totalPixels / 3)));
    beginSixel(outputBuffer, options);
    emitRasterAttributes(outputBuffer, source.width, source.height);
    if (options.useAdaptivePalette) {
        emitPalette(outputBuffer, *colorMapper);
    }
    else {
        emitPalette(outputBuffer, *fastColorMapper);
    }
    emitIndexedFrame(outputBuffer, paletteIndices, source.width, source.height, colorCount);
    outputBuffer += "\x1b\\";
    lastColorCountValue = options.useAdaptivePalette ? colorCount - 1 : fastColorMapper->usedColorCount;
    return outputBuffer;
}

std::string_view VideoSixelEncoder::encodeView(const IndexedFrameView& frame)
{
    const std::size_t totalPixels = static_cast<std::size_t>(frame.width) * frame.height;
    const int registerOffset = frame.palette.size == 256 ? 0 : 1;
    paletteIndices.resize(totalPixels);
    for (int row = 0; row < frame.height; ++row) {
        const std::uint8_t* source = frame.indices + static_cast<std::ptrdiff_t>(row) * frame.stride;
        std::uint8_t* destination = paletteIndices.data() + static_cast<std::size_t>(row) * frame.width;
        for (int column = 0; column < frame.width; ++column) {
            destination[column] = static_cast<std::uint8_t>(source[column] + registerOffset);
        }
    }

    outputBuffer.clear();
    outputBuffer.reserve(std::max(outputBuffer.capacity(), totalPixels / 3));
    beginSixel(outputBuffer, options);
    emitRasterAttributes(outputBuffer, frame.width, frame.height);
    emitPalette(outputBuffer, frame.palette, registerOffset);
    emitIndexedFrame(outputBuffer, paletteIndices, frame.width, frame.height,
                       static_cast<int>(frame.palette.size + registerOffset));
    outputBuffer += "\x1b\\";
    lastColorCountValue = static_cast<int>(frame.palette.size);
    return outputBuffer;
}

VideoSixelEncoder::~VideoSixelEncoder() = default;

}