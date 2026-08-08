/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rPlayer/FrameMetrics.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>

namespace rasterm::rPlayer {

class MetricsLogger {
public:
    explicit MetricsLogger(const std::string& path);

    void beginSession(const std::string& source, double sourceFps);
    void write(std::chrono::steady_clock::duration elapsed,
               const FrameMetricsSnapshot& metrics,
               double renderScale,
               std::uint64_t audioUnderruns);

private:
    std::ofstream output;
    double sourceFps = 0.0;
};

}