/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelEncoder.hpp>
#include <encoder/SixelPalette.hpp>
#include <encoder/SixelSimd.hpp>
#include <encoder/SixelWriter.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <windows.h>

namespace rasterm {
namespace {

std::atomic<int> avx2Mode{ -1 };

std::uint64_t paletteHash(const std::uint32_t* colors, const std::size_t count) noexcept
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t index = 0; index < count; ++index) {
        hash ^= colors[index];
        hash *= 1099511628211ULL;
    }
    hash ^= count;
    return hash * 1099511628211ULL;
}

std::uint64_t paletteHash(const PaletteView palette, const int firstRegister) noexcept
{
    std::uint64_t hash = 1469598103934665603ULL ^ static_cast<std::uint64_t>(firstRegister);
    for (std::size_t index = 0; index < palette.size; ++index) {
        const RgbColor color = palette.colors[index];
        hash ^= (static_cast<std::uint32_t>(color.red) << 16) |
            (static_cast<std::uint32_t>(color.green) << 8) | color.blue;
        hash *= 1099511628211ULL;
    }
    hash ^= palette.size;
    return hash * 1099511628211ULL;
}

int clampChannel(const int value) noexcept
{
    return std::clamp(value, 0, 255);
}

int quantizeChannel(const std::uint8_t channel, const int levels) noexcept
{
    if (levels <= 1) {
        return 0;
    }
    return (channel * (levels - 1) + 127) / 255;
}

void applyFloydSteinberg(std::vector<std::uint8_t>& pixels, const int width, const int height,
                         const int levels)
{
    int quantizedValues[256];
    for (int value = 0; value < 256; ++value) {
        const int quantized = quantizeChannel(static_cast<std::uint8_t>(value), levels);
        quantizedValues[value] = levels > 1 ? quantized * 255 / (levels - 1) : 0;
    }

    const int stride = width * 3;
    for (int y = 0; y < height; ++y) {
        std::uint8_t* row = pixels.data() + static_cast<std::size_t>(y) * stride;
        std::uint8_t* next = y + 1 < height ? row + stride : nullptr;
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const int offset = x * 3 + channel;
                const int oldValue = row[offset];
                const int newValue = quantizedValues[oldValue];
                const int error = oldValue - newValue;
                row[offset] = static_cast<std::uint8_t>(newValue);

                const int error7 = error * 7 / 16;
                const int error5 = error * 5 / 16;
                const int error3 = error * 3 / 16;
                const int error1 = error - error7 - error5 - error3;
                if (x + 1 < width) {
                    row[offset + 3] = static_cast<std::uint8_t>(clampChannel(row[offset + 3] + error7));
                }
                if (next != nullptr) {
                    if (x > 0) {
                        next[offset - 3] = static_cast<std::uint8_t>(clampChannel(next[offset - 3] + error3));
                    }
                    next[offset] = static_cast<std::uint8_t>(clampChannel(next[offset] + error5));
                    if (x + 1 < width) {
                        next[offset + 3] = static_cast<std::uint8_t>(clampChannel(next[offset + 3] + error1));
                    }
                }
            }
        }
    }
}

void applyOrderedDither(std::vector<std::uint8_t>& pixels, const int width, const int height,
                        const int levels)
{
    static constexpr int bayer[4][4] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 },
    };
    const int stride = width * 3;
    const float step = levels > 1 ? 255.0f / (levels - 1) : 255.0f;
    for (int y = 0; y < height; ++y) {
        auto* row = pixels.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const float offset = ((bayer[y & 3][x & 3] + 0.5f) / 16.0f - 0.5f) * step;
            for (int channel = 0; channel < 3; ++channel) {
                row[x * 3 + channel] = static_cast<std::uint8_t>(
                    clampChannel(static_cast<int>(std::lround(row[x * 3 + channel] + offset))));
            }
        }
    }
}

}

