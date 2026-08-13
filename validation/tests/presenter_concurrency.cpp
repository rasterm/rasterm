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
            !entered.exchange(true)) {
            presenter->shutdown();
            called.store(true);
        }
        return true;
    }
    bool flush() noexcept override { return true; }

    rasterm::Presenter* presenter = nullptr;
    std::atomic<bool> entered = false;
    std::atomic<bool> called = false;
};

class GateSink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        if (bytes.find("\x1bP") != std::string_view::npos && !blocked.exchange(true)) {
            while (!released.load()) std::this_thread::yield();
        }
        return true;
    }
    bool flush() noexcept override { return true; }

    std::atomic<bool> blocked = false;
    std::atomic<bool> released = false;
};

struct EventCapture {
    std::atomic<std::uint64_t> droppedId = 0;
};

void captureEvent(const rasterm::Event& event, void* context) noexcept
{
    if (event.type == rasterm::EventType::FrameDropped) {
        static_cast<EventCapture*>(context)->droppedId.store(event.frameId);
    }
}

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
    std::atomic<std::uint64_t> accepted = 0;
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
                if (presenter.submit(frame)) ++accepted;
            }
        });
    }
    std::thread stopper([&] {
        while (accepted.load() == 0) std::this_thread::yield();
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

    GateSink gate;
    EventCapture events;
    rasterm::Presenter outcomes;
    rasterm::PresenterOptions outcomeOptions;
    outcomeOptions.engine.output = &gate;
    outcomeOptions.engine.enableDirtyRegions = true;
    outcomeOptions.engine.events = { captureEvent, &events };
    if (!outcomes.initialize(outcomeOptions)) return 8;
    std::array<std::uint8_t, 100 * 100 * 3> outcomePixels{};
    rasterm::FrameView identified{
        outcomePixels.data(), 100, 100, 100 * 3, rasterm::PixelFormat::RGB24,
    };
    identified.metadata.frameId = 1;
    if (!outcomes.submit(identified) || !waitFor(gate.blocked)) return 9;
    const std::array<rasterm::DamageRect, 1> firstDamage{ rasterm::DamageRect{ 0, 1, 1, 1 } };
    const std::array<rasterm::DamageRect, 1> secondDamage{ rasterm::DamageRect{ 99, 7, 1, 1 } };
    identified.metadata.frameId = 2;
    identified.metadata.damage = { firstDamage.data(), firstDamage.size(), true };
    if (!outcomes.submit(identified)) return 10;
    identified.metadata.frameId = 3;
    identified.metadata.damage = { secondDamage.data(), secondDamage.size(), true };
    if (!outcomes.submit(identified)) return 11;
    if (outcomes.waitUntilIdle(std::chrono::milliseconds(1)) ||
        outcomes.status().code != rasterm::ErrorCode::TimedOut) return 12;
    gate.released.store(true);
    if (!outcomes.waitUntilIdle(std::chrono::seconds(2))) return 13;
    if (events.droppedId.load() != 2) return 14;
    auto outcomeStats = outcomes.stats();
    if (outcomeStats.submittedFrames != 3 || outcomeStats.replacedFrames != 1 ||
        outcomeStats.submittedFrames != outcomeStats.presentedFrames +
            outcomeStats.unchangedFrames + outcomeStats.failedFrames +
            outcomeStats.replacedFrames + outcomeStats.cancelledFrames) return 15;
    if (outcomeStats.latestRender.fullFrame) return 18;
    if (outcomeStats.latestRender.dirtyRegions != 2) return 19;
    if (outcomes.submit(rasterm::FrameView{}) ||
        outcomes.status().code != rasterm::ErrorCode::InvalidArgument ||
        outcomes.stats().rejectedFrames != 1) return 16;
    if (!outcomes.invalidate() || !outcomes.submit(identified) ||
        !outcomes.waitUntilIdle(std::chrono::seconds(2)) ||
        !outcomes.stats().latestRender.fullFrame) return 17;
    outcomes.shutdown();
    return 0;
}
