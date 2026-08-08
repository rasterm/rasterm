/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <encoder/SixelPalette.hpp>

#include <rasterm/IndexedFrame.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace rasterm {

void beginSixel(std::string& output, const SixelOptions& options);
void emitRasterAttributes(std::string& output, int width, int height);
void emitPalette(std::string& output, const FastColorMapper& mapper);
void emitPalette(std::string& output, const FastAdaptiveColorMapper& mapper);
void emitPalette(std::string& output, PaletteView palette, int firstRegister = 1);
void emitColorBand(std::string& output, const std::vector<uint8_t>& paletteIndices,
                     int width, int height, int bandRow, int colorIndex);
void emitIndexedFrame(std::string& output, const std::vector<uint8_t>& paletteIndices,
                        int width, int height, int colorCount);

}