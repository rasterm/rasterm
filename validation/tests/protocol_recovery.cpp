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

    const rasterm::DamageRect damage{ 14, 26, 7, 13 };
    frame.metadata.damage = { &damage, 1, true };
    const std::size_t dirtyStart = sink.bytes.size();
    if (!renderer.render(frame).rendered) return 15;
    if (std::string_view(sink.bytes).substr(dirtyStart).find("\x1b[3;3H") ==
        std::string_view::npos) return 16;
    renderer.updateCellPixels({ 14, 13 });
    renderer.reset();
    if (!renderer.render(frame).rendered) return 17;
    return 0;
}