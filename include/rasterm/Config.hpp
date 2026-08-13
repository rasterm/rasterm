/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Diagnostics.hpp>
#include <rasterm/Capabilities.hpp>
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

struct TerminalOverrides {
    CapabilitySupport sixel = CapabilitySupport::Unknown;
    CapabilitySupport synchronizedOutput = CapabilitySupport::Unknown;
    TerminalGeometry geometry{};
};

struct EncoderTuning {
    bool persistPaletteRegisters = true;
    int paletteRefreshFrames = 120;
    std::size_t outputChunkBytes = 64 * 1024;
    int maximumThreads = 0;
    bool independentRegionQuantization = true;
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
    TerminalOverrides terminalOverrides{};
    EncoderTuning encoder{};
};

struct PresenterOptions {
    EngineOptions engine{};
    double maximumFramesPerSecond = 0.0;
};

}
