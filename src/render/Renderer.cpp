/* SPDX-License-Identifier: Apache-2.0 */

#include <render/Renderer.hpp>
#include <render/DamagePlanner.hpp>

#include <backend/sixel/SixelBackend.hpp>

#include <algorithm>
#include <utility>

namespace rasterm {
namespace {

RendererOptions sanitize(RendererOptions options) noexcept
{
    options.cellPixels.width = std::max(1, options.cellPixels.width);
    options.cellPixels.height = std::max(1, options.cellPixels.height);
    return options;
}

class UpdateScope {
public:
    UpdateScope(TerminalRenderer& terminal, const bool preserveCursor,
                const bool synchronizedOutput) noexcept : terminal(terminal)
    {
        beginSucceeded = terminal.beginSynchronizedUpdate(preserveCursor, synchronizedOutput);
    }

    ~UpdateScope()
    {
        if (!finished) {
            terminal.endSynchronizedUpdate();
        }
    }

    bool finish() noexcept
    {
        if (!finished) {
            finished = true;
            return terminal.endSynchronizedUpdate() && beginSucceeded;
        }
        return beginSucceeded;
    }

private:
    TerminalRenderer& terminal;
    bool beginSucceeded = false;
    bool finished = false;
};

}

Renderer::Renderer(OutputSink& output, RendererOptions options) :
    options(sanitize(std::move(options))),
    terminal(output, this->options.useAlternateScreen),
    backend(std::make_unique<SixelBackend>(this->options.backend)),
    damage([&] {
        auto config = this->options.damage;
        config.tileWidth = std::max(config.tileWidth, this->options.cellPixels.width);
        config.tileHeight = std::max(config.tileHeight, this->options.cellPixels.height);
        config.tileWidth = (config.tileWidth / this->options.cellPixels.width) *
            this->options.cellPixels.width;
        config.tileHeight = (config.tileHeight / this->options.cellPixels.height) *
            this->options.cellPixels.height;
        return config;
    }())
{
}

ErrorCode Renderer::clear()
{
    const bool cleared = terminal.clearAndHome();
    reset();
    return cleared ? ErrorCode::None : terminal.error();
}

void Renderer::reset()
{
    backend->invalidate(BackendInvalidation::All);
    damage.reset();
    usingSuppliedDamage = false;
    previousWidth = 0;
    previousHeight = 0;
    previousFormatTag = -1;
    previousPalette.clear();
}

std::size_t Renderer::scratchCapacity() const noexcept
{
    return backend->scratchCapacity() + damage.scratchCapacity() +
        suppliedDamage.capacity() * sizeof(DamageRegion) +
        presentationDamage.capacity() * sizeof(DamageRegion) +
        encodedRegions.capacity() * sizeof(EncodedRegion) +
        previousPalette.capacity() * sizeof(RgbColor);
}

std::size_t Renderer::outputCapacity() const noexcept
{
    return backend->outputCapacity() + transactionBytes.capacity();
}

void Renderer::updateCellPixelSize(const CellPixelSize cellPixelSize)
{
    if (cellPixelSize.width <= 0 || cellPixelSize.height <= 0 ||
        (cellPixelSize.width == options.cellPixels.width &&
         cellPixelSize.height == options.cellPixels.height)) {
        return;
    }
    options.cellPixels = cellPixelSize;
    auto config = options.damage;
    config.tileWidth = std::max(config.tileWidth, cellPixelSize.width);
    config.tileHeight = std::max(config.tileHeight, cellPixelSize.height);
    config.tileWidth = (config.tileWidth / cellPixelSize.width) * cellPixelSize.width;
    config.tileHeight = (config.tileHeight / cellPixelSize.height) * cellPixelSize.height;
    damage = DamageTracker(config);
    usingSuppliedDamage = false;
    previousWidth = 0;
    previousHeight = 0;
    previousFormatTag = -1;
    previousPalette.clear();
}

RenderResult Renderer::render(const FrameView& frame)
{
    return renderFrame(frame);
}

RenderResult Renderer::render(const IndexedFrameView& frame)
{
    if (!frame.hasValidIndices()) {
        return {};
    }
    return renderValidated(frame);
}

RenderResult Renderer::renderValidated(const IndexedFrameView& frame)
{
    return renderFrame(frame);
}

template<typename Frame>
RenderResult Renderer::renderFrame(const Frame& frame)
{
    if (!frame.isValid()) {
        return {};
    }

    const int formatTag = [&] {
        if constexpr (std::is_same_v<Frame, IndexedFrameView>) return 100;
        else return static_cast<int>(frame.format);
    }();
    bool paletteChanged = false;
    if constexpr (std::is_same_v<Frame, IndexedFrameView>) {
        paletteChanged = previousPalette.size() != frame.palette.size ||
            !std::equal(previousPalette.begin(), previousPalette.end(), frame.palette.colors);
    }
    const bool dimensionsChanged = previousWidth > 0 &&
        (frame.width != previousWidth || frame.height != previousHeight);
    const bool frameIdentityChanged = previousFormatTag >= 0 &&
        (formatTag != previousFormatTag || paletteChanged);
    if (dimensionsChanged || frameIdentityChanged) {
        damage.reset();
        usingSuppliedDamage = false;
    }

    if (!options.enableDirtyRegions) {
        const auto encodeStart = std::chrono::steady_clock::now();
        backend->beginFrame(frame, FramePreparation::FullFrame);
        const EncodedUpdate encoded = backend->encode(frame, {
            .maximumBytes = options.maximumOutputBytes,
            .fullFrame = true,
        });
        const std::string_view sixel = encoded.bytes;
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (encoded.outputLimitExceeded) {
            reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded,
                     .scratchBytes = scratchCapacity(),
                     .outputCapacityBytes = outputCapacity(),
                     .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                         encodeEnd - encodeStart) };
        }
        const std::size_t wireStart = terminal.acceptedBytes();
        UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
        bool outputSucceeded = true;
        if (dimensionsChanged) {
            outputSucceeded = terminal.clearAndHome();
        }
        const auto presentStart = encodeEnd;
        outputSucceeded = terminal.drawAtHome(
            sixel, false, options.outputChunkBytes) && outputSucceeded;
        outputSucceeded = update.finish() && outputSucceeded;
        if (!outputSucceeded) {
            reset();
        }
        else {
            previousWidth = frame.width;
            previousHeight = frame.height;
            previousFormatTag = formatTag;
            if constexpr (std::is_same_v<Frame, IndexedFrameView>) {
                previousPalette.assign(frame.palette.colors, frame.palette.colors + frame.palette.size);
            }
            else {
                previousPalette.clear();
            }
        }
        return { .error = outputSucceeded ? ErrorCode::None : terminal.error(),
                 .rendered = outputSucceeded, .usedFullFrame = true,
                 .outputBytes = outputSucceeded ? sixel.size() : 0,
                 .wireBytes = terminal.acceptedBytes() - wireStart,
                 .scratchBytes = scratchCapacity(),
                 .outputCapacityBytes = outputCapacity(),
                 .colorsUsed = encoded.colorsUsed,
                 .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart),
                 .presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - presentStart) };
    }

    DamageResult difference;
    if (frame.metadata.damage.supplied) {
        if (!usingSuppliedDamage) {
            damage.reset();
            usingSuppliedDamage = true;
        }
        suppliedDamage.clear();
        suppliedDamage.reserve(frame.metadata.damage.count);
        for (std::size_t index = 0; index < frame.metadata.damage.count; ++index) {
            suppliedDamage.push_back(alignDamageToCells(
                frame.metadata.damage.rectangles[index], frame.width, frame.height,
                options.cellPixels));
        }
        normalizePresentationDamage(suppliedDamage);
        const auto regions = std::span<const DamageRegion>(suppliedDamage);
        const bool coversFrame = previousWidth == 0 || dimensionsChanged || frameIdentityChanged ||
            (regions.size() == 1 && regions[0].x == 0 && regions[0].y == 0 &&
             regions[0].width == frame.width && regions[0].height == frame.height);
        difference = {
            .isFullFrame = coversFrame,
            .hasChanges = coversFrame || !regions.empty(),
            .changedAreaRatio = coversFrame ? 1.0f : 0.0f,
            .regions = regions,
        };
    }
    else {
        if (usingSuppliedDamage) {
            damage.reset();
            usingSuppliedDamage = false;
        }
        difference = damage.compareAndUpdate(frame);
    }
    if (!difference.hasChanges) {
        return {};
    }
    if (!difference.isFullFrame) {
        presentationDamage.assign(difference.regions.begin(), difference.regions.end());
        const bool adaptivePalette = options.backend.quality != QualityProfile::Realtime;
        const int estimatedColors = options.backend.quality == QualityProfile::HighQuality
            ? 255 : adaptivePalette ? 128 : 204;
        difference.isFullFrame = planDamagePresentation(
            presentationDamage, frame.width, frame.height, options.cellPixels,
            options.useSynchronizedOutput, estimatedColors,
            adaptivePalette && options.backend.tuning.independentRegionQuantization).useFullFrame;
        difference.regions = presentationDamage;
    }

    /* Windows Terminal can scroll when a positioned SIXEL reaches the final
       terminal row, including patches composed of complete six pixel bands. */

    if (!difference.isFullFrame &&
        !canPresentWithoutBottomScroll(difference.regions, frame.height)) {
        difference.isFullFrame = true;
    }

    if (difference.isFullFrame) {
        const auto encodeStart = std::chrono::steady_clock::now();
        backend->beginFrame(frame, FramePreparation::FullFrame);
        const EncodedUpdate encoded = backend->encode(frame, {
            .maximumBytes = options.maximumOutputBytes,
            .fullFrame = true,
        });
        const std::string_view sixel = encoded.bytes;
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (encoded.outputLimitExceeded) {
            reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded,
                     .scratchBytes = scratchCapacity(),
                     .outputCapacityBytes = outputCapacity(),
                     .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                         encodeEnd - encodeStart) };
        }
        const std::size_t wireStart = terminal.acceptedBytes();
        UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
        bool outputSucceeded = true;
        if (dimensionsChanged) {
            outputSucceeded = terminal.clearAndHome();
        }
        const auto presentStart = encodeEnd;
        outputSucceeded = terminal.drawAtHome(
            sixel, false, options.outputChunkBytes) && outputSucceeded;
        outputSucceeded = update.finish() && outputSucceeded;
        if (!outputSucceeded) {
            reset();
        }
        else {
            previousWidth = frame.width;
            previousHeight = frame.height;
            previousFormatTag = formatTag;
            if constexpr (std::is_same_v<Frame, IndexedFrameView>) {
                previousPalette.assign(frame.palette.colors, frame.palette.colors + frame.palette.size);
            }
            else {
                previousPalette.clear();
            }
        }
        return { .error = outputSucceeded ? ErrorCode::None : terminal.error(),
                 .rendered = outputSucceeded, .usedFullFrame = true,
                 .outputBytes = outputSucceeded ? sixel.size() : 0,
                 .wireBytes = terminal.acceptedBytes() - wireStart,
                 .scratchBytes = scratchCapacity(),
                 .outputCapacityBytes = outputCapacity(),
                 .colorsUsed = encoded.colorsUsed,
                 .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart),
                 .presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - presentStart) };
    }

    transactionBytes.clear();
    encodedRegions.clear();
    encodedRegions.reserve(difference.regions.size());
    const auto encodeStart = std::chrono::steady_clock::now();
    backend->beginFrame(frame, FramePreparation::Regional);
    int transactionColors = 0;
    for (const DamageRegion& region : difference.regions) {
        if (options.maximumOutputBytes > 0 &&
            transactionBytes.size() >= options.maximumOutputBytes) {
            reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded,
                     .scratchBytes = scratchCapacity(),
                     .outputCapacityBytes = outputCapacity(),
                     .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - encodeStart) };
        }
        const std::size_t remaining = options.maximumOutputBytes == 0
            ? 0 : options.maximumOutputBytes - transactionBytes.size();
        const EncodedUpdate update = backend->encode(frame, {
            .region = region,
            .maximumBytes = remaining,
            .fullFrame = false,
        });
        const std::string_view sixel = update.bytes;
        if (update.outputLimitExceeded) {
            reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded,
                     .scratchBytes = scratchCapacity(),
                     .outputCapacityBytes = outputCapacity(),
                     .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - encodeStart) };
        }
        const std::size_t offset = transactionBytes.size();
        transactionBytes.append(sixel);
        const int colors = update.colorsUsed;
        transactionColors = std::max(transactionColors, colors);
        encodedRegions.push_back({ region, offset, sixel.size(), colors });
    }
    const auto encodeEnd = std::chrono::steady_clock::now();

    const std::size_t wireStart = terminal.acceptedBytes();
    UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
    bool outputSucceeded = true;
    if (dimensionsChanged) {
        outputSucceeded = terminal.clearAndHome();
    }
    RenderResult result{ .rendered = true, .usedFullFrame = false,
                         .outputBytes = transactionBytes.size(),
                         .scratchBytes = scratchCapacity(),
                         .outputCapacityBytes = outputCapacity(),
                         .dirtyRegionCount = static_cast<std::uint32_t>(encodedRegions.size()),
                         .colorsUsed = transactionColors,
                         .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart) };
    const auto presentStart = std::chrono::steady_clock::now();
    for (const EncodedRegion& encoded : encodedRegions) {
        const std::string_view sixel(transactionBytes.data() + encoded.offset, encoded.size);
        outputSucceeded = terminal.drawAtCell(encoded.region.y / options.cellPixels.height,
                                              encoded.region.x / options.cellPixels.width,
                                              sixel, false, options.outputChunkBytes) &&
            outputSucceeded;
    }
    outputSucceeded = update.finish() && outputSucceeded;
    result.presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - presentStart);
    result.wireBytes = terminal.acceptedBytes() - wireStart;
    if (!outputSucceeded) {
        reset();
        result.error = terminal.error();
        result.rendered = false;
        result.outputBytes = 0;
    }
    else {
        previousWidth = frame.width;
        previousHeight = frame.height;
        previousFormatTag = formatTag;
        if constexpr (std::is_same_v<Frame, IndexedFrameView>) {
            previousPalette.assign(frame.palette.colors, frame.palette.colors + frame.palette.size);
        }
        else {
            previousPalette.clear();
        }
    }
    return result;
}

}