class EncoderParallelExecutor {
public:
    using RowFunction = void (*)(void*, int) noexcept;

    explicit EncoderParallelExecutor(const int requestedThreads)
    {
        const unsigned hardware = std::max(1U, std::thread::hardware_concurrency());
        threadCount = requestedThreads > 0
            ? std::clamp(requestedThreads, 1, 16)
            : static_cast<int>(std::min(4U, hardware));
    }

    ~EncoderParallelExecutor()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            ++generation;
        }
        start.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }

    bool usefulFor(const int rows, const int pixels) const noexcept
    {
        return threadCount > 1 && rows >= threadCount * 4 && pixels >= 128 * 128;
    }

    void run(const int rows, RowFunction function, void* context)
    {
        ensureWorkers();
        if (workers.empty()) {
            for (int row = 0; row < rows; ++row) function(context, row);
            return;
        }
        {
            std::lock_guard lock(mutex);
            rowFunction = function;
            rowContext = context;
            rowCount = rows;
            nextRow.store(0, std::memory_order_relaxed);
            remainingWorkers = workers.size();
            ++generation;
        }
        start.notify_all();
        processRows(function, context, rows);
        std::unique_lock lock(mutex);
        finished.wait(lock, [&] { return remainingWorkers == 0; });
    }

private:
    void ensureWorkers()
    {
        if (started || threadCount <= 1) return;
        started = true;
        try {
            workers.reserve(static_cast<std::size_t>(threadCount - 1));
            for (int index = 1; index < threadCount; ++index) {
                workers.emplace_back([this] { workerLoop(); });
            }
        }
        catch (...) {
            threadCount = static_cast<int>(workers.size()) + 1;
        }
    }

    void processRows(RowFunction function, void* context, const int rows) noexcept
    {
        for (;;) {
            const int row = nextRow.fetch_add(1, std::memory_order_relaxed);
            if (row >= rows) return;
            function(context, row);
        }
    }

    void workerLoop()
    {
        std::uint64_t observedGeneration = 0;
        for (;;) {
            RowFunction function = nullptr;
            void* context = nullptr;
            int rows = 0;
            {
                std::unique_lock lock(mutex);
                start.wait(lock, [&] { return stopping || generation != observedGeneration; });
                if (stopping) return;
                observedGeneration = generation;
                function = rowFunction;
                context = rowContext;
                rows = rowCount;
            }
            processRows(function, context, rows);
            {
                std::lock_guard lock(mutex);
                if (--remainingWorkers == 0) finished.notify_one();
            }
        }
    }

    std::mutex mutex;
    std::condition_variable start;
    std::condition_variable finished;
    std::vector<std::thread> workers;
    std::atomic<int> nextRow{ 0 };
    RowFunction rowFunction = nullptr;
    void* rowContext = nullptr;
    int rowCount = 0;
    std::size_t remainingWorkers = 0;
    std::uint64_t generation = 0;
    int threadCount = 1;
    bool stopping = false;
    bool started = false;
};

bool sixelAvx2Supported() noexcept
{
    return IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE) != FALSE;
}

bool sixelAvx2Enabled() noexcept
{
    return avx2Mode.load(std::memory_order_relaxed) != 0 && sixelAvx2Supported();
}

void setSixelAvx2ModeForTesting(const int mode) noexcept
{
    avx2Mode.store(std::clamp(mode, -1, 0), std::memory_order_relaxed);
}

VideoSixelEncoder::VideoSixelEncoder(const SixelOptions& opts) : options(opts)
{
    options.maxColors = std::clamp(options.maxColors, 2, 256);
    options.maxFrameColors = std::clamp(options.maxFrameColors, 1, options.maxColors - 1);
    const int activeColorSlots = options.maxFrameColors + 1;
    analyzer = std::make_unique<FastColorAnalyzer>();
    colorMapper = std::make_unique<FastAdaptiveColorMapper>(activeColorSlots);
    fastColorMapper = std::make_unique<FastColorMapper>(activeColorSlots, options.paletteLevelsPerChannel);
    parallelExecutor = std::make_unique<EncoderParallelExecutor>(options.maximumThreads);
}

