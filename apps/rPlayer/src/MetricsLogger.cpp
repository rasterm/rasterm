/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/MetricsLogger.hpp>

#include <iomanip>

namespace rasterm::rPlayer {

MetricsLogger::MetricsLogger(const std::string& path) : output(path, std::ios::trunc)
{
}

void MetricsLogger::beginSession(const std::string& source, const double fps)
{
    sourceFps = fps;
    if (!output) {
        return;
    }
    output << "# source=" << source << '\n'
           << "elapsed_s,fps,source_fps,decode_ms,resize_ms,encode_ms,present_ms,"
              "kib_per_frame,width,height,colors,render_scale,rendered,dropped,dirty_frames,audio_underruns,"
              "payload_mib_s,present_p95_ms,present_p99_ms,output_failures,backpressure_events,payload_limit_drops\n";
    output.flush();
}

void MetricsLogger::write(const std::chrono::steady_clock::duration elapsed,
                          const FrameMetricsSnapshot& metrics,
                          const double renderScale,
                          const std::uint64_t audioUnderruns)
{
    if (!output) {
        return;
    }
    output << std::fixed << std::setprecision(3)
           << std::chrono::duration<double>(elapsed).count() << ','
           << metrics.framesPerSecond << ',' << sourceFps << ','
           << metrics.averageDecodeMs << ',' << metrics.averageResizeMs << ','
           << metrics.averageEncodeMs << ',' << metrics.averagePresentMs << ','
           << metrics.averageBytesPerFrame / 1024.0 << ','
           << metrics.outputWidth << ',' << metrics.outputHeight << ','
           << metrics.averageColorsPerFrame << ',' << renderScale << ','
           << metrics.renderedFrames << ',' << metrics.droppedFrames << ','
           << metrics.dirtyFrames << ',' << audioUnderruns << ','
           << metrics.payloadBytesPerSecond / (1024.0 * 1024.0) << ','
           << metrics.terminalWriteP95Milliseconds << ','
           << metrics.terminalWriteP99Milliseconds << ','
           << metrics.outputFailures << ',' << metrics.backpressureEvents << ','
           << metrics.payloadLimitDrops << '\n';
    output.flush();
}

}