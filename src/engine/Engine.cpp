/* SPDX-License-Identifier: Apache-2.0 */

#include <color/ColorConverter.hpp>
#include <diagnostics/DiagnosticReporter.hpp>
#include <output/StdoutSink.hpp>
#include <platform/windows/ConsoleSession.hpp>
#include <rasterm/Engine.hpp>
#include <render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rasterm {
namespace {

bool validOptions(const EngineOptions& options) noexcept
{
    const auto validQuality = options.quality == QualityProfile::Realtime ||
        options.quality == QualityProfile::AdaptiveVideo ||
        options.quality == QualityProfile::HighQuality;
    const auto validToneMap = options.color.toneMap == ToneMapOperator::None ||
        options.color.toneMap == ToneMapOperator::Reinhard ||
        options.color.toneMap == ToneMapOperator::Hable ||
        options.color.toneMap == ToneMapOperator::Aces;
    const auto validDither = options.color.realtimeDither == DitherMode::None ||
        options.color.realtimeDither == DitherMode::OrderedBayer4x4 ||
        options.color.realtimeDither == DitherMode::FloydSteinberg;
    const auto validSupport = [](const CapabilitySupport support) {
        return support == CapabilitySupport::Unknown ||
            support == CapabilitySupport::Unsupported || support == CapabilitySupport::Supported;
    };
    const TerminalGeometry& geometry = options.terminalOverrides.geometry;
    const bool validGeometry = geometry.columns >= 0 && geometry.rows >= 0 &&
        geometry.pixelWidth >= 0 && geometry.pixelHeight >= 0 &&
        geometry.cellPixelWidth >= 0 && geometry.cellPixelHeight >= 0 &&
        ((geometry.cellPixelWidth == 0) == (geometry.cellPixelHeight == 0));
    return validQuality && validToneMap && validDither && validGeometry &&
        options.encoder.paletteRefreshFrames >= 0 && options.encoder.maximumThreads >= 0 &&
        validSupport(options.terminalOverrides.sixel) &&
        validSupport(options.terminalOverrides.synchronizedOutput) &&
        std::isfinite(options.backpressureThresholdMilliseconds) &&
        options.backpressureThresholdMilliseconds >= 0.0 &&
        std::isfinite(options.color.outputPeakNits) && options.color.outputPeakNits > 0.0f &&
        options.color.adaptivePaletteLockFrames >= 0 &&
        std::isfinite(options.color.sceneCutThreshold) &&
        options.color.sceneCutThreshold >= 0.0f && options.color.sceneCutThreshold <= 1.0f;
}

bool validColorMetadata(const ColorMetadata& color) noexcept
{
    const bool validPrimaries = color.primaries == ColorPrimaries::Unspecified ||
        color.primaries == ColorPrimaries::Bt709 || color.primaries == ColorPrimaries::Bt2020 ||
        color.primaries == ColorPrimaries::DisplayP3;
    const bool validTransfer = color.transfer == TransferFunction::Unspecified ||
        color.transfer == TransferFunction::Srgb || color.transfer == TransferFunction::Linear ||
        color.transfer == TransferFunction::Bt709 || color.transfer == TransferFunction::Gamma22 ||
        color.transfer == TransferFunction::Pq || color.transfer == TransferFunction::Hlg;
    const bool validMatrix = color.matrix == MatrixCoefficients::Unspecified ||
        color.matrix == MatrixCoefficients::Identity || color.matrix == MatrixCoefficients::Bt601 ||
        color.matrix == MatrixCoefficients::Bt709 ||
        color.matrix == MatrixCoefficients::Bt2020NonConstant;
    const bool validRange = color.range == ColorRange::Unspecified ||
        color.range == ColorRange::Limited || color.range == ColorRange::Full;
    return validPrimaries && validTransfer && validMatrix && validRange &&
        std::isfinite(color.referenceWhiteNits) && color.referenceWhiteNits > 0.0f &&
        std::isfinite(color.masteringPeakNits) && color.masteringPeakNits > 0.0f;
}

bool validMetadata(const FrameMetadata& metadata) noexcept
{
    return validColorMetadata(metadata.color) && validColorMetadata(metadata.sourceColor) &&
        (metadata.damage.count == 0 || metadata.damage.rectangles != nullptr) &&
        (metadata.damage.supplied ||
         (metadata.damage.count == 0 && metadata.damage.rectangles == nullptr));
}

std::uint8_t expand4(const unsigned value) noexcept
{
    return static_cast<std::uint8_t>((value << 4) | value);
}

std::uint8_t expand5(const unsigned value) noexcept
{
    return static_cast<std::uint8_t>((value << 3) | (value >> 2));
}

std::uint8_t expand6(const unsigned value) noexcept
{
    return static_cast<std::uint8_t>((value << 2) | (value >> 4));
}

}

