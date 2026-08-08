/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

class DiscardSink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override { return true; }
    bool flush() noexcept override { return true; }
};

int main()
{
    constexpr int width = 64;
    constexpr int height = 48;
    std::vector<std::uint8_t> pixels(width * height * 3, 0);
    DiscardSink sink;
    rasterm::Engine engine;
    if (!engine.initialize({ .enableDirtyRegions = true, .output = &sink })) return 1;

    if (engine.renderFrame(pixels.data(), width, height, width * 3,
                           rasterm::PixelFormat::RGB24).error != rasterm::ErrorCode::None) {
        return 2;
    }

    pixels[0] = 255;
    const rasterm::DamageRect changed{ 0, 0, 8, 8 };
    rasterm::FrameView frame{
        pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24,
        { .damage = { &changed, 1, true } },
    };
    return engine.renderFrame(frame).error == rasterm::ErrorCode::None ? 0 : 3;
}