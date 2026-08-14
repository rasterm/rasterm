/* SPDX-License-Identifier: Apache-2.0 */

#include <output/TerminalRenderer.hpp>
#include <render/Renderer.hpp>

#include <validation/tests/support/MicrosoftSixelHarness.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FaultSink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view data) noexcept override
    {
        ++writeCalls;
        if (writeCalls == failedWrite) return false;
        bytes.append(data);
        chunks.emplace_back(data);
        return true;
    }
    bool flush() noexcept override
    {
        ++flushCalls;
        return flushCalls != failedFlush;
    }
    void failWriteAfter(const std::size_t offset) noexcept { failedWrite = writeCalls + offset; }
    void clearBytes() { bytes.clear(); chunks.clear(); }

    std::size_t writeCalls = 0;
    std::size_t flushCalls = 0;
    std::size_t failedWrite = static_cast<std::size_t>(-1);
    std::size_t failedFlush = static_cast<std::size_t>(-1);
    std::string bytes;
    std::vector<std::string> chunks;
};

std::size_t count(const std::string_view text, const std::string_view needle)
{
    std::size_t result = 0;
    for (std::size_t position = 0; (position = text.find(needle, position)) != text.npos;
         position += needle.size()) ++result;
    return result;
}

class DcsFaultSink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view data) noexcept override
    {
        if (fail && data.find("\x1bP") != data.npos) {
            fail = false;
            return false;
        }
        bytes.append(data);
        return true;
    }
    bool flush() noexcept override { return true; }
    bool fail = false;
    std::string bytes;
};

}