class Engine::Impl {
public:
    ~Impl()
    {
        shutdown();
    }

    Status initialize(const EngineOptions& requestedOptions)
    {
        shutdown();
        if (!validOptions(requestedOptions)) {
            latestStatus = Status::failure(ErrorCode::InvalidArgument,
                                           "Engine options contain an invalid enum or numeric value.");
            return latestStatus;
        }
        try {
            options = requestedOptions;
            latestStatus = diagnostics.configure(options.diagnostics);
            if (!latestStatus) {
                return latestStatus;
            }
            if (options.output == nullptr) {
                latestStatus = console.activate(options.useAlternateScreen);
                detectedCapabilities = console.capabilities();
                if (!latestStatus) {
                    diagnostics.report(DiagnosticSeverity::Error, latestStatus.code,
                                       latestStatus.message);
                    return latestStatus;
                }
            }
            else {
                detectedCapabilities = {};
                detectedCapabilities.customOutput = true;
            }
            applyOverrides(detectedCapabilities);
            if (options.requireSixelSupport &&
                detectedCapabilities.sixel != CapabilitySupport::Supported) {
                console.restore();
                emit({ .type = EventType::UnsupportedCapability,
                       .error = ErrorCode::UnsupportedTerminal });
                return fail(ErrorCode::UnsupportedTerminal,
                            "SIXEL support could not be confirmed for the active terminal.");
            }

            RendererOptions rendererOptions;
            rendererOptions.useAlternateScreen = options.useAlternateScreen;
            rendererOptions.preserveCursor = options.preserveCursor;
            rendererOptions.enableDirtyRegions = options.enableDirtyRegions;
            rendererOptions.useSynchronizedOutput = options.useSynchronizedOutput &&
                detectedCapabilities.synchronizedOutput != CapabilitySupport::Unsupported;
            rendererOptions.maximumOutputBytes = options.maximumOutputBytes;
            rendererOptions.outputChunkBytes = options.encoder.outputChunkBytes;
            if (detectedCapabilities.geometry.cellPixelWidth > 0 &&
                detectedCapabilities.geometry.cellPixelHeight > 0) {
                rendererOptions.cellPixels = {
                    detectedCapabilities.geometry.cellPixelWidth,
                    detectedCapabilities.geometry.cellPixelHeight,
                };
            }
            if (options.quality == QualityProfile::HighQuality) {
                rendererOptions.sixel = SixelOptions::ForImage();
            }
            else if (options.quality == QualityProfile::AdaptiveVideo) {
                rendererOptions.sixel = SixelOptions::ForHighQualityVideo();
                if (options.color.realtimeDither != DitherMode::None) {
                    rendererOptions.sixel.dither = options.color.realtimeDither;
                }
            }
            else {
                rendererOptions.sixel = SixelOptions::ForRealtimeVideo();
                rendererOptions.sixel.dither = options.color.realtimeDither;
            }
            rendererOptions.sixel.adaptivePaletteLockFrames =
                std::max(0, options.color.adaptivePaletteLockFrames);
            rendererOptions.sixel.sceneCutThreshold =
                std::clamp(options.color.sceneCutThreshold, 0.0f, 1.0f);
            rendererOptions.sixel.persistPaletteRegisters =
                options.encoder.persistPaletteRegisters;
            rendererOptions.sixel.paletteRefreshFrames = options.encoder.paletteRefreshFrames;
            rendererOptions.sixel.maximumThreads = options.encoder.maximumThreads;
            rendererOptions.sixel.independentRegionQuantization =
                options.encoder.independentRegionQuantization;
            activeOutput = options.output != nullptr ? options.output : &stdoutOutput;
            renderer = std::make_unique<Renderer>(*activeOutput, rendererOptions);
            if (!renderer->good()) {
                renderer.reset();
                console.restore();
                return fail(ErrorCode::OutputWriteFailed,
                            "The output sink rejected terminal initialization bytes.");
            }

            initialized = true;
            lastFrameTime = {};
            latestStats = {};
            writeSampleCount = 0;
            writeSampleCursor = 0;
            outputFailures = 0;
            backpressureEvents = 0;
            payloadLimitDrops = 0;
            latestStatus = Status::success();
            diagnostics.report(DiagnosticSeverity::Info, ErrorCode::None,
                               "rasterm engine initialized.");
            return latestStatus;
        }
        catch (const std::bad_alloc&) {
            renderer.reset();
            console.restore();
            return fail(ErrorCode::OutOfMemory,
                        "Out of memory.");
        }
        catch (const std::exception& error) {
            renderer.reset();
            console.restore();
            return fail(ErrorCode::InitializationException, error.what());
        }
        catch (...) {
            renderer.reset();
            console.restore();
            return fail(ErrorCode::InitializationException,
                        "Unknown exception while initializing rasterm.");
        }
    }

