/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace rasterm::rPlayer {

struct FrameMetricSample {
    std::chrono::microseconds decode{};
    std::chrono::microseconds resize{};
    std::chrono::microseconds encode{};
    std::chrono::microseconds present{};
    std::size_t outputBytes = 0;
    std::uint32_t dirtyRegions = 0;
    int width = 0;
    int height = 0;
    int colorsUsed = 0;
    double terminalWriteP95Milliseconds = 0.0;
    double terminalWriteP99Milliseconds = 0.0;
    std::uint64_t outputFailures = 0;
    std::uint64_t backpressureEvents = 0;
    std::uint64_t payloadLimitDrops = 0;
    bool dropped = false;
};

struct FrameMetricsSnapshot {
    double framesPerSecond = 0.0;
    double averageDecodeMs = 0.0;
    double averageResizeMs = 0.0;
    double averageEncodeMs = 0.0;
    double averagePresentMs = 0.0;
    double averageBytesPerFrame = 0.0;
    double averageColorsPerFrame = 0.0;
    double payloadBytesPerSecond = 0.0;
    double terminalWriteP95Milliseconds = 0.0;
    double terminalWriteP99Milliseconds = 0.0;
    std::uint64_t outputFailures = 0;
    std::uint64_t backpressureEvents = 0;
    std::uint64_t payloadLimitDrops = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    std::uint64_t renderedFrames = 0;
    std::uint64_t droppedFrames = 0;
    std::uint64_t dirtyFrames = 0;
};

class FrameMetrics {
public:
    void record(const FrameMetricSample& sample);
    [[nodiscard]] FrameMetricsSnapshot snapshot(std::chrono::steady_clock::duration elapsed) const;
    void reset();

private:
    std::uint64_t renderedFrames = 0;
    std::uint64_t droppedFrames = 0;
    std::uint64_t dirtyFrames = 0;
    std::uint64_t decodeUs = 0;
    std::uint64_t resizeUs = 0;
    std::uint64_t encodeUs = 0;
    std::uint64_t presentUs = 0;
    std::uint64_t outputBytes = 0;
    std::uint64_t colorsUsed = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    double terminalWriteP95Milliseconds = 0.0;
    double terminalWriteP99Milliseconds = 0.0;
    std::uint64_t outputFailures = 0;
    std::uint64_t backpressureEvents = 0;
    std::uint64_t payloadLimitDrops = 0;
};

}