int main()
{
    constexpr std::string_view sixel = "\x1bPq\"1;1;1;1#0;2;100;0;0#0@\x1b\\";
    for (std::size_t boundary = 1; boundary <= 3; ++boundary) {
        FaultSink sink;
        sink.failedWrite = boundary;
        {
            rasterm::TerminalRenderer terminal(sink, true);
            if (terminal.good()) return 1;
        }
        if (sink.bytes.find("\x1b[?25h") == std::string::npos ||
            sink.bytes.find("\x1b[?1049l") == std::string::npos) return 2;
    }
    {
        FaultSink sink;
        sink.failedFlush = 1;
        {
            rasterm::TerminalRenderer terminal(sink, true);
            if (terminal.good()) return 3;
        }
        if (sink.flushCalls < 2) return 4;
    }

    for (std::size_t boundary = 1; boundary <= 6; ++boundary) {
        FaultSink sink;
        {
            rasterm::TerminalRenderer terminal(sink, true);
            sink.clearBytes();
            sink.failWriteAfter(boundary);
            const bool began = terminal.beginSynchronizedUpdate(true, true);
            const bool drew = terminal.drawAtHome(sixel);
            const bool ended = terminal.endSynchronizedUpdate();
            if (began && drew && ended) return 5;
            if (count(sink.bytes, "\x1bP") != count(sink.bytes, "\x1b\\")) return 6;
            if (count(sink.bytes, "\x1b[?2026h") != count(sink.bytes, "\x1b[?2026l")) return 7;
        }
        if (sink.bytes.find("\x1b[?25h") == std::string::npos ||
            sink.bytes.find("\x1b[?1049l") == std::string::npos) return 8;
    }

    {
        FaultSink sink;
        rasterm::TerminalRenderer terminal(sink);
        sink.failedFlush = sink.flushCalls + 1;
        terminal.beginSynchronizedUpdate();
        terminal.drawAtHome(sixel);
        if (terminal.endSynchronizedUpdate()) return 9;
        if (count(sink.bytes, "\x1b[?2026h") != count(sink.bytes, "\x1b[?2026l")) return 10;
    }

    {
        FaultSink sink;
        rasterm::TerminalRenderer terminal(sink);
        sink.clearBytes();
        if (!terminal.drawAtHome(sixel)) return 20;
        const auto displayMode = sink.bytes.find("\x1b[?80h");
        const auto image = sink.bytes.find("\x1bP");
        const auto scrollingMode = sink.bytes.find("\x1b[?80l", image);
        if (displayMode == std::string::npos || image <= displayMode ||
            scrollingMode <= image) return 21;
    }

    {
        FaultSink sink;
        rasterm::TerminalRenderer terminal(sink);
        sink.clearBytes();
        if (!terminal.drawAtHome(sixel, false, 5) || sink.chunks.size() < 4 ||
            rasterm::test::MicrosoftSixelHarness{}.parse(sink.bytes).complete == false) return 24;

        sink.clearBytes();
        sink.failWriteAfter(4);
        if (terminal.drawAtHome(sixel, false, 5)) return 25;
        if (count(sink.bytes, "\x1bP") != count(sink.bytes, "\x1b\\")) return 26;
    }

    DcsFaultSink sink;
    rasterm::Renderer renderer(sink, {
        .enableDirtyRegions = true,
        .cellPixels = { 7, 13 },
    });
    std::array<rasterm::RgbColor, 2> palette{{ { 255, 0, 0 }, { 0, 0, 255 } }};
    std::array<std::uint8_t, 28 * 39> indices{};
    rasterm::IndexedFrameView frame{
        indices.data(), 28, 39, 28, { palette.data(), palette.size() }
    };
    if (!renderer.render(frame).rendered) return 11;
    std::fill(indices.begin(), indices.end(), std::uint8_t{ 1 });
    sink.fail = true;
    if (renderer.render(frame).error != rasterm::ErrorCode::OutputWriteFailed) return 12;
    const std::size_t successfulStart = sink.bytes.size();
    if (!renderer.render(frame).rendered) return 13;
    const auto recovered = rasterm::test::MicrosoftSixelHarness{}.parse(
        std::string_view(sink.bytes).substr(successfulStart));
    if (!recovered.complete || recovered.pixels.empty() ||
        recovered.pixels.front() != palette[1]) return 14;

    const rasterm::DamageRect damage{ 14, 13, 7, 13 };
    frame.metadata.damage = { &damage, 1, true };
    const std::size_t dirtyStart = sink.bytes.size();
    if (!renderer.render(frame).rendered) return 15;
    if (std::string_view(sink.bytes).substr(dirtyStart).find("\x1b[2;3H") ==
        std::string_view::npos) return 16;

    const rasterm::DamageRect unalignedDamage{ 15, 14, 1, 1 };
    frame.metadata.damage = { &unalignedDamage, 1, true };
    const std::size_t unalignedStart = sink.bytes.size();
    if (!renderer.render(frame).rendered) return 17;
    const auto alignedPatch = rasterm::test::MicrosoftSixelHarness{}.parse(
        std::string_view(sink.bytes).substr(unalignedStart));
    if (!alignedPatch.complete || alignedPatch.width != 7 || alignedPatch.height != 13) return 18;

    renderer.updateCellPixelSize({ 14, 13 });
    renderer.reset();
    if (!renderer.render(frame).rendered) return 19;

    FaultSink bottomSink;
    rasterm::Renderer bottomRenderer(bottomSink, {
        .preserveCursor = false,
        .enableDirtyRegions = true,
        .useSynchronizedOutput = false,
        .cellPixels = { 10, 20 },
    });
    std::array<std::uint8_t, 10 * 140 * 3> bottomPixels{};
    rasterm::FrameView bottomFrame{
        bottomPixels.data(), 10, 140, 10 * 3, rasterm::PixelFormat::RGB24
    };
    if (!bottomRenderer.render(bottomFrame).usedFullFrame) return 22;
    const rasterm::DamageRect bottomDamage{ 0, 120, 10, 20 };
    bottomFrame.metadata.damage = { &bottomDamage, 1, true };
    if (!bottomRenderer.render(bottomFrame).usedFullFrame) return 23;

    FaultSink limitedSink;
    rasterm::Renderer limitedRenderer(limitedSink, {
        .preserveCursor = false,
        .enableDirtyRegions = false,
        .useSynchronizedOutput = false,
        .maximumOutputBytes = 64,
    });
    const std::size_t beforeLimitedFrame = limitedSink.bytes.size();
    std::array<std::uint8_t, 32 * 32 * 3> limitedPixels{};
    for (std::size_t index = 0; index < limitedPixels.size(); ++index) {
        limitedPixels[index] = static_cast<std::uint8_t>((index * 131 + index / 7) & 0xff);
    }
    const rasterm::FrameView limitedFrame{
        limitedPixels.data(), 32, 32, 32 * 3, rasterm::PixelFormat::RGB24
    };
    const auto limitedResult = limitedRenderer.render(limitedFrame);
    if (limitedResult.error != rasterm::ErrorCode::OutputBufferLimitExceeded ||
        limitedResult.rendered || limitedSink.bytes.size() != beforeLimitedFrame) {
        return 27;
    }
    return 0;
}