    void shutdown() noexcept
    {
        renderer.reset();
        convertedFrame.clear();
        console.restore();
        activeOutput = nullptr;
        initialized = false;
        diagnostics.reset();
    }

    RenderStats renderFrame(const FrameView& frame)
    {
        const auto validationStart = std::chrono::steady_clock::now();
        if (!initialized || !renderer || !frame.isValid() || !validMetadata(frame.metadata)) {
            return invalidFrame("Packed frame validation failed.");
        }
        const auto validationEnd = std::chrono::steady_clock::now();

        try {
            refreshTerminalGeometry();
            const auto conversionStart = std::chrono::steady_clock::now();
            FrameView unpacked = frame;
            const ColorMetadata color = frame.metadata.color;
            const bool alreadySrgb = color.transfer == TransferFunction::Srgb &&
                color.primaries == ColorPrimaries::Bt709 && color.range == ColorRange::Full;
            const bool needsColorConversion = options.color.convertToSrgb && !alreadySrgb;
            if (bytesPerPixel(frame.format) == 2 && needsColorConversion) {
                const std::size_t outputStride = static_cast<std::size_t>(frame.width) * 3;
                convertedFrame.resize(outputStride * static_cast<std::size_t>(frame.height));
                for (int row = 0; row < frame.height; ++row) {
                    const std::uint8_t* source = frame.data +
                        static_cast<std::ptrdiff_t>(row) * frame.stride;
                    std::uint8_t* destination = convertedFrame.data() +
                        static_cast<std::size_t>(row) * outputStride;
                    for (int column = 0; column < frame.width; ++column) {
                        std::uint16_t pixel = 0;
                        std::memcpy(&pixel, source + static_cast<std::size_t>(column) * 2,
                                    sizeof(pixel));
                        if (frame.format == PixelFormat::RGB565) {
                            destination[column * 3] = expand5((pixel >> 11) & 0x1F);
                            destination[column * 3 + 1] = expand6((pixel >> 5) & 0x3F);
                            destination[column * 3 + 2] = expand5(pixel & 0x1F);
                        }
                        else if (frame.format == PixelFormat::XRGB1555) {
                            destination[column * 3] = expand5((pixel >> 10) & 0x1F);
                            destination[column * 3 + 1] = expand5((pixel >> 5) & 0x1F);
                            destination[column * 3 + 2] = expand5(pixel & 0x1F);
                        }
                        else {
                            destination[column * 3] = expand4((pixel >> 12) & 0x0F);
                            destination[column * 3 + 1] = expand4((pixel >> 8) & 0x0F);
                            destination[column * 3 + 2] = expand4((pixel >> 4) & 0x0F);
                        }
                    }
                }
                unpacked = {
                    .data = convertedFrame.data(),
                    .width = frame.width,
                    .height = frame.height,
                    .stride = static_cast<std::ptrdiff_t>(outputStride),
                    .format = PixelFormat::RGB24,
                    .metadata = frame.metadata,
                };
            }
            const FrameView colorManaged = colorConverter.toSrgb(unpacked, options.color);
            const auto conversionEnd = std::chrono::steady_clock::now();
            RenderResult rendered = renderer->render(colorManaged);
            rendered.scratchBytes += convertedFrame.capacity() + colorConverter.scratchCapacity();
            RenderStats result = record(rendered, colorManaged.width, colorManaged.height,
                                        colorManaged.metadata);
            result.validationMilliseconds =
                std::chrono::duration<double, std::milli>(validationEnd - validationStart).count();
            result.conversionMilliseconds =
                std::chrono::duration<double, std::milli>(conversionEnd - conversionStart).count();
            latestStats = result;
            return result;
        }
        catch (const std::bad_alloc&) {
            renderer->reset();
            return renderFailure(ErrorCode::OutOfMemory,
                                 "Out of memory.");
        }
        catch (const std::exception& error) {
            renderer->reset();
            return renderException(error.what());
        }
        catch (...) {
            renderer->reset();
            return renderException("Unknown exception while rendering a packed frame.");
        }
    }