bool sixelAvx512Supported() noexcept
{
#ifdef PF_AVX512F_INSTRUCTIONS_AVAILABLE
    return IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE) != FALSE;
#else
    return false;
#endif
}

bool sixelAvx512Enabled() noexcept
{
    return avx2Mode.load(std::memory_order_relaxed) != 0 && sixelAvx512Supported();
}

void VideoSixelEncoder::prepareFrame(const FrameView& frame)
{
    beginLogicalFrame();
    adaptiveFramePrepared = false;
    if (!options.useAdaptivePalette || !frame.isValid()) return;

    analyzer->analyzeFrameFast(frame, pixelLayout(frame.format));
    float sceneDifference = 1.0f;
    if (colorMapper->hasHistogram) {
        sceneDifference = 0.0f;
        for (std::size_t index = 0; index < analyzer->luminanceHistogram.size(); ++index) {
            sceneDifference += std::abs(analyzer->luminanceHistogram[index] -
                                        colorMapper->previousHistogram[index]);
        }
        sceneDifference *= 0.5f;
    }
    if (!hasAdaptivePalette || paletteAge >= options.adaptivePaletteLockFrames ||
        sceneDifference >= options.sceneCutThreshold) {
        colorMapper->buildOptimalPalette(*analyzer);
        paletteAge = 0;
        hasAdaptivePalette = true;
    }
    else {
        ++paletteAge;
    }
    adaptiveFramePrepared = true;
}

void VideoSixelEncoder::prepareFrame(const IndexedFrameView&)
{
    beginLogicalFrame();
    adaptiveFramePrepared = false;
}

void VideoSixelEncoder::prepareRegionalFrame(const FrameView& frame)
{
    if (options.useAdaptivePalette && options.independentRegionQuantization) {
        beginLogicalFrame();
        adaptiveFramePrepared = false;
        return;
    }
    prepareFrame(frame);
}

void VideoSixelEncoder::prepareRegion(const FrameView& frame, const DamageRegion& region)
{
    if (!options.useAdaptivePalette || !options.independentRegionQuantization ||
        !frame.isValid() || !region.isValid()) {
        return;
    }
    const FrameView view{
        .data = frame.data + static_cast<std::ptrdiff_t>(region.y) * frame.stride +
            static_cast<std::ptrdiff_t>(region.x) * bytesPerPixel(frame.format),
        .width = region.width,
        .height = region.height,
        .stride = frame.stride,
        .format = frame.format,
    };
    analyzer->analyzeFrameFast(view, pixelLayout(view.format));
    colorMapper->buildOptimalPalette(*analyzer);
    paletteAge = 0;
    hasAdaptivePalette = true;
    adaptiveFramePrepared = true;
}

void VideoSixelEncoder::beginLogicalFrame() noexcept
{
    if (paletteRegistersValid && options.paletteRefreshFrames > 0 &&
        framesSincePaletteRefresh < options.paletteRefreshFrames) {
        ++framesSincePaletteRefresh;
    }
}

bool VideoSixelEncoder::shouldEmitPalette(const std::uint64_t signature) noexcept
{
    if (!options.persistPaletteRegisters) return true;
    const bool refreshDue = options.paletteRefreshFrames > 0 &&
        framesSincePaletteRefresh >= options.paletteRefreshFrames;
    if (!paletteRegistersValid || signature != paletteSignature || refreshDue) {
        paletteRegistersValid = true;
        paletteSignature = signature;
        framesSincePaletteRefresh = 0;
        return true;
    }
    return false;
}

