/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstdint>
#include <string>

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        output.append(bytes);
        return true;
    }

    bool flush() noexcept override { return true; }

    std::string output;
};

struct CallbackState {
    std::uint32_t diagnostics = 0;
    std::uint32_t events = 0;
};

void onDiagnostic(const rasterm::DiagnosticEvent&, void* context) noexcept
{
    ++static_cast<CallbackState*>(context)->diagnostics;
}

void onEvent(const rasterm::Event&, void* context) noexcept
{
    ++static_cast<CallbackState*>(context)->events;
}

int main()
{
    MemorySink sink;
    CallbackState callbackState;
    rasterm::EngineOptions options;
    options.output = &sink;
    options.diagnostics = { nullptr, onDiagnostic, &callbackState };
    options.events = { onEvent, &callbackState };

    /* custom sinks cannot be probed. declare only capabilities and geometry the
       destination is known to support, unknown/zero keeps automatic behavior. */

    options.terminalOverrides.sixel = rasterm::CapabilitySupport::Supported;
    options.terminalOverrides.synchronizedOutput = rasterm::CapabilitySupport::Supported;
    options.terminalOverrides.geometry = { 80, 24, 800, 480, 10, 20 };

    rasterm::Engine engine;
    if (!engine.initialize(options)) return 1;

    const std::array<std::uint8_t, 12> pixels{
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255,
    };
    const rasterm::FrameView frame = rasterm::FrameView::tightlyPacked(
        pixels.data(), 2, 2, rasterm::PixelFormat::RGB24);
    const rasterm::RenderStats rendered = engine.renderFrame(frame);
    return rendered.rendered && !sink.output.empty() ? 0 : 2;
}
