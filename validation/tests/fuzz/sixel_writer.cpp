/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelWriter.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size < 3) return 0;
    const int width = 1 + data[0] % 32;
    const int height = 1 + data[1] % 32;
    const int colors = 1 + data[2] % 64;
    std::vector<rasterm::RgbColor> palette(static_cast<std::size_t>(colors));
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(width) * height);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const std::uint8_t value = size > index + 3 ? data[index + 3] : 0;
        palette[index] = { value, static_cast<std::uint8_t>(value * 3),
                           static_cast<std::uint8_t>(value * 7) };
    }
    for (std::size_t index = 0; index < indices.size(); ++index) {
        indices[index] = size > index + 3 ? data[index + 3] % colors : 0;
    }
    std::string output;
    const std::size_t limit = size > 3 ? data[3] : 0;
    rasterm::SixelOutput checked(output, limit);
    rasterm::emitRasterAttributes(checked, width, height);
    rasterm::emitPalette(checked, { palette.data(), palette.size() });
    rasterm::emitIndexedFrame(checked, indices, width, height, colors);
    return 0;
}
