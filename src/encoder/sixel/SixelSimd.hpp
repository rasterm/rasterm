/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/sixel/SixelEncoder.hpp>

#include <cstdint>

namespace rasterm {

void mapPaletteRowAvx2(const std::uint8_t* pixels, int width, PixelLayout layout,
                       const std::int32_t* lookup, std::uint8_t* output) noexcept;
void mapPaletteRowAvx512(const std::uint8_t* pixels, int width, PixelLayout layout,
                         const std::int32_t* lookup, std::uint8_t* output) noexcept;

[[nodiscard]] bool sixelAvx2Supported() noexcept;
[[nodiscard]] bool sixelAvx2Enabled(int width = 0) noexcept;
[[nodiscard]] bool sixelAvx512Supported() noexcept;
[[nodiscard]] bool sixelAvx512Enabled(int width = 0) noexcept;

/* internal benchmark/test hook: -1 selects automatic dispatch, 0 scalar,
   1 AVX2, and 2 AVX-512. unsupported forced modes fall back to scalar. */

void setSixelSimdModeForTesting(int mode) noexcept;

}
