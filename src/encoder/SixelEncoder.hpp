/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>
#include <rasterm/Color.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/* SIXEL encoder for 24-bit RGB frames.
   quantizes RGB frames to a terminal compatible indexed palette.
   encodes per 6pixel vertical bands, per color, with simple RLE.
   returns a complete SIXEL DCS string (ESC P q ... ESC \\). */

namespace rasterm {

    enum class PixelLayout { RGB, BGR };

    enum class QualityPreset {
        Fast,       /* stable perceptual palette, no dithering, INTER_LINEAR (for video) */
        Balanced,   /* 6x6x6 colors, dithering, INTER_LINEAR (good compromise) */
        High        /* 8x8x8 colors, dithering, INTER_CUBIC (for images) */
    };

    struct SixelOptions {
        int paletteLevelsPerChannel = 6;            /* 6x6x6 = 216 + background (balanced default) */
        bool clampNoUpscale = true;                 /* dont upscale beyond source size */
        bool transparentBackground = true;          /* use transparent background for better video playback */
        DitherMode dither = DitherMode::None;
        bool useHighQualityResize = false;          /* use bicubic instead of nearest neighbor */
        int maxColors = 256;                        /* maximum colors (matching Windows Terminal MAX_COLORS) */
        int maxFrameColors = 64;                    /* active palette colors per encoded frame (excluding background) */
        int pixelAspectRatio = 1;                   /* pixel aspect ratio (1:1 default, encoded with a compatible DCS macro parameter) */
        bool useAdaptivePalette = false;            /* use content aware adaptive palette for maximum quality */
        int adaptivePaletteLockFrames = 12;
        float sceneCutThreshold = 0.30f;
        PixelLayout inputLayout = PixelLayout::RGB; /* input channel order */

        /* preset convenience functions optimized for Windows Terminal compatibility */

        static SixelOptions ForVideo() {
            SixelOptions opts;
            opts.paletteLevelsPerChannel = 5;
            opts.dither = DitherMode::None;
            opts.transparentBackground = false; /* solid background for video */
            opts.maxColors = 256;
            opts.maxFrameColors = 204;
            opts.pixelAspectRatio = 1;          /* 1:1 for smooth video playback */
            opts.useAdaptivePalette = false;    /* fast fixed palette for video performance */
            opts.inputLayout = PixelLayout::RGB;
            return opts;
        }

        static SixelOptions ForRealtimeVideo() {
            return ForVideo();
        }

        static SixelOptions ForImage() {
            SixelOptions opts;
            opts.paletteLevelsPerChannel = 8;
            opts.dither = DitherMode::FloydSteinberg;
            opts.transparentBackground = true; /* transparent background for images */
            opts.maxColors = 256;
            opts.maxFrameColors = 128;
            opts.pixelAspectRatio = 1;         /* 1:1 for crisp image display */
            opts.useAdaptivePalette = true;    /* adaptive palette for maximum image quality */
            opts.inputLayout = PixelLayout::RGB;
            return opts;
        }

        /* video preset optimized for decoder compatibility */

        static SixelOptions ForHighQualityVideo() {
            SixelOptions opts;
            opts.paletteLevelsPerChannel = 8;
            opts.dither = DitherMode::OrderedBayer4x4;
            opts.transparentBackground = false;
            opts.maxColors = 256;
            opts.maxFrameColors = 128;
            opts.pixelAspectRatio = 1;
            opts.useAdaptivePalette = true;   /* keep adaptive palette for perfect colors */
            opts.inputLayout = PixelLayout::RGB;
            return opts;
        }
    };

    /* forward declarations for internal classes */

    struct FastColorAnalyzer;
    struct FastAdaptiveColorMapper;
    struct FastColorMapper;

    /* reusable encoder for video streams (avoids allocations) */

    class VideoSixelEncoder {
    public:
        VideoSixelEncoder(const SixelOptions& opts = SixelOptions::ForRealtimeVideo());

        /* The returned view remains valid until the next encode call on this encoder. */

        std::string_view encodeFrame(const FrameView& frame);
        std::string_view encodeFrame(const IndexedFrameView& frame);
        std::string_view encodeRegion(const FrameView& frame, const DamageRegion& region);
        std::string_view encodeRegion(const IndexedFrameView& frame, const DamageRegion& region);

        [[nodiscard]] std::size_t lastEncodedBytes() const noexcept { return outputBuffer.size(); }
        [[nodiscard]] int lastColorCount() const noexcept { return lastColorCountValue; }

        ~VideoSixelEncoder();

    private:
        SixelOptions options;
        std::string outputBuffer;
        std::vector<uint8_t> paletteIndices;
        std::unique_ptr<FastColorAnalyzer> analyzer;
        std::unique_ptr<FastAdaptiveColorMapper> colorMapper;
        std::unique_ptr<FastColorMapper> fastColorMapper;
        std::vector<uint8_t> workingBuffer;
        int lastColorCountValue = 0;
        int paletteAge = 0;
        bool hasAdaptivePalette = false;

        std::string_view encodeView(const FrameView& frame, PixelLayout layout);
        std::string_view encodeView(const IndexedFrameView& frame);
    };

    struct TerminalCellPixels { int cellWidth = 10; int cellHeight = 20; };

}