    RenderStats renderFrame(const IndexedFrameView& frame)
    {
        const auto validationStart = std::chrono::steady_clock::now();
        if (!initialized || !renderer || !frame.hasValidIndices() ||
            !validMetadata(frame.metadata)) {
            return invalidFrame("Indexed frame validation failed.");
        }
        const auto validationEnd = std::chrono::steady_clock::now();
        RenderStats result = renderValidatedIndexedFrame(frame);
        result.validationMilliseconds =
            std::chrono::duration<double, std::milli>(validationEnd - validationStart).count();
        latestStats = result;
        return result;
    }

    RenderStats renderValidatedIndexedFrame(const IndexedFrameView& frame)
    {
        if (!initialized || !renderer || !frame.isValid() || !validMetadata(frame.metadata)) {
            return invalidFrame("Indexed frame validation failed.");
        }
        try {
            refreshTerminalGeometry();
            return record(renderer->renderValidated(frame), frame.width, frame.height, frame.metadata);
        }
        catch (const std::bad_alloc&) {
            renderer->reset();
            return renderFailure(ErrorCode::OutOfMemory,
                                 "Out of memory.");
        }
        catch (const std::exception& error) {
            renderer->reset();
            return renderException(error.what());
        }
        catch (...) {
            renderer->reset();
            return renderException("Unknown exception while rendering an indexed frame.");
        }
    }

    RenderStats record(const RenderResult& result, const int width, const int height,
                       const FrameMetadata metadata)
    {
        const auto now = std::chrono::steady_clock::now();
        double currentFps = latestStats.framesPerSecond;
        double currentBytesPerSecond = latestStats.payloadBytesPerSecond;
        if (lastFrameTime.time_since_epoch().count() != 0) {
            const double elapsed = std::chrono::duration<double>(now - lastFrameTime).count();
            if (elapsed > 0.0) {
                const double instantaneousFps = 1.0 / elapsed;
                currentFps = currentFps == 0.0 ? instantaneousFps : currentFps * 0.9 + instantaneousFps * 0.1;
                const double instantaneousBytes = result.outputBytes / elapsed;
                currentBytesPerSecond = currentBytesPerSecond == 0.0
                    ? instantaneousBytes
                    : currentBytesPerSecond * 0.9 + instantaneousBytes * 0.1;
            }
        }
        lastFrameTime = now;

        if (result.presentDuration.count() > 0) {
            terminalWriteSamples[writeSampleCursor] = result.presentDuration.count() / 1000.0;
            writeSampleCursor = (writeSampleCursor + 1) % terminalWriteSamples.size();
            writeSampleCount = std::min(writeSampleCount + 1, terminalWriteSamples.size());
        }
        if (result.error == ErrorCode::OutputWriteFailed || result.error == ErrorCode::OutputFlushFailed) {
            ++outputFailures;
        }
        if (result.error == ErrorCode::OutputBufferLimitExceeded) {
            ++payloadLimitDrops;
        }
        if (result.presentDuration.count() / 1000.0 > options.backpressureThresholdMilliseconds) {
            ++backpressureEvents;
        }
        const auto [p95, p99] = writePercentiles();

        latestStats = {
            .error = result.error,
            .rendered = result.rendered,
            .fullFrame = result.usedFullFrame,
            .payloadBytes = result.outputBytes,
            .dirtyRegions = result.dirtyRegionCount,
            .width = width,
            .height = height,
            .colorsUsed = result.colorsUsed,
            .encodeMilliseconds = result.encodeDuration.count() / 1000.0,
            .presentMilliseconds = result.presentDuration.count() / 1000.0,
            .framesPerSecond = currentFps,
            .payloadBytesPerSecond = currentBytesPerSecond,
            .terminalWriteP95Milliseconds = p95,
            .terminalWriteP99Milliseconds = p99,
            .outputFailures = outputFailures,
            .backpressureEvents = backpressureEvents,
            .payloadLimitDrops = payloadLimitDrops,
            .wireBytes = result.wireBytes,
            .scratchBytes = result.scratchBytes,
            .outputCapacityBytes = result.outputCapacityBytes,
        };
        if (result.error != ErrorCode::None) {
            latestStatus = Status::failure(result.error, renderErrorMessage(result.error));
            diagnostics.report(DiagnosticSeverity::Error, result.error, latestStatus.message);
            emit({ .type = EventType::OutputFailure, .error = result.error,
                   .frameId = metadata.frameId,
                   .timestampNanoseconds = metadata.timestampNanoseconds });
        }
        else {
            latestStatus = Status::success();
        }
        return latestStats;
    }

