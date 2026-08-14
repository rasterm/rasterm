/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelSimd.hpp>

#include <immintrin.h>
#include <algorithm>

namespace rasterm {

void mapPaletteRowAvx2(const std::uint8_t* pixels, const int width, const PixelLayout layout,
                       const std::int32_t* lookup, std::uint8_t* output) noexcept
{
    alignas(32) std::int32_t offsets[8];
    alignas(32) std::int32_t mapped[8];
    const int sourceStride = pixelStride(layout);
    int x = 0;
    for (; x + 8 <= width; x += 8) {
        for (int lane = 0; lane < 8; ++lane) {
            const std::uint8_t* pixel = pixels + (x + lane) * sourceStride;
            const PixelChannels channels = readPixel(pixel, layout);
            const int red = channels.red;
            const int green = channels.green;
            const int blue = channels.blue;
            const int lr = (red * 31 + 127) / 255;
            const int lg = (green * 31 + 127) / 255;
            const int lb = (blue * 31 + 127) / 255;
            offsets[lane] = (lr * 32 + lg) * 32 + lb;
        }
        const __m256i indices = _mm256_load_si256(reinterpret_cast<const __m256i*>(offsets));
        const __m256i colors = _mm256_i32gather_epi32(lookup, indices, 4);
        _mm256_store_si256(reinterpret_cast<__m256i*>(mapped), colors);
        for (int lane = 0; lane < 8; ++lane) {
            output[x + lane] = static_cast<std::uint8_t>(mapped[lane]);
        }
    }
    for (; x < width; ++x) {
        const std::uint8_t* pixel = pixels + x * sourceStride;
        const PixelChannels channels = readPixel(pixel, layout);
        const int red = channels.red;
        const int green = channels.green;
        const int blue = channels.blue;
        const int lr = (red * 31 + 127) / 255;
        const int lg = (green * 31 + 127) / 255;
        const int lb = (blue * 31 + 127) / 255;
        output[x] = static_cast<std::uint8_t>(lookup[(lr * 32 + lg) * 32 + lb]);
    }
}

}
