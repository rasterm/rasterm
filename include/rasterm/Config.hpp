/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Diagnostics.hpp>
#include <rasterm/Color.hpp>
#include <rasterm/Events.hpp>

#include <cstddef>

namespace rasterm {

class OutputSink;

enum class QualityProfile : std::int32_t {
    Realtime = 0,
    AdaptiveVideo = 1,
    HighQuality = 2,
};

struct EngineOptions {
    QualityProfile quality = QualityProfile::Realtime;
    bool useAlternateScreen = false;
    bool preserveCursor = true;
    bool enableDirtyRegions = false;
    bool requireSixelSupport = false;
    bool useSynchronizedOutput = true;
    std::size_t maximumOutputBytes = 0;
    double backpressureThresholdMilliseconds = 12.0;
    ColorOptions color{};
    DiagnosticOptions diagnostics{};
    EventOptions events{};
    OutputSink* output = nullptr;
};

struct PresenterOptions {
    EngineOptions engine{};
    double maximumFramesPerSecond = 0.0;
};

}