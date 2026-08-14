/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/sixel/SixelPalette.hpp>

#include <rasterm/IndexedFrame.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rasterm {

class SixelOutput {
public:
    explicit SixelOutput(std::string& storage, std::size_t maximumBytes = 0) noexcept :
        storage(storage), maximumBytes(maximumBytes) {}

    bool append(std::string_view bytes);
    bool append(char byte);
    bool appendNumber(std::uint32_t value);
    [[nodiscard]] bool exceeded() const noexcept { return limitExceeded; }

private:
    std::string& storage;
    std::size_t maximumBytes = 0;
    bool limitExceeded = false;
};

void beginSixel(SixelOutput& output, const SixelOptions& options);
void emitRasterAttributes(SixelOutput& output, int width, int height);
void emitPalette(SixelOutput& output, const FixedPaletteMapper& mapper);
void emitFullPalette(SixelOutput& output, const FixedPaletteMapper& mapper);
void emitPalette(SixelOutput& output, const AdaptivePaletteMapper& mapper);
void emitPalette(SixelOutput& output, PaletteView palette, int firstRegister = 1);
void emitIndexedFrame(SixelOutput& output, const std::vector<uint8_t>& paletteIndices,
                      int width, int height, int colorCount);

void beginSixel(std::string& output, const SixelOptions& options);
void emitRasterAttributes(std::string& output, int width, int height);
void emitPalette(std::string& output, const FixedPaletteMapper& mapper);
void emitPalette(std::string& output, const AdaptivePaletteMapper& mapper);
void emitPalette(std::string& output, PaletteView palette, int firstRegister = 1);
void emitIndexedFrame(std::string& output, const std::vector<uint8_t>& paletteIndices,
                      int width, int height, int colorCount);

}