void VideoSixelEncoder::reset() noexcept
{
    paletteAge = 0;
    hasAdaptivePalette = false;
    adaptiveFramePrepared = false;
    paletteRegistersValid = false;
    paletteSignature = 0;
    framesSincePaletteRefresh = 0;
    colorMapper->hasHistogram = false;
    colorMapper->rgbToColorNum.clear();
}

std::string_view VideoSixelEncoder::encodeFrame(const FrameView& frame)
{
    return frame.isValid() ? encodeView(frame, pixelLayout(frame.format)) : std::string_view{};
}

std::string_view VideoSixelEncoder::encodeFrame(const IndexedFrameView& frame)
{
    return frame.isValid() ? encodeView(frame) : std::string_view{};
}

std::string_view VideoSixelEncoder::encodeRegion(const FrameView& frame, const DamageRegion& region)
{
    const int sourcePixelStride = bytesPerPixel(frame.format);
    if (!frame.isValid() || !region.isValid() ||
        region.x < 0 || region.y < 0 || region.x + region.width > frame.width ||
        region.y + region.height > frame.height) {
        return {};
    }

    const FrameView view{
        .data = frame.data + static_cast<std::ptrdiff_t>(region.y) * frame.stride +
            region.x * sourcePixelStride,
        .width = region.width,
        .height = region.height,
        .stride = frame.stride,
        .format = frame.format,
    };
    return encodeView(view, pixelLayout(frame.format));
}

std::string_view VideoSixelEncoder::encodeRegion(const IndexedFrameView& frame,
                                                 const DamageRegion& region)
{
    if (!frame.isValid() || !region.isValid() || region.x < 0 || region.y < 0 ||
        region.x + region.width > frame.width || region.y + region.height > frame.height) {
        return {};
    }

    return encodeView({
        .indices = frame.indices + static_cast<std::ptrdiff_t>(region.y) * frame.stride + region.x,
        .width = region.width,
        .height = region.height,
        .stride = frame.stride,
        .palette = frame.palette,
    });
}

