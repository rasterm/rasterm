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

DamageRegion alignToTerminalCells(const DamageRegion region, const int frameWidth,
                                  const int frameHeight, const TerminalCellPixels cells) noexcept
{
    const int x = (region.x / cells.cellWidth) * cells.cellWidth;
    const int y = (region.y / cells.cellHeight) * cells.cellHeight;
    const auto right = static_cast<std::int64_t>(region.x) + region.width;
    const auto bottom = static_cast<std::int64_t>(region.y) + region.height;
    const auto alignedRight = ((right + cells.cellWidth - 1) / cells.cellWidth) * cells.cellWidth;
    const auto alignedBottom = ((bottom + cells.cellHeight - 1) / cells.cellHeight) * cells.cellHeight;
    const int clippedRight = static_cast<int>(std::min<std::int64_t>(frameWidth, alignedRight));
    const int clippedBottom = static_cast<int>(std::min<std::int64_t>(frameHeight, alignedBottom));
    return { x, y, clippedRight - x, clippedBottom - y };
}

std::int64_t area(const DamageRegion region) noexcept
{
    return static_cast<std::int64_t>(region.width) * region.height;
}

bool mergeWithoutInflation(DamageRegion& left, const DamageRegion right) noexcept
{
    const int x1 = std::min(left.x, right.x);
    const int y1 = std::min(left.y, right.y);
    const int x2 = std::max(left.x + left.width, right.x + right.width);
    const int y2 = std::max(left.y + left.height, right.y + right.height);
    const int overlapWidth = std::max(0, std::min(left.x + left.width, right.x + right.width) -
        std::max(left.x, right.x));
    const int overlapHeight = std::max(0, std::min(left.y + left.height, right.y + right.height) -
        std::max(left.y, right.y));
    const std::int64_t unionArea = area(left) + area(right) -
        static_cast<std::int64_t>(overlapWidth) * overlapHeight;
    const DamageRegion bounds{ x1, y1, x2 - x1, y2 - y1 };
    if (area(bounds) != unionArea) return false;
    left = bounds;
    return true;
}

void normalizeDamage(std::vector<DamageRegion>& regions)
{
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t first = 0; first < regions.size() && !merged; ++first) {
            for (std::size_t second = first + 1; second < regions.size(); ++second) {
                if (mergeWithoutInflation(regions[first], regions[second])) {
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(second));
                    merged = true;
                    break;
                }
            }
        }
    }
    std::sort(regions.begin(), regions.end(), [](const DamageRegion left,
                                                  const DamageRegion right) {
        return left.y != right.y ? left.y < right.y : left.x < right.x;
    });
}

bool canPresentWithoutBottomScroll(const std::span<const DamageRegion> regions,
                                   const int frameHeight) noexcept
{
    return std::none_of(regions.begin(), regions.end(), [frameHeight](const DamageRegion region) {
        return region.y + region.height == frameHeight;
    });
}

bool planDamagePresentation(std::vector<DamageRegion>& regions, const int frameWidth,
                            const int frameHeight, const TerminalCellPixels cells,
                            const float fullFrameThreshold)
{
    if (regions.empty()) return false;
    normalizeDamage(regions);

    std::int64_t changedArea = 0;
    DamageRegion bounds = regions.front();
    for (const DamageRegion region : regions) {
        changedArea += area(region);
        const int right = std::max(bounds.x + bounds.width, region.x + region.width);
        const int bottom = std::max(bounds.y + bounds.height, region.y + region.height);
        bounds.x = std::min(bounds.x, region.x);
        bounds.y = std::min(bounds.y, region.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
    }

    const std::int64_t frameArea = static_cast<std::int64_t>(frameWidth) * frameHeight;
    if (changedArea >= static_cast<double>(frameArea) * fullFrameThreshold) return true;
    if (regions.size() == 1) return false;

    const std::int64_t cursorAndFrameCost = static_cast<std::int64_t>(cells.cellWidth) *
        cells.cellHeight * 2 * static_cast<std::int64_t>(regions.size() - 1);
    const std::int64_t proportionalAllowance = changedArea /
        (regions.size() >= 8 ? 4 : 8);
    if (area(bounds) <= changedArea + std::max(cursorAndFrameCost, proportionalAllowance)) {
        regions.assign(1, bounds);
        return area(bounds) >= static_cast<double>(frameArea) * fullFrameThreshold;
    }
    return false;
}

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
    const bool cleared = terminal.clearAndHome();
    reset();
    return cleared ? ErrorCode::None : terminal.error();
}

void Renderer::reset()
{
    backend->reset();
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
        backend->prepareFrame(frame);
        backend->setOutputLimit(options.maximumOutputBytes);
        const std::string_view sixel = backend->encodeFrame(frame);
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (backend->outputLimitExceeded()) {
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
                 .colorsUsed = backend->lastColorCount(),
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
            suppliedDamage.push_back(alignToTerminalCells(
                frame.metadata.damage.rectangles[index], frame.width, frame.height,
                options.cellPixels));
        }
        normalizeDamage(suppliedDamage);
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
        difference.isFullFrame = planDamagePresentation(
            presentationDamage, frame.width, frame.height, options.cellPixels,
            options.damage.fullFrameThreshold);
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
        backend->prepareFrame(frame);
        backend->setOutputLimit(options.maximumOutputBytes);
        const std::string_view sixel = backend->encodeFrame(frame);
        const auto encodeEnd = std::chrono::steady_clock::now();
        if (backend->outputLimitExceeded()) {
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
                 .colorsUsed = backend->lastColorCount(),
                 .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(encodeEnd - encodeStart),
                 .presentDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - presentStart) };
    }

    transactionBytes.clear();
    encodedRegions.clear();
    encodedRegions.reserve(difference.regions.size());
    const auto encodeStart = std::chrono::steady_clock::now();
    backend->prepareRegionalFrame(frame);
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
        backend->prepareRegion(frame, region);
        backend->setOutputLimit(remaining);
        const std::string_view sixel = backend->encodeRegion(frame, region);
        if (backend->outputLimitExceeded()) {
            reset();
            return { .error = ErrorCode::OutputBufferLimitExceeded,
                     .scratchBytes = scratchCapacity(),
                     .outputCapacityBytes = outputCapacity(),
                     .encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - encodeStart) };
        }
        const std::size_t offset = transactionBytes.size();
        transactionBytes.append(sixel);
        const int colors = backend->lastColorCount();
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
        outputSucceeded = terminal.drawAtCell(encoded.region.y / options.cellPixels.cellHeight,
                                              encoded.region.x / options.cellPixels.cellWidth,
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
