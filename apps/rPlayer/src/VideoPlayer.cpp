/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/VideoPlayer.hpp>
#include <rPlayer/AdaptiveResolution.hpp>
#include <rPlayer/Audio.hpp>
#include <rPlayer/FrameMetrics.hpp>
#include <rPlayer/MetricsLogger.hpp>
#include <rPlayer/VideoColor.hpp>

#include <rasterm/rasterm.hpp>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

namespace rasterm::rPlayer {
namespace {

cv::VideoCapture openVideo(const std::string& path)
{
    cv::VideoCapture capture;
    for (const int backend : { cv::CAP_FFMPEG, cv::CAP_MSMF, cv::CAP_ANY }) {
        try {
            capture.open(path, backend);
        }
        catch (const cv::Exception&) {
        }
        if (capture.isOpened()) {
            break;
        }
    }
    return capture;
}

cv::Size scaledVideoSize(const Extent maximum, const double scale)
{
    const int width = std::max(1, static_cast<int>(std::lround(maximum.width * scale)));
    const int unalignedHeight = std::max(6, static_cast<int>(std::lround(maximum.height * scale)));
    const int height = std::max(6, unalignedHeight / 6 * 6);
    return { width, height };
}

}

bool playVideoFile(const std::string& path)
{
    cv::VideoCapture capture = openVideo(path);
    if (!capture.isOpened()) {
        std::cerr << "Failed to open video: " << path << '\n';
        return false;
    }

    double fps = capture.get(cv::CAP_PROP_FPS);
    if (fps <= 0.0 || std::isnan(fps)) {
        fps = 30.0;
    }
    const ColorMetadata sourceColor = probeVideoColor(path);
    const ColorMetadata pixelColor = decodedRgbColor(sourceColor);

    Engine engine;
    const Status initialization = engine.initialize({
            .quality = QualityProfile::Realtime,
            .useAlternateScreen = true,
            .preserveCursor = true,
            .enableDirtyRegions = false,
        });
    if (!initialization) {
        std::cerr << "rasterm failed to initialize: " << initialization.message << '\n';
        return false;
    }
    engine.clear();

    TerminalGeometry terminal = engine.terminalGeometry();
    const Extent sourceSize{
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT)),
    };
    Extent maximumTarget = fitWithin(sourceSize, { terminal.pixelWidth, terminal.pixelHeight });

    using Clock = std::chrono::steady_clock;
    const auto frameBudget = std::chrono::duration<double>(1.0 / fps);
    AudioPlayer audio;
    const bool hasAudio = audio.start(path);
    const double audioClockOrigin = hasAudio ? audio.playbackClockSeconds() : 0.0;

    const auto playbackStart = Clock::now();
    auto metricsStart = playbackStart;
    FrameMetrics metrics;
    MetricsLogger metricsLog("rplayer-metrics.csv");
    metricsLog.beginSession(path, fps);
    long long decodedFrameIndex = 0;
    double videoPtsOrigin = std::numeric_limits<double>::quiet_NaN();
    double previousRawPresentationTime = -1.0;
    bool useCaptureTimestamps = true;
    const double lateToleranceSeconds = 0.75 / fps;
    AdaptiveResolution resolution(frameBudget.count());
    double renderScale = resolution.scale();
    cv::Size target = scaledVideoSize(maximumTarget, renderScale);
    cv::Mat bgr;
    cv::Mat resized;

    for (;;) {
        const auto decodeStart = Clock::now();
        if (!capture.read(bgr)) {
            break;
        }
        const auto decodeEnd = Clock::now();
        const auto decodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(decodeEnd - decodeStart);
        ++decodedFrameIndex;
        if (decodedFrameIndex == 1 && !maximumTarget.width) {
            maximumTarget = fitWithin({ bgr.cols, bgr.rows }, { terminal.pixelWidth, terminal.pixelHeight });
            target = scaledVideoSize(maximumTarget, renderScale);
        }

        const double rawPresentationTime = capture.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
        double presentationTime = 0.0;
        const bool advancingTimestamp = decodedFrameIndex == 1 || rawPresentationTime > previousRawPresentationTime;
        if (useCaptureTimestamps && std::isfinite(rawPresentationTime) && rawPresentationTime >= 0.0 &&
            advancingTimestamp) {
            if (!std::isfinite(videoPtsOrigin)) {
                videoPtsOrigin = rawPresentationTime;
            }
            presentationTime = rawPresentationTime - videoPtsOrigin;
            previousRawPresentationTime = rawPresentationTime;
        }
        else {
            useCaptureTimestamps = false;
            presentationTime = static_cast<double>(decodedFrameIndex - 1) / fps;
        }

        const auto masterClock = [&] {
            return hasAudio ? audio.playbackClockSeconds() - audioClockOrigin
                            : std::chrono::duration<double>(Clock::now() - playbackStart).count();
        };

        if (presentationTime + lateToleranceSeconds < masterClock()) {
            metrics.record({ .decode = decodeDuration, .dropped = true });
            continue;
        }
        const double waitSeconds = presentationTime - masterClock();
        if (waitSeconds > 0.001) {
            std::this_thread::sleep_for(std::chrono::duration<double>(waitSeconds));
        }

        const auto resizeStart = Clock::now();
        cv::resize(bgr, resized, target, 0, 0, cv::INTER_AREA);
        const auto resizeEnd = Clock::now();
        const FrameView frame{
            .data = resized.ptr<std::uint8_t>(),
            .width = resized.cols,
            .height = resized.rows,
            .stride = static_cast<std::ptrdiff_t>(resized.step),
            .format = PixelFormat::BGR24,
            .metadata = {
                .frameId = static_cast<std::uint64_t>(decodedFrameIndex),
                .timestampNanoseconds = static_cast<std::int64_t>(presentationTime * 1'000'000'000.0),
                .color = pixelColor,
                .sourceColor = sourceColor,
            },
        };

        const auto renderStart = Clock::now();
        const RenderStats result = engine.renderFrame(frame);
        const auto renderEnd = Clock::now();
        const TerminalGeometry updatedTerminal = engine.terminalGeometry();
        if (updatedTerminal.pixelWidth > 0 && updatedTerminal.pixelHeight > 0 &&
            (updatedTerminal.pixelWidth != terminal.pixelWidth ||
             updatedTerminal.pixelHeight != terminal.pixelHeight)) {
            terminal = updatedTerminal;
            maximumTarget = fitWithin({ bgr.cols, bgr.rows },
                                      { terminal.pixelWidth, terminal.pixelHeight });
            target = scaledVideoSize(maximumTarget, renderScale);
        }
        const double renderSeconds = std::chrono::duration<double>(resizeEnd - resizeStart + renderEnd - renderStart).count();
        metrics.record({
            .decode = decodeDuration,
            .resize = std::chrono::duration_cast<std::chrono::microseconds>(resizeEnd - resizeStart),
            .encode = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::duration<double, std::milli>(result.encodeMilliseconds)),
            .present = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::duration<double, std::milli>(result.presentMilliseconds)),
            .outputBytes = result.payloadBytes,
            .dirtyRegions = result.fullFrame ? 0u : result.dirtyRegions,
            .width = result.width,
            .height = result.height,
            .colorsUsed = result.colorsUsed,
            .terminalWriteP95Milliseconds = result.terminalWriteP95Milliseconds,
            .terminalWriteP99Milliseconds = result.terminalWriteP99Milliseconds,
            .outputFailures = result.outputFailures,
            .backpressureEvents = result.backpressureEvents,
            .payloadLimitDrops = result.payloadLimitDrops,
        });

        if (resolution.observe(renderSeconds)) {
            renderScale = resolution.scale();
            target = scaledVideoSize(maximumTarget, renderScale);
        }

        const auto now = Clock::now();
        if (now - metricsStart >= std::chrono::seconds(2)) {
            metricsLog.write(now - playbackStart, metrics.snapshot(now - metricsStart), renderScale,
                             audio.underrunCount());
            metrics.reset();
            metricsStart = now;
        }
    }

    audio.stop();
    return true;
}

}
