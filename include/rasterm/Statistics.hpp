/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Error.hpp>

#include <cstddef>
#include <cstdint>

namespace rasterm {

struct RenderStats {
    ErrorCode error = ErrorCode::None;
    bool rendered = false;
    bool fullFrame = false;
    std::size_t payloadBytes = 0;
    std::uint32_t dirtyRegions = 0;
    int width = 0;
    int height = 0;
    int colorsUsed = 0;
    double encodeMilliseconds = 0.0;
    double presentMilliseconds = 0.0;
    double framesPerSecond = 0.0;
    double payloadBytesPerSecond = 0.0;
    double terminalWriteP95Milliseconds = 0.0;
    double terminalWriteP99Milliseconds = 0.0;
    std::uint64_t outputFailures = 0;
    std::uint64_t backpressureEvents = 0;
    std::uint64_t payloadLimitDrops = 0;
    std::size_t wireBytes = 0;
    std::size_t scratchBytes = 0;
    std::size_t outputCapacityBytes = 0;
    double validationMilliseconds = 0.0;
    double conversionMilliseconds = 0.0;
};

struct PresenterStats {
    std::uint64_t submittedFrames = 0;
    std::uint64_t presentedFrames = 0;
    std::uint64_t replacedFrames = 0;
    RenderStats latestRender{};
    std::uint64_t unchangedFrames = 0;
    std::uint64_t failedFrames = 0;
    std::uint64_t rejectedFrames = 0;
    std::uint64_t cancelledFrames = 0;
};

struct TerminalGeometry {
    int columns = 0;
    int rows = 0;
    int pixelWidth = 0;
    int pixelHeight = 0;
    int cellPixelWidth = 0;
    int cellPixelHeight = 0;
};

}
