/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

extern "C" int FuzzerInputTest(const std::uint8_t* data, const std::size_t size)
{
    if (size < 6) return 0;
    std::array<std::uint8_t, 64 * 512> pixels{};
    const int width = data[0] % 65;
    const int height = data[1] % 65;
    const auto format = static_cast<rasterm::PixelFormat>(static_cast<std::int8_t>(data[2]));
    const std::ptrdiff_t stride = static_cast<std::int8_t>(data[3]) * 4;
    const rasterm::FrameView frame{ pixels.data(), width, height, stride, format };
    (void)frame.isValid();

    std::array<rasterm::RgbColor, 256> palette{};
    const std::size_t paletteSize = data[4];
    const rasterm::IndexedFrameView indexed{
        pixels.data(), width, height, stride, { palette.data(), paletteSize }
    };
    (void)indexed.isValid();
    if (indexed.isValid()) (void)indexed.hasValidIndices();
    return 0;
}