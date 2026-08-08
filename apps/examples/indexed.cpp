/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstdint>

int main()
{
    constexpr int width = 4;
    constexpr int height = 4;
    std::array<std::uint8_t, width * height> indices{
        0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 0, 1, 3, 0, 1, 2,
    };
    constexpr std::array palette{
        rasterm::RgbColor{ 0, 0, 0 }, rasterm::RgbColor{ 255, 0, 0 },
        rasterm::RgbColor{ 0, 255, 0 }, rasterm::RgbColor{ 0, 0, 255 },
    };

    rasterm::Engine engine;
    if (!engine.initialize()) return 1;
    const rasterm::RenderStats stats = engine.renderFrame({
        indices.data(), width, height, width,
        { palette.data(), palette.size() },
    });
    return stats.error == rasterm::ErrorCode::None ? 0 : 2;
}