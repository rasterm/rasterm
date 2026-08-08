/* SPDX-License-Identifier: Apache-2.0 */

#include <damage/DamageTracker.hpp>

#include <array>
#include <cstdint>
#include <vector>

int main()
{
    rasterm::DamageTracker tracker({ .tileWidth = 4, .tileHeight = 3,
                                     .fullFrameThreshold = 1.0f });
    constexpr int width = 10;
    constexpr int height = 7;
    constexpr int stride = width * 3 + 5;
    std::vector<std::uint8_t> pixels(stride * height);
    rasterm::FrameView frame{
        pixels.data(), width, height, stride, rasterm::PixelFormat::RGB24
    };

    const auto first = tracker.compareAndUpdate(frame);
    if (!first.hasChanges || !first.isFullFrame || first.changedAreaRatio != 1.0f) {
        return 1;
    }
    if (tracker.compareAndUpdate(frame).hasChanges) {
        return 2;
    }

    pixels[(height - 1) * stride + (width - 1) * 3] = 1;
    const auto edge = tracker.compareAndUpdate(frame);
    if (!edge.hasChanges || edge.isFullFrame || edge.regions.size() != 1 ||
        edge.regions[0].x != 8 || edge.regions[0].y != 6 ||
        edge.regions[0].width != 2 || edge.regions[0].height != 1) {
        return 3;
    }

    tracker.reset();
    tracker.compareAndUpdate(frame);
    pixels[0] = 2;
    pixels[3 * stride] = 2;
    const auto merged = tracker.compareAndUpdate(frame);
    if (merged.regions.size() != 1 || merged.regions[0].x != 0 ||
        merged.regions[0].y != 0 || merged.regions[0].width != 4 ||
        merged.regions[0].height != 6) {
        return 4;
    }

    rasterm::DamageTracker threshold({ .tileWidth = 4, .tileHeight = 3,
                                       .fullFrameThreshold = 0.1f });
    threshold.compareAndUpdate(frame);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            pixels[y * stride + x * 3] ^= 0xFF;
        }
    }
    if (!threshold.compareAndUpdate(frame).isFullFrame) {
        return 5;
    }

    std::vector<std::uint8_t> larger((width + 1) * height * 3);
    const rasterm::FrameView largerFrame{
        larger.data(), width + 1, height, (width + 1) * 3, rasterm::PixelFormat::RGB24
    };
    if (!threshold.compareAndUpdate(largerFrame).isFullFrame) {
        return 6;
    }

    std::array<rasterm::RgbColor, 2> palette{{ { 0, 0, 0 }, { 255, 255, 255 } }};
    std::array<std::uint8_t, 16> indices{};
    const rasterm::IndexedFrameView indexed{
        indices.data(), 4, 4, 4, { palette.data(), palette.size() }
    };
    tracker.reset();
    if (!tracker.compareAndUpdate(indexed).isFullFrame ||
        tracker.compareAndUpdate(indexed).hasChanges) {
        return 7;
    }
    indices[15] = 1;
    const auto indexedEdge = tracker.compareAndUpdate(indexed);
    if (!indexedEdge.hasChanges || indexedEdge.isFullFrame) {
        return 8;
    }
    palette[1] = { 255, 0, 0 };
    const auto paletteChange = tracker.compareAndUpdate(indexed);
    if (!paletteChange.hasChanges || !paletteChange.isFullFrame) {
        return 9;
    }
    return 0;
}