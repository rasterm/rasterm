/* SPDX-License-Identifier: Apache-2.0 */

#include <render/Renderer.hpp>

#include <backend/SixelBackend.hpp>

#include <algorithm>
#include <utility>

namespace rasterm {
namespace {

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
    options(std::move(options)),
    terminal(output, this->options.useAlternateScreen),
    backend(std::make_unique<SixelBackend>(this->options.sixel)),
    damage([&] {
        auto config = this->options.damage;
        config.tileWidth = std::max(config.tileWidth, this->options.cellPixels.cellWidth);
        config.tileHeight = std::max(config.tileHeight, this->options.cellPixels.cellHeight);
        config.tileWidth = (config.tileWidth / this->options.cellPixels.cellWidth) * this->options.cellPixels.cellWidth;
        config.tileHeight = (config.tileHeight / this->options.cellPixels.cellHeight) * this->options.cellPixels.cellHeight;
        return config;
    }())
{
}

ErrorCode Renderer::clear()
{
    return terminal.clearAndHome() ? ErrorCode::None : terminal.error();
}

void Renderer::reset()
{
    damage.reset();
    previousWidth = 0;
    previousHeight = 0;
}

void Renderer::updateCellPixels(const TerminalCellPixels cellPixels)
{
    if (cellPixels.cellWidth <= 0 || cellPixels.cellHeight <= 0 ||
        (cellPixels.cellWidth == options.cellPixels.cellWidth &&
         cellPixels.cellHeight == options.cellPixels.cellHeight)) {
        return;
    }
    options.cellPixels = cellPixels;
    auto config = options.damage;
    config.tileWidth = std::max(config.tileWidth, cellPixels.cellWidth);
    config.tileHeight = std::max(config.tileHeight, cellPixels.cellHeight);
    config.tileWidth = (config.tileWidth / cellPixels.cellWidth) * cellPixels.cellWidth;
    config.tileHeight = (config.tileHeight / cellPixels.cellHeight) * cellPixels.cellHeight;
    damage = DamageTracker(config);
    previousWidth = 0;
    previousHeight = 0;
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
    return renderFrame(frame);
}

template<typename Frame>
RenderResult Renderer::renderFrame(const Frame& frame)
{
    if (!frame.isValid()) {
        return {};
    }

    const bool dimensionsChanged = previousWidth > 0 &&
        (frame.width != previousWidth || frame.height != previousHeight);
    if (dimensionsChanged) {
        damage.reset();
    }

    if (!options.enableDirtyRegions) {
        const auto encodeStart = std::chrono::steady_clock::now();
        const std::string_view sixel = backend->encodeFrame(frame);
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (options.maximumOutputBytes > 0 && sixel.size() > options.maximumOutputBytes) {
            damage.reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded };
        }
        UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
        bool outputSucceeded = true;
        if (dimensionsChanged) {
            outputSucceeded = terminal.clearAndHome();
        }
        const auto presentStart = encodeEnd;
        outputSucceeded = terminal.drawAtHome(sixel) && outputSucceeded;
        outputSucceeded = update.finish() && outputSucceeded;
        previousWidth = frame.width;
        previousHeight = frame.height;
        if (!outputSucceeded) {
            damage.reset();
        }
        return { .error = outputSucceeded ? ErrorCode::None : terminal.error(),
                 .rendered = outputSucceeded, .usedFullFrame = true,
                 .outputBytes = outputSucceeded ? sixel.size() : 0, .colorsUsed = backend->lastColorCount(),
                 .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart),
                 .presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - presentStart) };
    }

    DamageResult difference;
    if (frame.metadata.damage.supplied) {
        damage.update(frame);
        const auto regions = std::span<const DamageRegion>(
            frame.metadata.damage.rectangles, frame.metadata.damage.count);
        const bool coversFrame = dimensionsChanged ||
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
        difference = damage.compareAndUpdate(frame);
    }
    if (!difference.hasChanges) {
        return {};
    }

    if (difference.isFullFrame) {
        const auto encodeStart = std::chrono::steady_clock::now();
        const std::string_view sixel = backend->encodeFrame(frame);
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (options.maximumOutputBytes > 0 && sixel.size() > options.maximumOutputBytes) {
            damage.reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded };
        }
        UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
        bool outputSucceeded = true;
        if (dimensionsChanged) {
            outputSucceeded = terminal.clearAndHome();
        }
        const auto presentStart = encodeEnd;
        outputSucceeded = terminal.drawAtHome(sixel) && outputSucceeded;
        outputSucceeded = update.finish() && outputSucceeded;
        previousWidth = frame.width;
        previousHeight = frame.height;
        if (!outputSucceeded) {
            damage.reset();
        }
        return { .error = outputSucceeded ? ErrorCode::None : terminal.error(),
                 .rendered = outputSucceeded, .usedFullFrame = true,
                 .outputBytes = outputSucceeded ? sixel.size() : 0, .colorsUsed = backend->lastColorCount(),
                 .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart),
                 .presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - presentStart) };
    }

    if (options.maximumOutputBytes > 0) {
        std::size_t predictedBytes = 0;
        for (const DamageRegion& region : difference.regions) {
            predictedBytes += backend->encodeRegion(frame, region).size();
            if (predictedBytes > options.maximumOutputBytes) {
                damage.reset();
                return { .error = ErrorCode::OutputBufferLimitExceeded };
            }
        }
    }

    UpdateScope update(terminal, options.preserveCursor, options.useSynchronizedOutput);
    bool outputSucceeded = true;
    if (dimensionsChanged) {
        outputSucceeded = terminal.clearAndHome();
    }
    RenderResult result{ .rendered = true, .usedFullFrame = false, .dirtyRegionCount = static_cast<std::uint32_t>(difference.regions.size()) };
    for (const DamageRegion& region : difference.regions) {
        const auto encodeStart = std::chrono::steady_clock::now();
        const std::string_view sixel = backend->encodeRegion(frame, region);
        const auto encodeEnd = std::chrono::steady_clock::now();
        outputSucceeded = terminal.drawAtCell(region.y / options.cellPixels.cellHeight,
                                              region.x / options.cellPixels.cellWidth,
                                              sixel) && outputSucceeded;
        result.encodeDuration += std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart);
        result.presentDuration += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - encodeEnd);
        result.outputBytes += sixel.size();
        result.colorsUsed = std::max(result.colorsUsed, backend->lastColorCount());
    }
    outputSucceeded = update.finish() && outputSucceeded;
    if (!outputSucceeded) {
        damage.reset();
        result.error = terminal.error();
        result.rendered = false;
        result.outputBytes = 0;
    }
    previousWidth = frame.width;
    previousHeight = frame.height;
    return result;
}

}