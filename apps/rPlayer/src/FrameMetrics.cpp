/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/FrameMetrics.hpp>

namespace rasterm::rPlayer {

void FrameMetrics::record(const FrameMetricSample& sample)
{
    if (sample.dropped) {
        ++droppedFrames;
        return;
    }

    ++renderedFrames;
    dirtyFrames += sample.dirtyRegions > 0 ? 1 : 0;
    decodeUs += static_cast<std::uint64_t>(sample.decode.count());
    resizeUs += static_cast<std::uint64_t>(sample.resize.count());
    encodeUs += static_cast<std::uint64_t>(sample.encode.count());
    presentUs += static_cast<std::uint64_t>(sample.present.count());
    outputBytes += sample.outputBytes;
    colorsUsed += sample.colorsUsed;
    outputWidth = sample.width;
    outputHeight = sample.height;
    terminalWriteP95Milliseconds = sample.terminalWriteP95Milliseconds;
    terminalWriteP99Milliseconds = sample.terminalWriteP99Milliseconds;
    outputFailures = sample.outputFailures;
    backpressureEvents = sample.backpressureEvents;
    payloadLimitDrops = sample.payloadLimitDrops;
}

FrameMetricsSnapshot FrameMetrics::snapshot(const std::chrono::steady_clock::duration elapsed) const
{
    if (renderedFrames == 0) {
        return { .droppedFrames = droppedFrames };
    }

    const double count = static_cast<double>(renderedFrames);
    const double elapsedSeconds = std::chrono::duration<double>(elapsed).count();
    return {
        .framesPerSecond = elapsedSeconds > 0.0 ? count / elapsedSeconds : 0.0,
        .averageDecodeMs = decodeUs / count / 1000.0,
        .averageResizeMs = resizeUs / count / 1000.0,
        .averageEncodeMs = encodeUs / count / 1000.0,
        .averagePresentMs = presentUs / count / 1000.0,
        .averageBytesPerFrame = outputBytes / count,
        .averageColorsPerFrame = colorsUsed / count,
        .payloadBytesPerSecond = elapsedSeconds > 0.0 ? outputBytes / elapsedSeconds : 0.0,
        .terminalWriteP95Milliseconds = terminalWriteP95Milliseconds,
        .terminalWriteP99Milliseconds = terminalWriteP99Milliseconds,
        .outputFailures = outputFailures,
        .backpressureEvents = backpressureEvents,
        .payloadLimitDrops = payloadLimitDrops,
        .outputWidth = outputWidth,
        .outputHeight = outputHeight,
        .renderedFrames = renderedFrames,
        .droppedFrames = droppedFrames,
        .dirtyFrames = dirtyFrames,
    };
}

void FrameMetrics::reset()
{
    *this = {};
}

}