std::string_view VideoSixelEncoder::encodeView(const FrameView& frame, const PixelLayout layout)
{
    limitExceeded = false;
    FrameView source = frame;
    PixelLayout sourceLayout = layout;
    if (options.dither != DitherMode::None) {
        const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 3;
        workingBuffer.resize(rowBytes * frame.height);
        const int sourceStride = pixelStride(layout);
        for (int row = 0; row < frame.height; ++row) {
            const auto* input = frame.data + static_cast<std::ptrdiff_t>(row) * frame.stride;
            auto* output = workingBuffer.data() + static_cast<std::size_t>(row) * rowBytes;
            for (int column = 0; column < frame.width; ++column) {
                const PixelChannels channels = readPixel(input + column * sourceStride, layout);
                output[column * 3] = channels.red;
                output[column * 3 + 1] = channels.green;
                output[column * 3 + 2] = channels.blue;
            }
        }
        if (options.dither == DitherMode::FloydSteinberg) {
            applyFloydSteinberg(workingBuffer, frame.width, frame.height,
                                options.paletteLevelsPerChannel);
        }
        else {
            applyOrderedDither(workingBuffer, frame.width, frame.height,
                               options.paletteLevelsPerChannel);
        }
        source = {
            .data = workingBuffer.data(),
            .width = frame.width,
            .height = frame.height,
            .stride = static_cast<std::ptrdiff_t>(rowBytes),
            .format = PixelFormat::RGB24,
        };
        sourceLayout = PixelLayout::RGB;
    }

    const int totalPixels = source.width * source.height;
    if (paletteIndices.size() < static_cast<std::size_t>(totalPixels)) {
        paletteIndices.resize(totalPixels);
    }

    int colorCount = 1;
    if (options.useAdaptivePalette) {
        if (!adaptiveFramePrepared) {
            analyzer->analyzeFrameFast(source, sourceLayout);
            float sceneDifference = 1.0f;
            if (colorMapper->hasHistogram) {
                sceneDifference = 0.0f;
                for (std::size_t index = 0; index < analyzer->luminanceHistogram.size(); ++index) {
                    sceneDifference += std::abs(analyzer->luminanceHistogram[index] -
                                                colorMapper->previousHistogram[index]);
                }
                sceneDifference *= 0.5f;
            }
            if (!hasAdaptivePalette || paletteAge >= options.adaptivePaletteLockFrames ||
                sceneDifference >= options.sceneCutThreshold) {
                colorMapper->buildOptimalPalette(*analyzer);
                paletteAge = 0;
                hasAdaptivePalette = true;
            }
            else {
                ++paletteAge;
            }
        }
        colorCount = colorMapper->nextColorNum;
        struct AdaptiveMapContext {
            const FrameView* source;
            PixelLayout layout;
            FastAdaptiveColorMapper* mapper;
            std::uint8_t* indices;
        } context{ &source, sourceLayout, colorMapper.get(), paletteIndices.data() };
        const auto mapRow = +[](void* raw, const int y) noexcept {
            auto& task = *static_cast<AdaptiveMapContext*>(raw);
            const int sourceStride = pixelStride(task.layout);
            const std::uint8_t* row = task.source->data +
                static_cast<std::ptrdiff_t>(y) * task.source->stride;
            std::uint8_t* mapped = task.indices +
                static_cast<std::size_t>(y) * task.source->width;
            for (int x = 0; x < task.source->width; ++x) {
                const std::uint8_t* pixel = row + x * sourceStride;
                const PixelChannels channels = readPixel(pixel, task.layout);
                mapped[x] = static_cast<std::uint8_t>(task.mapper->getColorNumberReadOnly(
                    channels.red, channels.green, channels.blue));
            }
        };
        if (parallelExecutor->usefulFor(source.height, totalPixels)) {
            parallelExecutor->run(source.height, mapRow, &context);
        }
        else {
            for (int y = 0; y < source.height; ++y) mapRow(&context, y);
        }
    }
    else {
        fastColorMapper->reset();
        const bool simdEligible = pixelStride(sourceLayout) >= 3 && source.width >= 64;
        const int simdMode = simdEligible && sixelAvx512Enabled() ? 2
            : simdEligible && sixelAvx2Enabled() ? 1 : 0;
        struct FixedMapContext {
            const FrameView* source;
            PixelLayout layout;
            const FastColorMapper* mapper;
            std::uint8_t* indices;
            int simd;
        } context{ &source, sourceLayout, fastColorMapper.get(), paletteIndices.data(), simdMode };
        const auto mapRow = +[](void* raw, const int y) noexcept {
            auto& task = *static_cast<FixedMapContext*>(raw);
            const std::uint8_t* row = task.source->data +
                static_cast<std::ptrdiff_t>(y) * task.source->stride;
            std::uint8_t* mapped = task.indices +
                static_cast<std::size_t>(y) * task.source->width;
            if (task.simd == 2) {
                mapPaletteRowAvx512(row, task.source->width, task.layout,
                                    task.mapper->rgbLookup32.data(), mapped);
            }
            else if (task.simd == 1) {
                mapPaletteRowAvx2(row, task.source->width, task.layout,
                                  task.mapper->rgbLookup32.data(), mapped);
            }
            else {
                const int sourceStride = pixelStride(task.layout);
                for (int x = 0; x < task.source->width; ++x) {
                    const std::uint8_t* pixel = row + x * sourceStride;
                    const PixelChannels channels = readPixel(pixel, task.layout);
                    const int lr = (static_cast<int>(channels.red) * 31 + 127) / 255;
                    const int lg = (static_cast<int>(channels.green) * 31 + 127) / 255;
                    const int lb = (static_cast<int>(channels.blue) * 31 + 127) / 255;
                    mapped[x] = task.mapper->rgbLookup[(lr * 32 + lg) * 32 + lb];
                }
            }
        };
        if (parallelExecutor->usefulFor(source.height, totalPixels)) {
            parallelExecutor->run(source.height, mapRow, &context);
        }
        else {
            for (int y = 0; y < source.height; ++y) mapRow(&context, y);
        }
        for (int pixel = 0; pixel < totalPixels; ++pixel) {
            const int color = paletteIndices[pixel];
            if (!fastColorMapper->colorUsed[color]) {
                fastColorMapper->colorUsed[color] = true;
                ++fastColorMapper->usedColorCount;
            }
        }
        colorCount = fastColorMapper->nextColorNum;
    }

    outputBuffer.clear();
    const std::size_t requested = static_cast<std::size_t>(totalPixels / 3);
    outputBuffer.reserve(outputLimit > 0 ? std::min(requested, outputLimit) : requested);
    SixelOutput output(outputBuffer, outputLimit);
    beginSixel(output, options);
    emitRasterAttributes(output, source.width, source.height);
    if (options.useAdaptivePalette) {
        const bool definePalette = shouldEmitPalette(paletteHash(
            colorMapper->palette.data(), static_cast<std::size_t>(colorMapper->nextColorNum)));
        if (definePalette) emitPalette(output, *colorMapper);
    }
    else {
        const bool definePalette = shouldEmitPalette(paletteHash(
            fastColorMapper->colorNumToRgb.data(),
            static_cast<std::size_t>(fastColorMapper->nextColorNum)));
        if (definePalette) {
            if (options.persistPaletteRegisters) emitFullPalette(output, *fastColorMapper);
            else emitPalette(output, *fastColorMapper);
        }
    }
    emitIndexedFrame(output, paletteIndices, source.width, source.height, colorCount);
    output.append("\x1b\\");
    limitExceeded = output.exceeded();
    lastColorCountValue = options.useAdaptivePalette ? colorCount - 1 : fastColorMapper->usedColorCount;
    return limitExceeded ? std::string_view{} : std::string_view(outputBuffer);
}

