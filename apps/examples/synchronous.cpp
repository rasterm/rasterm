/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <cstdint>
#include <vector>

int main()
{
    constexpr int width = 320;
    constexpr int height = 180;
    std::vector<std::uint8_t> pixels(width * height * 4, 255);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<std::uint8_t>(x * 255 / width);
            pixels[offset + 1] = static_cast<std::uint8_t>(y * 255 / height);
            pixels[offset + 2] = 160;
        }
    }

    rasterm::Engine engine;
    const rasterm::Status status = engine.initialize({ .useAlternateScreen = true });
    if (!status) return 1;

    const rasterm::RenderStats stats = engine.renderFrame(
        pixels.data(), width, height, width * 4, rasterm::PixelFormat::RGBA32);
    return stats.error == rasterm::ErrorCode::None ? 0 : 2;
}