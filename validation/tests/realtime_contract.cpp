/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string_view>
#include <thread>
#include <vector>

namespace {

class SlowSink final : public rasterm::OutputSink {
public:
    explicit SlowSink(const std::chrono::milliseconds delay = {}) : delay(delay) {}

    bool write(const std::string_view bytes) noexcept override
    {
        if (bytes.starts_with("\x1bP")) {
            std::this_thread::sleep_for(delay);
        }
        return true;
    }

    bool flush() noexcept override { return true; }

private:
    std::chrono::milliseconds delay;
};

double percentile(std::vector<double> samples, const double fraction)
{
    std::sort(samples.begin(), samples.end());
    const std::size_t rank = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(samples.size())));
    const std::size_t index = std::min(samples.size() - 1,
        std::max<std::size_t>(1, rank) - 1);
    return samples[index];
}

}

int main()
{
    using Clock = std::chrono::steady_clock;
    constexpr int width = 160;
    constexpr int height = 90;
    constexpr int frameCount = 120;
    std::vector<std::uint8_t> pixels(width * height * 3);
    SlowSink slow(std::chrono::milliseconds(25));
    rasterm::Presenter presenter;
    rasterm::PresenterOptions options;
    options.engine.output = &slow;
    options.engine.preserveCursor = false;
    options.engine.useSynchronizedOutput = false;
    if (!presenter.initialize(options)) return 1;

    std::vector<double> submitMilliseconds;
    submitMilliseconds.reserve(frameCount);
    const auto started = Clock::now();
    auto due = started;
    for (int frameId = 0; frameId < frameCount; ++frameId) {
        due += std::chrono::microseconds(16667);
        for (std::size_t index = 0; index < pixels.size(); index += 3) {
            pixels[index] = static_cast<std::uint8_t>((index + frameId * 13) & 0xff);
            pixels[index + 1] = static_cast<std::uint8_t>((index / 3 + frameId * 7) & 0xff);
            pixels[index + 2] = static_cast<std::uint8_t>((frameId * 17) & 0xff);
        }
        rasterm::FrameView frame{
            pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24,
            { .frameId = static_cast<std::uint64_t>(frameId) },
        };
        const auto submitStarted = Clock::now();
        if (!presenter.submit(frame)) return 2;
        submitMilliseconds.push_back(
            std::chrono::duration<double, std::milli>(Clock::now() - submitStarted).count());
        std::this_thread::sleep_until(due);
    }
    const double producerSeconds = std::chrono::duration<double>(Clock::now() - started).count();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const rasterm::PresenterStats stats = presenter.stats();
    presenter.shutdown();
    if (stats.submittedFrames != frameCount || stats.replacedFrames == 0 ||
        stats.presentedFrames >= stats.submittedFrames) return 3;
    if (producerSeconds < 1.85 || producerSeconds > 2.30 ||
        percentile(submitMilliseconds, 0.99) > 10.0) return 4;

    SlowSink measured(std::chrono::milliseconds(4));
    rasterm::Engine engine;
    rasterm::EngineOptions engineOptions;
    engineOptions.output = &measured;
    engineOptions.preserveCursor = false;
    engineOptions.useSynchronizedOutput = false;
    engineOptions.backpressureThresholdMilliseconds = 0.5;
    if (!engine.initialize(engineOptions)) return 5;
    rasterm::FrameView frame{
        pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24,
    };
    const rasterm::RenderStats rendered = engine.renderFrame(frame);
    if (!rendered.rendered || rendered.backpressureEvents != 1) return 6;
    engine.shutdown();

    SlowSink limitSink;
    rasterm::EngineOptions limitOptions;
    limitOptions.output = &limitSink;
    limitOptions.preserveCursor = false;
    limitOptions.useSynchronizedOutput = false;
    limitOptions.enableDirtyRegions = false;
    limitOptions.maximumOutputBytes = 32;
    limitOptions.backpressureThresholdMilliseconds = 12.0;
    rasterm::Engine limited;
    if (!limited.initialize(limitOptions)) return 7;
    const rasterm::RenderStats dropped = limited.renderFrame(frame);
    if (dropped.error != rasterm::ErrorCode::OutputBufferLimitExceeded ||
        dropped.rendered || dropped.payloadLimitDrops != 1 ||
        dropped.backpressureEvents != 0) {
        std::fprintf(stderr,
            "output-limit result: error=%u rendered=%d limit_drops=%llu backpressure=%llu\n",
            static_cast<unsigned>(dropped.error), dropped.rendered ? 1 : 0,
            static_cast<unsigned long long>(dropped.payloadLimitDrops),
            static_cast<unsigned long long>(dropped.backpressureEvents));
        return 8;
    }
    limited.shutdown();
    return 0;
}
