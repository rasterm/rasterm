/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

class StateSink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override
    {
        if (failNextWrite) {
            failNextWrite = false;
            return false;
        }
        return true;
    }
    bool flush() noexcept override { return true; }

    bool failNextWrite = false;
};

std::uint32_t next(std::uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return state;
}

}

int main()
{
    constexpr int width = 60;
    constexpr int height = 40;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(width) * height);
    std::array<rasterm::RgbColor, 4> palette{{
        { 0, 0, 0 }, { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 },
    }};

    StateSink sink;
    rasterm::EngineOptions options;
    options.output = &sink;
    options.enableDirtyRegions = true;
    options.preserveCursor = false;
    options.useSynchronizedOutput = false;
    options.terminalOverrides.geometry = { 6, 2, width, height, 10, 20 };
    rasterm::Engine engine;
    if (!engine.initialize(options)) return 1;

    std::uint32_t random = 0x52415354u;
    bool requireFullFrame = true;
    bool useIndexed = false;
    for (int iteration = 0; iteration < 500; ++iteration) {
        const std::uint32_t operation = next(random);
        if (operation % 29 == 0) {
            engine.reset();
            requireFullFrame = true;
            continue;
        }
        if (operation % 37 == 0) {
            if (!engine.clear()) return 2;
            requireFullFrame = true;
            continue;
        }
        if (operation % 17 == 0) sink.failNextWrite = true;
        if (operation % 23 == 0) {
            useIndexed = !useIndexed;
            requireFullFrame = true;
        }

        const int x = static_cast<int>(next(random) % width);
        const int y = static_cast<int>(next(random) % height);
        rasterm::DamageRect damage{ x, y, 1, 1 };
        rasterm::FrameMetadata metadata;
        if ((operation & 1u) != 0) metadata.damage = { &damage, 1, true };

        rasterm::RenderStats rendered;
        if (useIndexed) {
            indices[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint8_t>((indices[static_cast<std::size_t>(y) * width + x] + 1) %
                                          palette.size());
            rendered = engine.renderFrame({
                indices.data(), width, height, width,
                { palette.data(), palette.size() }, metadata,
            });
        }
        else {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 3;
            pixels[offset] ^= 0xff;
            rendered = engine.renderFrame({
                pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24, metadata,
            });
        }

        if (rendered.error != rasterm::ErrorCode::None) {
            if (rendered.error != rasterm::ErrorCode::OutputWriteFailed) return 3;
            requireFullFrame = true;
            continue;
        }
        if (!rendered.rendered) return 4;
        if (requireFullFrame && !rendered.fullFrame) return 5;
        requireFullFrame = false;
    }
    engine.shutdown();
    return 0;
}
