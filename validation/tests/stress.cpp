/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>

namespace {

class ProtocolSink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        std::lock_guard lock(mutex);
        dcsStarts += count(bytes, "\x1bP");
        dcsEnds += count(bytes, "\x1b\\");
        syncStarts += count(bytes, "\x1b[?2026h");
        syncEnds += count(bytes, "\x1b[?2026l");
        alternateStarts += count(bytes, "\x1b[?1049h");
        alternateEnds += count(bytes, "\x1b[?1049l");
        cursorHides += count(bytes, "\x1b[?25l");
        cursorShows += count(bytes, "\x1b[?25h");
        return true;
    }
    bool flush() noexcept override { return true; }

    std::size_t dcsStarts = 0;
    std::size_t dcsEnds = 0;
    std::size_t syncStarts = 0;
    std::size_t syncEnds = 0;
    std::size_t alternateStarts = 0;
    std::size_t alternateEnds = 0;
    std::size_t cursorHides = 0;
    std::size_t cursorShows = 0;

private:
    static std::size_t count(const std::string_view input, const std::string_view pattern) noexcept
    {
        std::size_t matches = 0;
        std::size_t position = 0;
        while ((position = input.find(pattern, position)) != std::string_view::npos) {
            ++matches;
            position += pattern.size();
        }
        return matches;
    }

    std::mutex mutex;
};

}

int main()
{
    constexpr int width = 16;
    constexpr int height = 13;
    int frameCount = 2000;
    char configuredFrames[32]{};
    std::size_t configuredLength = 0;
    if (getenv_s(&configuredLength, configuredFrames, sizeof(configuredFrames),
                 "RASTERM_STRESS_FRAMES") == 0 && configuredLength > 1) {
        frameCount = std::max(1, static_cast<int>(std::strtol(configuredFrames, nullptr, 10)));
    }
    std::array<rasterm::RgbColor, 2> palette{{ { 10, 20, 30 }, { 240, 220, 200 } }};
    std::array<std::uint8_t, width * height> indices{};
    rasterm::IndexedFrameView frame{
        indices.data(), width, height, width, { palette.data(), palette.size() }
    };

    ProtocolSink sink;
    rasterm::Engine engine;
    if (!engine.initialize({ .useAlternateScreen = true, .output = &sink })) {
        return 1;
    }
    for (int sequence = 0; sequence < frameCount; ++sequence) {
        indices[sequence % indices.size()] ^= 1;
        if (!engine.renderFrame(frame).rendered) {
            return 2;
        }
    }
    engine.shutdown();
    const auto expectedFrames = static_cast<std::size_t>(frameCount);
    if (sink.dcsStarts != expectedFrames || sink.dcsEnds != expectedFrames ||
        sink.syncStarts != expectedFrames || sink.syncEnds != expectedFrames ||
        sink.alternateStarts != 1 || sink.alternateEnds != 1 ||
        sink.cursorHides != 1 || sink.cursorShows != 1) {
        return 3;
    }

    for (int cycle = 0; cycle < 20; ++cycle) {
        if (!engine.initialize({ .useAlternateScreen = true, .output = &sink })) {
            return 4;
        }
        engine.shutdown();
    }
    if (sink.alternateStarts != 21 || sink.alternateEnds != 21 ||
        sink.cursorHides != 21 || sink.cursorShows != 21) {
        return 5;
    }

    rasterm::Presenter presenter;
    if (!presenter.initialize({
            .engine = { .output = &sink },
            .maximumFramesPerSecond = 240.0,
        })) {
        return 6;
    }
    constexpr std::uint64_t submissions = 10000;
    for (std::uint64_t sequence = 0; sequence < submissions; ++sequence) {
        indices[sequence % indices.size()] ^= 1;
        if (!presenter.submit(frame)) {
            return 7;
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (presenter.stats().presentedFrames == 0 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    presenter.shutdown();
    const auto stats = presenter.stats();
    if (stats.submittedFrames != submissions || stats.presentedFrames == 0 ||
        stats.presentedFrames >= submissions || stats.replacedFrames == 0 ||
        sink.dcsStarts != sink.dcsEnds || sink.syncStarts != sink.syncEnds) {
        return 8;
    }
    return 0;
}