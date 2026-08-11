/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <validation/tests/support/MicrosoftSixelHarness.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>

namespace {

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view value) noexcept override
    {
        std::lock_guard lock(mutex);
        bytes.append(value);
        return true;
    }
    bool flush() noexcept override { return true; }

    std::string take()
    {
        std::lock_guard lock(mutex);
        return std::exchange(bytes, {});
    }

private:
    std::mutex mutex;
    std::string bytes;
};

class FailingSink final : public rasterm::OutputSink {
public:
    explicit FailingSink(const int successfulWrites) : remaining(successfulWrites) {}
    bool write(std::string_view) noexcept override { return remaining-- > 0; }
    bool flush() noexcept override { return true; }
private:
    int remaining;
};

void diagnosticCallback(const rasterm::DiagnosticEvent& event, void* context) noexcept
{
    auto* count = static_cast<int*>(context);
    if (event.severity == rasterm::DiagnosticSeverity::Error) {
        ++*count;
    }
}

bool rendersRed(rasterm::Engine& engine, MemorySink& sink, const std::uint8_t* data,
                const std::ptrdiff_t stride, const rasterm::PixelFormat format)
{
    sink.take();
    const auto stats = engine.renderFrame(data, 1, 1, stride, format);
    const auto decoded = rasterm::test::MicrosoftSixelHarness{}.parse(sink.take());
    return stats.rendered && decoded.complete && decoded.pixels.size() == 1 &&
        decoded.pixels[0].red > 180 && decoded.pixels[0].red > decoded.pixels[0].blue;
}

}

