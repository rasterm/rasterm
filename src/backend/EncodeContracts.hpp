/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <encoder/EncoderMetrics.hpp>

#include <rasterm/Config.hpp>

#include <cstddef>
#include <string_view>

namespace rasterm {

enum class FramePreparation { FullFrame, Regional };
enum class BackendInvalidation { PaletteRegisters, All };

struct BackendConfiguration {
    QualityProfile quality = QualityProfile::Realtime;
    DitherMode realtimeDither = DitherMode::None;
    int adaptivePaletteLockFrames = 12;
    float sceneCutThreshold = 0.30f;
    EncoderTuning tuning{};
};

struct EncodeRequest {
    DamageRegion region{};
    std::size_t maximumBytes = 0;
    bool fullFrame = true;
};

struct EncodedUpdate {
    std::string_view bytes;
    DamageRegion region{};
    EncodeStageTimings stages{};
    int colorsUsed = 0;
    bool fullFrame = false;
    bool outputLimitExceeded = false;
};

}
