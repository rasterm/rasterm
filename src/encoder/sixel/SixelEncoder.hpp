/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>
#include <encoder/EncoderMetrics.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>
#include <rasterm/Color.hpp>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/* SIXEL encoder for rasterm's packed and indexed frame views. packed pixels are
   quantized to a terminal compatible palette, indexed input preserves its exact palette.
   alpha is ignored, and output is encoded in six pixel vertical bands with SIXEL RLE. */

namespace rasterm {

    enum class PixelLayout { RGB, BGR, RGBA, BGRA, RGB565, XRGB1555, RGBA4444 };

    struct PixelChannels {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
    };

    [[nodiscard]] constexpr int pixelStride(const PixelLayout layout) noexcept
    {
        if (layout == PixelLayout::RGBA || layout == PixelLayout::BGRA) return 4;
        if (layout == PixelLayout::RGB || layout == PixelLayout::BGR) return 3;
        return 2;
    }

    [[nodiscard]] constexpr PixelLayout pixelLayout(const PixelFormat format) noexcept
    {
        switch (format) {
        case PixelFormat::RGB24: return PixelLayout::RGB;
        case PixelFormat::BGR24: return PixelLayout::BGR;
        case PixelFormat::RGBA32: return PixelLayout::RGBA;
        case PixelFormat::BGRA32: return PixelLayout::BGRA;
        case PixelFormat::RGB565: return PixelLayout::RGB565;
        case PixelFormat::XRGB1555: return PixelLayout::XRGB1555;
        case PixelFormat::RGBA4444: return PixelLayout::RGBA4444;
        }
        return PixelLayout::RGB;
    }

    [[nodiscard]] constexpr PixelChannels readPixel(const std::uint8_t* pixel,
                                                     const PixelLayout layout) noexcept
    {
        if (layout == PixelLayout::RGB || layout == PixelLayout::RGBA) {
            return { pixel[0], pixel[1], pixel[2] };
        }
        if (layout == PixelLayout::BGR || layout == PixelLayout::BGRA) {
            return { pixel[2], pixel[1], pixel[0] };
        }

        const auto packed = static_cast<std::uint16_t>(pixel[0]) |
            static_cast<std::uint16_t>(pixel[1] << 8U);
        if (layout == PixelLayout::RGB565) {
            const auto expand5 = [](const std::uint16_t value) {
                return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
            };
            const auto expand6 = [](const std::uint16_t value) {
                return static_cast<std::uint8_t>((value << 2U) | (value >> 4U));
            };
            return { expand5((packed >> 11U) & 0x1fU),
                     expand6((packed >> 5U) & 0x3fU),
                     expand5(packed & 0x1fU) };
        }
        if (layout == PixelLayout::XRGB1555) {
            const auto expand5 = [](const std::uint16_t value) {
                return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
            };
            return { expand5((packed >> 10U) & 0x1fU),
                     expand5((packed >> 5U) & 0x1fU),
                     expand5(packed & 0x1fU) };
        }
        const auto expand4 = [](const std::uint16_t value) {
            return static_cast<std::uint8_t>((value << 4U) | value);
        };
        return { expand4((packed >> 12U) & 0x0fU),
                 expand4((packed >> 8U) & 0x0fU),
                 expand4((packed >> 4U) & 0x0fU) };
    }

    struct SixelOptions {
        int ditherLevelsPerChannel = 6;             /* uniform grid strength for non palette aware dithering */
        bool transparentBackground = true;
        DitherMode dither = DitherMode::None;
        int maxColors = 256;                        /* maximum colors (matching Windows Terminal MAX_COLORS) */
        int maxFrameColors = 64;                    /* active palette colors per encoded frame (excluding background) */
        int pixelAspectRatio = 1;                   /* pixel aspect ratio (1:1 default, encoded with a compatible DCS macro parameter) */
        bool useAdaptivePalette = false;            /* use content aware adaptive palette for maximum quality */
        int adaptivePaletteLockFrames = 12;
        float sceneCutThreshold = 0.30f;
        bool persistPaletteRegisters = false;
        int paletteRefreshFrames = 120;
        int maximumThreads = 0;
        bool independentRegionQuantization = false;

        /* presets tuned for Windows Terminal's SIXEL decoder. */