int main()
{
    static_assert(RASTERM_VERSION_MAJOR == rasterm::version.major);
    static_assert(RASTERM_ABI_VERSION == rasterm::abiVersion);

    std::array<std::uint8_t, 8> data{};
    if (rasterm::FrameView{}.isValid() ||
        rasterm::FrameView{ data.data(), 0, 1, 3, rasterm::PixelFormat::RGB24 }.isValid() ||
        rasterm::FrameView{ data.data(), 1, 1, 2, rasterm::PixelFormat::RGB24 }.isValid() ||
        !rasterm::FrameView{ data.data(), 1, 1, 5, rasterm::PixelFormat::RGB24 }.isValid()) {
        return 1;
    }

    MemorySink sink;
    rasterm::Engine engine;
    rasterm::EngineOptions testOptions;
    testOptions.output = &sink;
    testOptions.encoder.persistPaletteRegisters = false;
    const rasterm::Status initialized = engine.initialize(testOptions);
    if (!initialized || !initialized.message.empty() || !engine.isInitialized()) {
        return 2;
    }
    const auto capabilities = engine.capabilities();
    if (!capabilities.customOutput || capabilities.validOutputHandle ||
        capabilities.sixel != rasterm::CapabilitySupport::Unknown) {
        return 3;
    }

    const std::array<std::uint8_t, 5> rgb{ 255, 0, 0, 12, 34 };
    const std::array<std::uint8_t, 5> bgr{ 0, 0, 255, 12, 34 };
    const std::array<std::uint8_t, 6> rgba{ 255, 0, 0, 17, 12, 34 };
    const std::array<std::uint8_t, 6> bgra{ 0, 0, 255, 17, 12, 34 };
    const std::array<std::uint8_t, 2> rgb565{ 0x00, 0xF8 };
    const std::array<std::uint8_t, 2> xrgb1555{ 0x00, 0x7C };
    const std::array<std::uint8_t, 2> rgba4444{ 0x0F, 0xF0 };
    if (!rendersRed(engine, sink, rgb.data(), 5, rasterm::PixelFormat::RGB24) ||
        !rendersRed(engine, sink, bgr.data(), 5, rasterm::PixelFormat::BGR24) ||
        !rendersRed(engine, sink, rgba.data(), 6, rasterm::PixelFormat::RGBA32) ||
        !rendersRed(engine, sink, bgra.data(), 6, rasterm::PixelFormat::BGRA32) ||
        !rendersRed(engine, sink, rgb565.data(), 2, rasterm::PixelFormat::RGB565) ||
        !rendersRed(engine, sink, xrgb1555.data(), 2, rasterm::PixelFormat::XRGB1555) ||
        !rendersRed(engine, sink, rgba4444.data(), 2, rasterm::PixelFormat::RGBA4444)) {
        return 4;
    }
    const auto measuredStats = engine.stats();
    if (measuredStats.wireBytes <= measuredStats.payloadBytes ||
        measuredStats.scratchBytes == 0 || measuredStats.outputCapacityBytes == 0) return 15;
    engine.shutdown();
    const std::string restored = sink.take();
    if (restored.find("\x1b[?25h") == std::string::npos) {
        return 5;
    }

    int diagnosticErrors = 0;
    const char* diagnosticsPath = "rasterm-public-api-diagnostics.csv";
    if (!engine.initialize({
            .diagnostics = {
                .filePath = diagnosticsPath,
                .callback = diagnosticCallback,
                .callbackContext = &diagnosticErrors,
            },
            .output = &sink,
        })) {
        return 6;
    }
    const auto invalid = engine.renderFrame(nullptr, 1, 1, 3, rasterm::PixelFormat::RGB24);
    const auto recovered = engine.renderFrame(rgb.data(), 1, 1, 5, rasterm::PixelFormat::RGB24);
    const auto recoveredStatus = engine.status();
    engine.shutdown();
    if (invalid.error != rasterm::ErrorCode::InvalidArgument || diagnosticErrors != 1 ||
        !recovered.rendered || !recoveredStatus ||
        sink.take().find("validation failed") != std::string::npos) {
        return 7;
    }
    std::FILE* diagnostics = nullptr;
    if (fopen_s(&diagnostics, diagnosticsPath, "rb") != 0 || diagnostics == nullptr) {
        return 8;
    }
    std::fseek(diagnostics, 0, SEEK_END);
    const long diagnosticBytes = std::ftell(diagnostics);
    std::fclose(diagnostics);
    std::remove(diagnosticsPath);
    if (diagnosticBytes <= 0) {
        return 9;
    }

    FailingSink initializationFailure(0);
    const rasterm::Status failedInitialization = engine.initialize({ .output = &initializationFailure });
    if (failedInitialization || failedInitialization.code != rasterm::ErrorCode::OutputWriteFailed ||
        failedInitialization.message.empty()) {
        return 10;
    }

    if (!engine.initialize({ .maximumOutputBytes = 8, .output = &sink })) {
        return 11;
    }
    const auto limited = engine.renderFrame(rgb.data(), 1, 1, 5, rasterm::PixelFormat::RGB24);
    engine.shutdown();
    if (limited.error != rasterm::ErrorCode::OutputBufferLimitExceeded ||
        limited.rendered || limited.payloadLimitDrops != 1) {
        return 12;
    }

    FailingSink runtimeFailure(2);
    if (!engine.initialize({ .output = &runtimeFailure })) {
        return 13;
    }
    const auto failedRender = engine.renderFrame(rgb.data(), 1, 1, 5, rasterm::PixelFormat::RGB24);
    engine.shutdown();
    if (failedRender.error != rasterm::ErrorCode::OutputWriteFailed ||
        failedRender.outputFailures != 1) {
        return 14;
    }

    rasterm::EngineOptions overrideOptions;
    overrideOptions.output = &sink;
    overrideOptions.terminalOverrides.sixel = rasterm::CapabilitySupport::Supported;
    overrideOptions.terminalOverrides.synchronizedOutput =
        rasterm::CapabilitySupport::Unsupported;
    overrideOptions.terminalOverrides.geometry = { 100, 40, 900, 720, 9, 18 };
    if (!engine.initialize(overrideOptions)) return 16;
    const auto overridden = engine.capabilities();
    engine.shutdown();
    if (!overridden.customOutput ||
        overridden.sixel != rasterm::CapabilitySupport::Supported ||
        overridden.synchronizedOutput != rasterm::CapabilitySupport::Unsupported ||
        overridden.geometry.columns != 100 || overridden.geometry.cellPixelWidth != 9 ||
        overridden.geometry.cellPixelHeight != 18) return 17;
    return 0;
}
