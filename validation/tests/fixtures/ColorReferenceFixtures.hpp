/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Color.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <array>
#include <string_view>

namespace rasterm::test::fixtures {

struct RgbTransferFixture {
    std::string_view name;
    RgbColor encoded;
    ColorMetadata metadata;
    RgbColor expectedSrgb;
    int tolerance;
};

inline constexpr std::array<RgbTransferFixture, 8> rgbTransfers{{
    { "sRGB full", { 64, 128, 192 }, {}, { 64, 128, 192 }, 0 },
    { "sRGB limited", { 16, 128, 235 },
      { .matrix = MatrixCoefficients::Bt601, .range = ColorRange::Limited },
      { 0, 130, 255 }, 1 },
    { "BT.601 SDR", { 64, 128, 192 },
      { .transfer = TransferFunction::Bt709, .matrix = MatrixCoefficients::Bt601 },
      { 79, 140, 198 }, 1 },
    { "BT.709 SDR", { 64, 128, 192 },
      { .transfer = TransferFunction::Bt709, .matrix = MatrixCoefficients::Bt709 },
      { 79, 140, 198 }, 1 },
    { "BT.2020 SDR", { 128, 128, 128 },
      { .primaries = ColorPrimaries::Bt2020, .transfer = TransferFunction::Bt709,
        .matrix = MatrixCoefficients::Bt2020NonConstant },
      { 140, 140, 140 }, 1 },
    { "linear SDR", { 64, 128, 192 },
      { .transfer = TransferFunction::Linear }, { 137, 188, 225 }, 1 },
    { "PQ HDR1000", { 64, 128, 192 },
      { .primaries = ColorPrimaries::Bt2020, .transfer = TransferFunction::Pq,
        .matrix = MatrixCoefficients::Bt2020NonConstant, .masteringPeakNits = 1000.0f },
      { 0, 57, 210 }, 4 },
    { "HLG HDR1000", { 64, 128, 192 },
      { .primaries = ColorPrimaries::Bt2020, .transfer = TransferFunction::Hlg,
        .matrix = MatrixCoefficients::Bt2020NonConstant, .masteringPeakNits = 1000.0f },
      { 0, 200, 240 }, 4 },
}};

inline constexpr std::array<RgbColor, 24> colorChecker{{
    {115,82,68},{194,150,130},{98,122,157},{87,108,67},{133,128,177},{103,189,170},
    {214,126,44},{80,91,166},{193,90,99},{94,60,108},{157,188,64},{224,163,46},
    {56,61,150},{70,148,73},{175,54,60},{231,199,31},{187,86,149},{8,133,161},
    {243,243,242},{200,200,200},{160,160,160},{122,122,121},{85,85,85},{52,52,52},
}};

inline constexpr std::array<RgbColor, 64> nesPalette{{
    {84,84,84},{0,30,116},{8,16,144},{48,0,136},{68,0,100},{92,0,48},{84,4,0},{60,24,0},
    {32,42,0},{8,58,0},{0,64,0},{0,60,0},{0,50,60},{0,0,0},{0,0,0},{0,0,0},
    {152,150,152},{8,76,196},{48,50,236},{92,30,228},{136,20,176},{160,20,100},{152,34,32},{120,60,0},
    {84,90,0},{40,114,0},{8,124,0},{0,118,40},{0,102,120},{0,0,0},{0,0,0},{0,0,0},
    {236,238,236},{76,154,236},{120,124,236},{176,98,236},{228,84,236},{236,88,180},{236,106,100},{212,136,32},
    {160,170,0},{116,196,0},{76,208,32},{56,204,108},{56,180,204},{60,60,60},{0,0,0},{0,0,0},
    {236,238,236},{168,204,236},{188,188,236},{212,178,236},{236,174,236},{236,174,212},{236,180,176},{228,196,144},
    {204,210,120},{180,222,120},{168,226,144},{152,226,180},{160,214,228},{160,162,160},{0,0,0},{0,0,0},
}};

}