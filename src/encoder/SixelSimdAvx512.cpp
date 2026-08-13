/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelSimd.hpp>

#include <immintrin.h>

namespace rasterm {

void mapPaletteRowAvx512(const std::uint8_t* pixels, const int width,
                         const PixelLayout layout, const std::int32_t* lookup,
                         std::uint8_t* output) noexcept
{
    alignas(64) std::int32_t offsets[16];
    alignas(64) std::int32_t mapped[16];
    const int sourceStride = pixelStride(layout);
    int x = 0;
    for (; x + 16 <= width; x += 16) {
        for (int lane = 0; lane < 16; ++lane) {
            const PixelChannels channels = readPixel(
                pixels + (x + lane) * sourceStride, layout);
            const int lr = (static_cast<int>(channels.red) * 31 + 127) / 255;
            const int lg = (static_cast<int>(channels.green) * 31 + 127) / 255;
            const int lb = (static_cast<int>(channels.blue) * 31 + 127) / 255;
            offsets[lane] = (lr * 32 + lg) * 32 + lb;
        }
        const __m512i indices = _mm512_load_si512(offsets);
        const __m512i colors = _mm512_i32gather_epi32(indices, lookup, 4);
        _mm512_store_si512(mapped, colors);
        for (int lane = 0; lane < 16; ++lane) {
            output[x + lane] = static_cast<std::uint8_t>(mapped[lane]);
        }
    }
    for (; x < width; ++x) {
        const PixelChannels channels = readPixel(pixels + x * sourceStride, layout);
        const int lr = (static_cast<int>(channels.red) * 31 + 127) / 255;
        const int lg = (static_cast<int>(channels.green) * 31 + 127) / 255;
        const int lb = (static_cast<int>(channels.blue) * 31 + 127) / 255;
        output[x] = static_cast<std::uint8_t>(lookup[(lr * 32 + lg) * 32 + lb]);
    }
}

}
