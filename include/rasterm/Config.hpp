/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Diagnostics.hpp>
#include <rasterm/Capabilities.hpp>
#include <rasterm/Color.hpp>
#include <rasterm/Events.hpp>
#include <rasterm/defaults.h>

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
    bool persistPaletteRegisters = RASTERM_DEFAULT_PERSIST_PALETTE_REGISTERS != 0;
    int paletteRefreshFrames = RASTERM_DEFAULT_PALETTE_REFRESH_FRAMES;
    std::size_t outputChunkBytes = RASTERM_DEFAULT_OUTPUT_CHUNK_BYTES;
    int maximumThreads = RASTERM_DEFAULT_MAXIMUM_ENCODER_THREADS;
    bool independentRegionQuantization =
        RASTERM_DEFAULT_INDEPENDENT_REGION_QUANTIZATION != 0;
};

struct EngineOptions {
    QualityProfile quality = static_cast<QualityProfile>(RASTERM_DEFAULT_QUALITY);
    bool useAlternateScreen = RASTERM_DEFAULT_USE_ALTERNATE_SCREEN != 0;
    bool preserveCursor = RASTERM_DEFAULT_PRESERVE_CURSOR != 0;
    bool enableDirtyRegions = RASTERM_DEFAULT_ENABLE_DIRTY_REGIONS != 0;
    bool requireSixelSupport = RASTERM_DEFAULT_REQUIRE_SIXEL_SUPPORT != 0;
    bool useSynchronizedOutput = RASTERM_DEFAULT_USE_SYNCHRONIZED_OUTPUT != 0;
    std::size_t maximumOutputBytes = RASTERM_DEFAULT_MAXIMUM_OUTPUT_BYTES;
    double backpressureThresholdMilliseconds = RASTERM_DEFAULT_BACKPRESSURE_MILLISECONDS;
    ColorOptions color{};
    DiagnosticOptions diagnostics{};
    EventOptions events{};
    OutputSink* output = nullptr;
    TerminalOverrides terminalOverrides{};
    EncoderTuning encoder{};
};

struct PresenterOptions {
    EngineOptions engine{};
    double maximumFramesPerSecond = RASTERM_DEFAULT_MAXIMUM_FRAMES_PER_SECOND;
};

}
