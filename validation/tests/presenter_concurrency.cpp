/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <string_view>
#include <thread>
#include <vector>

namespace {

class SlowSink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override
    {
        std::this_thread::yield();
        return true;
    }
    bool flush() noexcept override { return true; }
};

class ReentrantSink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        if (presenter != nullptr && bytes.find("\x1bP") != std::string_view::npos &&
            !called.exchange(true)) {
            presenter->shutdown();
        }
        return true;
    }
    bool flush() noexcept override { return true; }

    rasterm::Presenter* presenter = nullptr;
    std::atomic<bool> called = false;
};

bool waitFor(const std::atomic<bool>& value)
{
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!value.load() && std::chrono::steady_clock::now() < limit) {
        std::this_thread::yield();
    }
    return value.load();
}

}

int main()
{
    SlowSink sink;
    rasterm::Presenter presenter;
    if (!presenter.initialize({ .engine = { .output = &sink } })) return 1;

    std::atomic<bool> query = true;
    std::atomic<bool> invalidStats = false;
    std::thread observer([&] {
        std::uint64_t previous = 0;
        while (query.load()) {
            const auto stats = presenter.stats();
            if (stats.submittedFrames < previous || stats.presentedFrames > stats.submittedFrames) {
                invalidStats.store(true);
            }
            previous = stats.submittedFrames;
        }
    });

    std::vector<std::thread> submitters;
    for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
        submitters.emplace_back([&, threadIndex] {
            std::array<std::uint8_t, 16 * 6 * 3> pixels{};
            for (int frameIndex = 0; frameIndex < 500; ++frameIndex) {
                pixels[0] = static_cast<std::uint8_t>(threadIndex + frameIndex);
                const rasterm::FrameView frame{
                    pixels.data(), 16, 6, 16 * 3, rasterm::PixelFormat::RGB24,
                    { .frameId = static_cast<std::uint64_t>(threadIndex * 500 + frameIndex) },
                };
                (void)presenter.submit(frame);
            }
        });
    }
    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        presenter.shutdown();
    });
    for (auto& submitter : submitters) submitter.join();
    stopper.join();
    query.store(false);
    observer.join();
    const auto concurrentStats = presenter.stats();
    if (invalidStats.load() || concurrentStats.submittedFrames == 0 ||
        concurrentStats.presentedFrames > concurrentStats.submittedFrames) return 2;

    std::array<std::uint8_t, 18> pixels{};
    const rasterm::FrameView frame{
        pixels.data(), 1, 6, 3, rasterm::PixelFormat::RGB24,
    };
    for (int iteration = 0; iteration < 25; ++iteration) {
        if (!presenter.initialize({ .engine = { .output = &sink } })) return 3;
        if (!presenter.submit(frame)) return 4;
        presenter.shutdown();
    }

    ReentrantSink reentrantSink;
    rasterm::Presenter reentrantPresenter;
    reentrantSink.presenter = &reentrantPresenter;
    if (!reentrantPresenter.initialize({ .engine = { .output = &reentrantSink } })) return 5;
    if (!reentrantPresenter.submit(frame) || !waitFor(reentrantSink.called)) return 6;
    if (reentrantPresenter.status().code != rasterm::ErrorCode::InvalidArgument) return 7;
    reentrantPresenter.shutdown();
    return 0;
}