    void applyOverrides(TerminalCapabilities& capabilities) const noexcept
    {
        const TerminalOverrides& overrides = options.terminalOverrides;
        if (overrides.sixel != CapabilitySupport::Unknown) capabilities.sixel = overrides.sixel;
        if (overrides.synchronizedOutput != CapabilitySupport::Unknown) {
            capabilities.synchronizedOutput = overrides.synchronizedOutput;
        }
        const TerminalGeometry& source = overrides.geometry;
        TerminalGeometry& destination = capabilities.geometry;
        if (source.columns > 0) destination.columns = source.columns;
        if (source.rows > 0) destination.rows = source.rows;
        if (source.pixelWidth > 0) destination.pixelWidth = source.pixelWidth;
        if (source.pixelHeight > 0) destination.pixelHeight = source.pixelHeight;
        if (source.cellPixelWidth > 0) {
            destination.cellPixelWidth = source.cellPixelWidth;
            destination.cellPixelHeight = source.cellPixelHeight;
        }
    }

    void refreshTerminalGeometry()
    {
        if (options.output != nullptr) {
            return;
        }
        TerminalCapabilities refreshed = detectedCapabilities;
        refreshed.geometry = console.refreshGeometry();
        applyOverrides(refreshed);
        const TerminalGeometry geometry = refreshed.geometry;
        if (geometry.columns <= 0 || geometry.rows <= 0 ||
            (geometry.columns == detectedCapabilities.geometry.columns &&
             geometry.rows == detectedCapabilities.geometry.rows &&
             geometry.cellPixelWidth == detectedCapabilities.geometry.cellPixelWidth &&
             geometry.cellPixelHeight == detectedCapabilities.geometry.cellPixelHeight)) {
            return;
        }
        detectedCapabilities.geometry = geometry;
        renderer->updateCellPixels({ geometry.cellPixelWidth, geometry.cellPixelHeight });
        renderer->reset();
        emit({ .type = EventType::TerminalResized, .geometry = geometry });
    }

    void emit(const Event& event) const noexcept
    {
        if (options.events.callback != nullptr) {
            options.events.callback(event, options.events.context);
        }
    }

    Status fail(const ErrorCode code, std::string message)
    {
        latestStatus = Status::failure(code, std::move(message));
        diagnostics.report(DiagnosticSeverity::Error, latestStatus.code, latestStatus.message);
        return latestStatus;
    }

    RenderStats invalidFrame(const std::string_view message)
    {
        latestStatus = Status::failure(ErrorCode::InvalidArgument, std::string(message));
        diagnostics.report(DiagnosticSeverity::Error, latestStatus.code, latestStatus.message);
        latestStats = { .error = ErrorCode::InvalidArgument };
        return latestStats;
    }

    RenderStats renderException(const std::string_view message)
    {
        return renderFailure(ErrorCode::RenderingException, message);
    }

    RenderStats renderFailure(const ErrorCode code, const std::string_view message)
    {
        latestStatus = Status::failure(code, std::string(message));
        diagnostics.report(DiagnosticSeverity::Error, latestStatus.code, latestStatus.message);
        latestStats = { .error = code };
        return latestStats;
    }