std::string_view VideoSixelEncoder::encodeView(const IndexedFrameView& frame)
{
    limitExceeded = false;
    const std::size_t totalPixels = static_cast<std::size_t>(frame.width) * frame.height;
    const int registerOffset = frame.palette.size == 256 ? 0 : 1;
    paletteIndices.resize(totalPixels);
    for (int row = 0; row < frame.height; ++row) {
        const std::uint8_t* source = frame.indices + static_cast<std::ptrdiff_t>(row) * frame.stride;
        std::uint8_t* destination = paletteIndices.data() + static_cast<std::size_t>(row) * frame.width;
        for (int column = 0; column < frame.width; ++column) {
            destination[column] = static_cast<std::uint8_t>(source[column] + registerOffset);
        }
    }

    outputBuffer.clear();
    const std::size_t requested = totalPixels / 3;
    outputBuffer.reserve(outputLimit > 0 ? std::min(requested, outputLimit) : requested);
    SixelOutput output(outputBuffer, outputLimit);
    beginSixel(output, options);
    emitRasterAttributes(output, frame.width, frame.height);
    if (shouldEmitPalette(paletteHash(frame.palette, registerOffset))) {
        emitPalette(output, frame.palette, registerOffset);
    }
    emitIndexedFrame(output, paletteIndices, frame.width, frame.height,
                       static_cast<int>(frame.palette.size + registerOffset));
    output.append("\x1b\\");
    limitExceeded = output.exceeded();
    lastColorCountValue = static_cast<int>(frame.palette.size);
    return limitExceeded ? std::string_view{} : std::string_view(outputBuffer);
}

std::size_t VideoSixelEncoder::scratchCapacity() const noexcept
{
    return paletteIndices.capacity() + workingBuffer.capacity();
}

VideoSixelEncoder::~VideoSixelEncoder() = default;

}