        static SixelOptions forVideo()
        {
            SixelOptions result;
            result.ditherLevelsPerChannel = 5;
            result.dither = DitherMode::None;
            result.transparentBackground = false;
            result.maxColors = 256;
            result.maxFrameColors = 204;
            result.pixelAspectRatio = 1;
            result.useAdaptivePalette = false;
            return result;
        }

        static SixelOptions forImage()
        {
            SixelOptions result;
            result.ditherLevelsPerChannel = 8;
            result.dither = DitherMode::FloydSteinberg;
            result.transparentBackground = true;
            result.maxColors = 256;
            result.maxFrameColors = 255;
            result.pixelAspectRatio = 1;
            result.useAdaptivePalette = true;
            return result;
        }

        static SixelOptions forAdaptiveVideo()
        {
            SixelOptions result;
            result.ditherLevelsPerChannel = 8;
            result.dither = DitherMode::OrderedBayer4x4;
            result.transparentBackground = false;
            result.maxColors = 256;
            result.maxFrameColors = 128;
            result.pixelAspectRatio = 1;
            result.useAdaptivePalette = true;
            return result;
        }
    };

    /* internal encoder components. */

    struct ColorAnalyzer;
    struct AdaptivePaletteMapper;
    struct FixedPaletteMapper;
    class EncoderParallelExecutor;

    /* reusable SIXEL encoder. retained buffers avoid per frame allocations. */

    class SixelEncoder {
    public:
        SixelEncoder(const SixelOptions& options = SixelOptions::forVideo());

        /* the returned view remains valid until the next encode call on this encoder. */

        std::string_view encodeFrame(const FrameView& frame);
        std::string_view encodeFrame(const IndexedFrameView& frame);
        std::string_view encodeRegion(const FrameView& frame, const DamageRegion& region);
        std::string_view encodeRegion(const IndexedFrameView& frame, const DamageRegion& region);
        void prepareFrame(const FrameView& frame);
        void prepareFrame(const IndexedFrameView& frame);
        void prepareRegionalFrame(const FrameView& frame);
        void prepareRegionalFrame(const IndexedFrameView& frame) { prepareFrame(frame); }
        void prepareRegion(const FrameView& frame, const DamageRegion& region);
        void prepareRegion(const IndexedFrameView&, const DamageRegion&) {}
        void reset() noexcept;
        void invalidatePaletteRegisters() noexcept;

        void setOutputLimit(std::size_t maximumBytes) noexcept { outputLimit = maximumBytes; }
        [[nodiscard]] bool outputLimitExceeded() const noexcept { return limitExceeded; }
        [[nodiscard]] std::size_t outputCapacity() const noexcept { return outputBuffer.capacity(); }
        [[nodiscard]] std::size_t scratchCapacity() const noexcept;
        [[nodiscard]] EncodeStageTimings lastStageTimings() const noexcept { return stageTimings; }

        [[nodiscard]] std::size_t lastEncodedBytes() const noexcept { return outputBuffer.size(); }
        [[nodiscard]] int lastColorCount() const noexcept { return lastColorCountValue; }

        ~SixelEncoder();

    private:
        SixelOptions options;
        std::string outputBuffer;
        std::vector<uint8_t> paletteIndices;
        std::unique_ptr<ColorAnalyzer> analyzer;
        std::unique_ptr<AdaptivePaletteMapper> adaptivePaletteMapper;
        std::unique_ptr<FixedPaletteMapper> fixedPaletteMapper;
        std::unique_ptr<EncoderParallelExecutor> parallelExecutor;
        std::vector<uint8_t> workingBuffer;
        std::vector<int> currentDitherError;
        std::vector<int> nextDitherError;
        int lastColorCountValue = 0;
        int paletteAge = 0;
        bool hasAdaptivePalette = false;
        std::size_t outputLimit = 0;
        bool limitExceeded = false;
        bool adaptiveFramePrepared = false;
        bool paletteRegistersValid = false;
        std::uint64_t paletteSignature = 0;
        int framesSincePaletteRefresh = 0;
        EncodeStageTimings stageTimings{};
        std::chrono::nanoseconds pendingAnalysisDuration{};

        std::string_view encodeView(const FrameView& frame, PixelLayout layout);
        std::string_view encodeView(const IndexedFrameView& frame);
        void beginLogicalFrame() noexcept;
        [[nodiscard]] bool shouldEmitPalette(std::uint64_t signature) noexcept;
    };
}
