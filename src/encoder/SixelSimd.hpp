/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/SixelEncoder.hpp>

#include <cstdint>

namespace rasterm {

void mapPaletteRowAvx2(const std::uint8_t* pixels, int width, PixelLayout layout,
                       const std::int32_t* lookup, std::uint8_t* output) noexcept;
void mapPaletteRowAvx512(const std::uint8_t* pixels, int width, PixelLayout layout,
                         const std::int32_t* lookup, std::uint8_t* output) noexcept;

[[nodiscard]] bool sixelAvx2Supported() noexcept;
[[nodiscard]] bool sixelAvx2Enabled() noexcept;
[[nodiscard]] bool sixelAvx512Supported() noexcept;
[[nodiscard]] bool sixelAvx512Enabled() noexcept;

/* internal test hook: -1 restores automatic dispatch, 0 forces scalar. */

void setSixelAvx2ModeForTesting(int mode) noexcept;

}
