/* SPDX-License-Identifier: Apache-2.0 */

#include <damage/DamageTracker.hpp>
#include <render/Renderer.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        if (failWrites) return false;
        written += bytes.size();
        return true;
    }

    bool flush() noexcept override { return true; }

    std::size_t written = 0;
    bool failWrites = false;
};

}

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

    MemorySink sink;
    rasterm::RendererOptions rendererOptions{};
    rendererOptions.enableDirtyRegions = true;
    rendererOptions.preserveCursor = false;
    rendererOptions.useSynchronizedOutput = false;
    rendererOptions.cellPixels = { 10, 20 };
    rasterm::Renderer renderer(sink, rendererOptions);
    std::array<std::uint8_t, 80 * 40 * 3> surface{};
    rasterm::FrameView surfaceFrame{
        surface.data(), 80, 40, 80 * 3, rasterm::PixelFormat::RGB24
    };
    if (!renderer.render(surfaceFrame).usedFullFrame) return 10;

    constexpr std::array supplied{
        rasterm::DamageRect{ 1, 1, 5, 5 },
        rasterm::DamageRect{ 1, 1, 5, 5 },
        rasterm::DamageRect{ 10, 1, 5, 5 },
    };
    surface[0] = 0xff;
    surfaceFrame.metadata.damage = { supplied.data(), supplied.size(), true };
    const rasterm::RenderResult normalized = renderer.render(surfaceFrame);
    if (!normalized.rendered || normalized.usedFullFrame ||
        normalized.dirtyRegionCount != 1) return 11;

    sink.failWrites = true;
    if (renderer.render(surfaceFrame).rendered) return 12;
    sink.failWrites = false;
    const rasterm::RenderResult recovered = renderer.render(surfaceFrame);
    if (!recovered.rendered || !recovered.usedFullFrame) return 13;

    constexpr std::array nearbyDamage{
        rasterm::DamageRect{ 0, 0, 10, 20 },
        rasterm::DamageRect{ 20, 0, 10, 20 },
    };
    surfaceFrame.metadata.damage = { nearbyDamage.data(), nearbyDamage.size(), true };
    const rasterm::RenderResult nearby = renderer.render(surfaceFrame);
    if (!nearby.rendered || nearby.usedFullFrame || nearby.dirtyRegionCount != 1) return 14;

    constexpr std::array distantDamage{
        rasterm::DamageRect{ 0, 0, 10, 20 },
        rasterm::DamageRect{ 70, 0, 10, 20 },
    };
    surfaceFrame.metadata.damage = { distantDamage.data(), distantDamage.size(), true };
    const rasterm::RenderResult distant = renderer.render(surfaceFrame);
    if (!distant.rendered || distant.usedFullFrame || distant.dirtyRegionCount != 2 ||
        distant.outputBytes == 0 || distant.wireBytes <= distant.outputBytes ||
        distant.scratchBytes == 0) return 15;

    constexpr rasterm::DamageRect largeDamage{ 0, 0, 40, 40 };
    surfaceFrame.metadata.damage = { &largeDamage, 1, true };
    if (!renderer.render(surfaceFrame).usedFullFrame) return 16;
    return 0;
}