    std::pair<double, double> writePercentiles() const
    {
        if (writeSampleCount == 0) {
            return {};
        }
        auto sorted = terminalWriteSamples;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(writeSampleCount));
        const auto valueAt = [&](const double percentile) {
            const std::size_t index = std::min(
                static_cast<std::size_t>(percentile * static_cast<double>(writeSampleCount - 1)),
                writeSampleCount - 1);
            return sorted[index];
        };
        return { valueAt(0.95), valueAt(0.99) };
    }

    static std::string renderErrorMessage(const ErrorCode code)
    {
        switch (code) {
        case ErrorCode::OutputWriteFailed: return "The output sink rejected terminal bytes.";
        case ErrorCode::OutputFlushFailed: return "The output sink could not flush terminal bytes.";
        case ErrorCode::OutputBufferLimitExceeded: return "The encoded frame exceeded the configured output ceiling.";
        default: return "rasterm could not render the frame.";
        }
    }

    Status clear()
    {
        if (!initialized || !renderer) {
            return fail(ErrorCode::InvalidArgument, "Engine is not initialized.");
        }
        const ErrorCode result = renderer->clear();
        if (result != ErrorCode::None) {
            return fail(result, renderErrorMessage(result));
        }
        latestStatus = Status::success();
        return latestStatus;
    }

    void reset()
    {
        if (renderer) {
            renderer->reset();
        }
        colorConverter.reset();
        latestStats = {};
        lastFrameTime = {};
    }

    TerminalGeometry terminalGeometry() const noexcept
    {
        return initialized ? detectedCapabilities.geometry : TerminalGeometry{};
    }

    EngineOptions options{};
    RenderStats latestStats{};
    StdoutSink stdoutOutput;
    OutputSink* activeOutput = nullptr;
    DiagnosticReporter diagnostics;
    ConsoleSession console;
    TerminalCapabilities detectedCapabilities{};
    Status latestStatus{};
    std::unique_ptr<Renderer> renderer;
    std::vector<std::uint8_t> convertedFrame;
    ColorConverter colorConverter;
    std::chrono::steady_clock::time_point lastFrameTime{};
    std::array<double, 256> terminalWriteSamples{};
    std::size_t writeSampleCount = 0;
    std::size_t writeSampleCursor = 0;
    std::uint64_t outputFailures = 0;
    std::uint64_t backpressureEvents = 0;
    std::uint64_t payloadLimitDrops = 0;
    bool initialized = false;
};

Engine::Engine() : impl(new Impl) {}
Engine::~Engine() { delete impl; }

Engine::Engine(Engine&& other) noexcept : impl(std::exchange(other.impl, nullptr)) {}

Engine& Engine::operator=(Engine&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = std::exchange(other.impl, nullptr);
    }
    return *this;
}

Status Engine::initialize(const EngineOptions& options)
{
    return impl ? impl->initialize(options)
                : Status::failure(ErrorCode::InitializationException,
                                  "Engine implementation is unavailable.");
}

void Engine::shutdown() noexcept
{
    if (impl) {
        impl->shutdown();
    }
}

bool Engine::isInitialized() const noexcept
{
    return impl && impl->initialized;
}

RenderStats Engine::renderFrame(const FrameView& frame)
{
    return impl ? impl->renderFrame(frame) : RenderStats{};
}

RenderStats Engine::renderFrame(const IndexedFrameView& frame)
{
    return impl ? impl->renderFrame(frame) : RenderStats{};
}

RenderStats Engine::renderValidatedIndexedFrame(const IndexedFrameView& frame)
{
    return impl ? impl->renderValidatedIndexedFrame(frame) : RenderStats{};
}

RenderStats Engine::renderFrame(const std::uint8_t* data, const int width, const int height,
                                const std::ptrdiff_t stride, const PixelFormat format)
{
    return renderFrame({ data, width, height, stride, format });
}

Status Engine::clear()
{
    return impl ? impl->clear()
                : Status::failure(ErrorCode::InitializationException,
                                  "Engine implementation is unavailable.");
}

void Engine::reset()
{
    if (impl) {
        impl->reset();
    }
}

TerminalGeometry Engine::terminalGeometry() const noexcept
{
    return impl ? impl->terminalGeometry() : TerminalGeometry{};
}

TerminalCapabilities Engine::capabilities() const noexcept
{
    return impl ? impl->detectedCapabilities : TerminalCapabilities{};
}

Status Engine::status() const
{
    return impl ? impl->latestStatus
                : Status::failure(ErrorCode::InitializationException,
                                  "Engine implementation is unavailable.");
}

RenderStats Engine::stats() const noexcept
{
    return impl ? impl->latestStats : RenderStats{};
}

}
