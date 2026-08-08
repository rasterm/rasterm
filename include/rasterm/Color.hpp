/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>

namespace rasterm {

enum class ColorPrimaries : std::int32_t { Unspecified = 0, Bt709 = 1, Bt2020 = 2, DisplayP3 = 3 };
enum class TransferFunction : std::int32_t { Unspecified = 0, Srgb = 1, Linear = 2, Bt709 = 3, Gamma22 = 4, Pq = 5, Hlg = 6 };
enum class MatrixCoefficients : std::int32_t { Unspecified = 0, Identity = 1, Bt601 = 2, Bt709 = 3, Bt2020NonConstant = 4 };
enum class ColorRange : std::int32_t { Unspecified = 0, Limited = 1, Full = 2 };
enum class ToneMapOperator : std::int32_t { None = 0, Reinhard = 1, Hable = 2, Aces = 3 };
enum class DitherMode : std::int32_t { None = 0, OrderedBayer4x4 = 1, FloydSteinberg = 2 };

struct ColorMetadata {
    ColorPrimaries primaries = ColorPrimaries::Bt709;
    TransferFunction transfer = TransferFunction::Srgb;
    MatrixCoefficients matrix = MatrixCoefficients::Identity;
    ColorRange range = ColorRange::Full;
    float referenceWhiteNits = 203.0f;
    float masteringPeakNits = 1000.0f;
};

struct ColorOptions {
    bool convertToSrgb = true;
    ToneMapOperator toneMap = ToneMapOperator::Aces;
    float outputPeakNits = 203.0f;
    DitherMode realtimeDither = DitherMode::None;
    int adaptivePaletteLockFrames = 12;
    float sceneCutThreshold = 0.30f;
};

}