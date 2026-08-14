/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>
#include <rasterm/rasterm.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace rasterm_c_detail {

constexpr size_t errorCapacity = 512;
constexpr size_t maximumDamageRectangles = 65536;
thread_local char globalLastError[errorCapacity]{};

void setGlobalError(const std::string_view message) noexcept
{
    const size_t count = (std::min)(message.size(), errorCapacity - 1);
    std::memcpy(globalLastError, message.data(), count);
    globalLastError[count] = '\0';
}

rasterm_result failure(const rasterm_result code, const std::string_view message) noexcept
{
    setGlobalError(message);
    return code;
}

void succeeded() noexcept
{
    globalLastError[0] = '\0';
}

rasterm_result copyError(const std::string_view message, char* buffer,
                         const size_t capacity, size_t* requiredSize) noexcept
{
    const size_t required = message.size() + 1;
    if (requiredSize != nullptr) *requiredSize = required;
    if (buffer == nullptr || capacity < required) {
        return RASTERM_ERROR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buffer, message.data(), message.size());
    buffer[message.size()] = '\0';
    return RASTERM_SUCCESS;
}

class CallbackSink final : public rasterm::OutputSink {
public:
    CallbackSink(void* context, const rasterm_write_callback write,
                 const rasterm_flush_callback flush) noexcept :
        context(context), writeCallback(write), flushCallback(flush) {}

    bool write(const std::string_view bytes) noexcept override
    {
        return writeCallback != nullptr &&
            writeCallback(context, bytes.data(), bytes.size()) != 0;
    }

    bool flush() noexcept override
    {
        return flushCallback != nullptr && flushCallback(context) != 0;
    }

private:
    void* context = nullptr;
    rasterm_write_callback writeCallback = nullptr;
    rasterm_flush_callback flushCallback = nullptr;
};

rasterm_result result(const rasterm::ErrorCode code) noexcept
{
    switch (code) {
    case rasterm::ErrorCode::None: return RASTERM_SUCCESS;
    case rasterm::ErrorCode::InvalidArgument: return RASTERM_ERROR_INVALID_ARGUMENT;
    case rasterm::ErrorCode::InvalidOutputHandle: return RASTERM_ERROR_INVALID_OUTPUT_HANDLE;
    case rasterm::ErrorCode::OutputIsNotConsole: return RASTERM_ERROR_OUTPUT_IS_NOT_CONSOLE;
    case rasterm::ErrorCode::ConsoleModeQueryFailed: return RASTERM_ERROR_CONSOLE_MODE_QUERY_FAILED;
    case rasterm::ErrorCode::ConsoleModeEnableFailed: return RASTERM_ERROR_CONSOLE_MODE_ENABLE_FAILED;
    case rasterm::ErrorCode::TerminalDimensionsUnavailable: return RASTERM_ERROR_TERMINAL_DIMENSIONS_UNAVAILABLE;
    case rasterm::ErrorCode::UnsupportedTerminal: return RASTERM_ERROR_UNSUPPORTED_TERMINAL;
    case rasterm::ErrorCode::StdoutModeFailed: return RASTERM_ERROR_STDOUT_MODE_FAILED;
    case rasterm::ErrorCode::ControlHandlerRegistrationFailed: return RASTERM_ERROR_CONTROL_HANDLER_REGISTRATION_FAILED;
    case rasterm::ErrorCode::DiagnosticOpenFailed: return RASTERM_ERROR_DIAGNOSTIC_OPEN_FAILED;
    case rasterm::ErrorCode::OutputWriteFailed: return RASTERM_ERROR_OUTPUT_WRITE_FAILED;
    case rasterm::ErrorCode::OutputFlushFailed: return RASTERM_ERROR_OUTPUT_FLUSH_FAILED;
    case rasterm::ErrorCode::OutputBufferLimitExceeded: return RASTERM_ERROR_OUTPUT_BUFFER_LIMIT_EXCEEDED;
    case rasterm::ErrorCode::PresenterThreadStartFailed: return RASTERM_ERROR_PRESENTER_THREAD_START_FAILED;
    case rasterm::ErrorCode::InitializationException: return RASTERM_ERROR_INITIALIZATION_EXCEPTION;
    case rasterm::ErrorCode::RenderingException: return RASTERM_ERROR_RENDERING_EXCEPTION;
    case rasterm::ErrorCode::BufferTooSmall: return RASTERM_ERROR_BUFFER_TOO_SMALL;
    case rasterm::ErrorCode::ApiVersionMismatch: return RASTERM_ERROR_API_VERSION_MISMATCH;
    case rasterm::ErrorCode::OutOfMemory: return RASTERM_ERROR_OUT_OF_MEMORY;
    case rasterm::ErrorCode::PresenterStopped: return RASTERM_ERROR_PRESENTER_STOPPED;
    case rasterm::ErrorCode::TimedOut: return RASTERM_ERROR_TIMED_OUT;
    }
    return RASTERM_ERROR_RENDERING_EXCEPTION;
}

bool validColor(const rasterm_color_metadata& color) noexcept
{
    return color.primaries >= RASTERM_PRIMARIES_UNSPECIFIED &&
        color.primaries <= RASTERM_PRIMARIES_DISPLAY_P3 &&
        color.transfer >= RASTERM_TRANSFER_UNSPECIFIED && color.transfer <= RASTERM_TRANSFER_HLG &&
        color.matrix >= RASTERM_MATRIX_UNSPECIFIED && color.matrix <= RASTERM_MATRIX_BT2020_NCL &&
        color.range >= RASTERM_RANGE_UNSPECIFIED && color.range <= RASTERM_RANGE_FULL &&
        std::isfinite(color.reference_white_nits) && color.reference_white_nits > 0.0f &&
        std::isfinite(color.mastering_peak_nits) && color.mastering_peak_nits > 0.0f;
}

rasterm::ColorMetadata color(const rasterm_color_metadata& value) noexcept
{
    const auto primaries = [&] {
        switch (value.primaries) {
        case RASTERM_PRIMARIES_BT709: return rasterm::ColorPrimaries::Bt709;
        case RASTERM_PRIMARIES_BT2020: return rasterm::ColorPrimaries::Bt2020;
        case RASTERM_PRIMARIES_DISPLAY_P3: return rasterm::ColorPrimaries::DisplayP3;
        default: return rasterm::ColorPrimaries::Unspecified;
        }
    }();
    const auto transfer = [&] {
        switch (value.transfer) {
        case RASTERM_TRANSFER_SRGB: return rasterm::TransferFunction::Srgb;
        case RASTERM_TRANSFER_LINEAR: return rasterm::TransferFunction::Linear;
        case RASTERM_TRANSFER_BT709: return rasterm::TransferFunction::Bt709;
        case RASTERM_TRANSFER_GAMMA22: return rasterm::TransferFunction::Gamma22;
        case RASTERM_TRANSFER_PQ: return rasterm::TransferFunction::Pq;
        case RASTERM_TRANSFER_HLG: return rasterm::TransferFunction::Hlg;
        default: return rasterm::TransferFunction::Unspecified;
        }
    }();
    const auto matrix = [&] {
        switch (value.matrix) {
        case RASTERM_MATRIX_IDENTITY: return rasterm::MatrixCoefficients::Identity;
        case RASTERM_MATRIX_BT601: return rasterm::MatrixCoefficients::Bt601;
        case RASTERM_MATRIX_BT709: return rasterm::MatrixCoefficients::Bt709;
        case RASTERM_MATRIX_BT2020_NCL: return rasterm::MatrixCoefficients::Bt2020NonConstant;
        default: return rasterm::MatrixCoefficients::Unspecified;
        }
    }();
    const auto range = value.range == RASTERM_RANGE_LIMITED ? rasterm::ColorRange::Limited
        : value.range == RASTERM_RANGE_FULL ? rasterm::ColorRange::Full
        : rasterm::ColorRange::Unspecified;
    return {
        primaries, transfer, matrix, range,
        value.reference_white_nits,
        value.mastering_peak_nits,
    };
}

rasterm::QualityProfile quality(const int32_t value) noexcept
{
    if (value == RASTERM_QUALITY_ADAPTIVE_VIDEO) return rasterm::QualityProfile::AdaptiveVideo;
    if (value == RASTERM_QUALITY_HIGH) return rasterm::QualityProfile::HighQuality;
    return rasterm::QualityProfile::Realtime;
}

rasterm::ToneMapOperator toneMap(const int32_t value) noexcept
{
    if (value == RASTERM_TONE_MAP_REINHARD) return rasterm::ToneMapOperator::Reinhard;
    if (value == RASTERM_TONE_MAP_HABLE) return rasterm::ToneMapOperator::Hable;
    if (value == RASTERM_TONE_MAP_ACES) return rasterm::ToneMapOperator::Aces;
    return rasterm::ToneMapOperator::None;
}

rasterm::DitherMode dither(const int32_t value) noexcept
{
    if (value == RASTERM_DITHER_ORDERED_BAYER_4X4) return rasterm::DitherMode::OrderedBayer4x4;
    if (value == RASTERM_DITHER_FLOYD_STEINBERG) return rasterm::DitherMode::FloydSteinberg;
    return rasterm::DitherMode::None;
}

rasterm::PixelFormat pixelFormat(const int32_t value) noexcept
{
    if (value == RASTERM_PIXEL_BGR24) return rasterm::PixelFormat::BGR24;
    if (value == RASTERM_PIXEL_RGBA32) return rasterm::PixelFormat::RGBA32;
    if (value == RASTERM_PIXEL_BGRA32) return rasterm::PixelFormat::BGRA32;
    if (value == RASTERM_PIXEL_RGB565) return rasterm::PixelFormat::RGB565;
    if (value == RASTERM_PIXEL_0RGB1555) return rasterm::PixelFormat::XRGB1555;
    if (value == RASTERM_PIXEL_RGBA4444) return rasterm::PixelFormat::RGBA4444;
    return rasterm::PixelFormat::RGB24;
}

int32_t capability(const rasterm::CapabilitySupport value) noexcept
{
    if (value == rasterm::CapabilitySupport::Unsupported) return RASTERM_CAPABILITY_UNSUPPORTED;
    if (value == rasterm::CapabilitySupport::Supported) return RASTERM_CAPABILITY_SUPPORTED;
    return RASTERM_CAPABILITY_UNKNOWN;
}

rasterm::CapabilitySupport capabilitySupport(const int32_t value) noexcept
{
    if (value == RASTERM_CAPABILITY_UNSUPPORTED) return rasterm::CapabilitySupport::Unsupported;
    if (value == RASTERM_CAPABILITY_SUPPORTED) return rasterm::CapabilitySupport::Supported;
    return rasterm::CapabilitySupport::Unknown;
}

void defaultColor(rasterm_color_metadata& color, const bool source) noexcept
{
    color.primaries = source ? RASTERM_PRIMARIES_UNSPECIFIED : RASTERM_PRIMARIES_BT709;
    color.transfer = source ? RASTERM_TRANSFER_UNSPECIFIED : RASTERM_TRANSFER_SRGB;
    color.matrix = source ? RASTERM_MATRIX_UNSPECIFIED : RASTERM_MATRIX_IDENTITY;
    color.range = source ? RASTERM_RANGE_UNSPECIFIED : RASTERM_RANGE_FULL;
    color.reference_white_nits = RASTERM_DEFAULT_REFERENCE_WHITE_NITS;
    color.mastering_peak_nits = RASTERM_DEFAULT_MASTERING_PEAK_NITS;
}

void defaultMetadata(rasterm_frame_metadata& metadata) noexcept
{
    std::memset(&metadata, 0, sizeof(metadata));
    metadata.timestamp_nanoseconds = RASTERM_C_UNKNOWN_TIMESTAMP;
    defaultColor(metadata.color, false);
    defaultColor(metadata.source_color, true);
}

bool validMetadata(const rasterm_frame_metadata& metadata) noexcept
{
    if (!validColor(metadata.color) || !validColor(metadata.source_color) ||
        metadata.damage_count > maximumDamageRectangles ||
        (metadata.damage_count > 0 && metadata.damage_rects == nullptr) ||
        (metadata.damage_supplied == 0 &&
         (metadata.damage_count != 0 || metadata.damage_rects != nullptr))) {
        return false;
    }
    return true;
}

void fillStats(const rasterm::RenderStats& source, rasterm_render_stats& destination) noexcept
{
    destination.error = static_cast<int32_t>(result(source.error));
    destination.rendered = source.rendered;
    destination.full_frame = source.fullFrame;
    destination.payload_bytes = static_cast<uint64_t>(source.payloadBytes);
    destination.dirty_regions = source.dirtyRegions;
    destination.width = source.width;
    destination.height = source.height;
    destination.colors_used = source.colorsUsed;
    destination.encode_milliseconds = source.encodeMilliseconds;
    destination.present_milliseconds = source.presentMilliseconds;
    destination.frames_per_second = source.framesPerSecond;
    destination.payload_bytes_per_second = source.payloadBytesPerSecond;
    destination.terminal_write_p95_milliseconds = source.terminalWriteP95Milliseconds;
    destination.terminal_write_p99_milliseconds = source.terminalWriteP99Milliseconds;
    destination.output_failures = source.outputFailures;
    destination.backpressure_events = source.backpressureEvents;
    destination.payload_limit_drops = source.payloadLimitDrops;
    destination.wire_bytes = static_cast<uint64_t>(source.wireBytes);
    destination.scratch_bytes = static_cast<uint64_t>(source.scratchBytes);
    destination.output_capacity_bytes = static_cast<uint64_t>(source.outputCapacityBytes);
    destination.validation_milliseconds = source.validationMilliseconds;
    destination.conversion_milliseconds = source.conversionMilliseconds;
}

}

struct rasterm_engine {
    std::unique_ptr<rasterm_c_detail::CallbackSink> sink;
    rasterm::Engine engine;
    std::vector<rasterm::DamageRect> damage;
    std::vector<rasterm::RgbColor> palette;
    std::string lastError;
};

struct rasterm_presenter {
    std::unique_ptr<rasterm_c_detail::CallbackSink> sink;
    rasterm::Presenter presenter;
    std::vector<rasterm::DamageRect> damage;
    std::vector<rasterm::RgbColor> palette;
    std::string lastError;
};

namespace rasterm_c_detail {

bool copyMetadata(std::vector<rasterm::DamageRect>& damage,
                  const rasterm_frame_metadata& source,
                  rasterm::FrameMetadata& destination)
{
    if (!validMetadata(source)) return false;
    damage.clear();
    damage.reserve(source.damage_count);
    for (size_t index = 0; index < source.damage_count; ++index) {
        const rasterm_damage_rect& rectangle = source.damage_rects[index];
        damage.push_back({ rectangle.x, rectangle.y, rectangle.width, rectangle.height });
    }
    destination = {
        .frameId = source.frame_id,
        .timestampNanoseconds = source.timestamp_nanoseconds,
        .color = color(source.color),
        .sourceColor = color(source.source_color),
        .damage = {
            damage.empty() ? nullptr : damage.data(),
            damage.size(),
            source.damage_supplied != 0,
        },
    };
    return true;
}

bool validEngineOptions(const rasterm_engine_options& options) noexcept
{
    return options.struct_size >= RASTERM_ENGINE_OPTIONS_V1_SIZE &&
        options.api_version == RASTERM_C_API_VERSION &&
        options.quality >= RASTERM_QUALITY_REALTIME && options.quality <= RASTERM_QUALITY_HIGH &&
        options.tone_map >= RASTERM_TONE_MAP_NONE && options.tone_map <= RASTERM_TONE_MAP_ACES &&
        options.realtime_dither >= RASTERM_DITHER_NONE &&
        options.realtime_dither <= RASTERM_DITHER_FLOYD_STEINBERG &&
        (options.write == nullptr) == (options.flush == nullptr) &&
        std::isfinite(options.backpressure_threshold_milliseconds) &&
        options.backpressure_threshold_milliseconds >= 0.0 &&
        std::isfinite(options.output_peak_nits) && options.output_peak_nits > 0.0f &&
        options.adaptive_palette_lock_frames >= 0 &&
        std::isfinite(options.scene_cut_threshold) && options.scene_cut_threshold >= 0.0f &&
        options.scene_cut_threshold <= 1.0f &&
        options.sixel_support_override >= RASTERM_CAPABILITY_UNKNOWN &&
        options.sixel_support_override <= RASTERM_CAPABILITY_SUPPORTED &&
        options.synchronized_output_override >= RASTERM_CAPABILITY_UNKNOWN &&
        options.synchronized_output_override <= RASTERM_CAPABILITY_SUPPORTED &&
        options.override_columns >= 0 && options.override_rows >= 0 &&
        options.override_pixel_width >= 0 && options.override_pixel_height >= 0 &&
        options.override_cell_pixel_width >= 0 && options.override_cell_pixel_height >= 0 &&
        ((options.override_cell_pixel_width == 0) ==
         (options.override_cell_pixel_height == 0)) &&
        options.palette_refresh_frames >= 0 && options.maximum_encoder_threads >= 0;
}

rasterm::EngineOptions engineOptions(const rasterm_engine_options& options,
                                     rasterm::OutputSink* sink) noexcept
{
    return {
        .quality = quality(options.quality),
        .useAlternateScreen = options.use_alternate_screen != 0,
        .preserveCursor = options.preserve_cursor != 0,
        .enableDirtyRegions = options.enable_dirty_regions != 0,
        .requireSixelSupport = options.require_sixel_support != 0,
        .useSynchronizedOutput = options.use_synchronized_output != 0,
        .maximumOutputBytes = static_cast<size_t>(options.maximum_output_bytes),
        .backpressureThresholdMilliseconds = options.backpressure_threshold_milliseconds,
        .color = {
            .convertToSrgb = options.convert_to_srgb != 0,
            .toneMap = toneMap(options.tone_map),
            .outputPeakNits = options.output_peak_nits,
            .realtimeDither = dither(options.realtime_dither),
            .adaptivePaletteLockFrames = options.adaptive_palette_lock_frames,
            .sceneCutThreshold = options.scene_cut_threshold,
        },
        .output = sink,
        .terminalOverrides = {
            .sixel = capabilitySupport(options.sixel_support_override),
            .synchronizedOutput = capabilitySupport(options.synchronized_output_override),
            .geometry = {
                options.override_columns,
                options.override_rows,
                options.override_pixel_width,
                options.override_pixel_height,
                options.override_cell_pixel_width,
                options.override_cell_pixel_height,
            },
        },
        .encoder = {
            .persistPaletteRegisters = options.persist_palette_registers != 0,
            .paletteRefreshFrames = options.palette_refresh_frames,
            .outputChunkBytes = static_cast<size_t>(options.output_chunk_bytes),
            .maximumThreads = options.maximum_encoder_threads,
            .independentRegionQuantization = options.independent_region_quantization != 0,
        },
    };
}

void fillPresenterStats(const rasterm::PresenterStats& source,
                        rasterm_presenter_stats& destination) noexcept
{
    destination.submitted_frames = source.submittedFrames;
    destination.presented_frames = source.presentedFrames;
    destination.replaced_frames = source.replacedFrames;
    fillStats(source.latestRender, destination.latest_render);
    destination.unchanged_frames = source.unchangedFrames;
    destination.failed_frames = source.failedFrames;
    destination.rejected_frames = source.rejectedFrames;
    destination.cancelled_frames = source.cancelledFrames;
}

}

extern "C" {

void rasterm_engine_options_init(rasterm_engine_options* options)
{
    if (options == nullptr) return;
    std::memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(*options);
    options->api_version = RASTERM_C_API_VERSION;
    options->quality = RASTERM_DEFAULT_QUALITY;
    options->use_alternate_screen = RASTERM_DEFAULT_USE_ALTERNATE_SCREEN;
    options->preserve_cursor = RASTERM_DEFAULT_PRESERVE_CURSOR;
    options->enable_dirty_regions = RASTERM_DEFAULT_ENABLE_DIRTY_REGIONS;
    options->require_sixel_support = RASTERM_DEFAULT_REQUIRE_SIXEL_SUPPORT;
    options->use_synchronized_output = RASTERM_DEFAULT_USE_SYNCHRONIZED_OUTPUT;
    options->maximum_output_bytes = RASTERM_DEFAULT_MAXIMUM_OUTPUT_BYTES;
    options->backpressure_threshold_milliseconds = RASTERM_DEFAULT_BACKPRESSURE_MILLISECONDS;
    options->convert_to_srgb = RASTERM_DEFAULT_CONVERT_TO_SRGB;
    options->tone_map = RASTERM_DEFAULT_TONE_MAP;
    options->output_peak_nits = RASTERM_DEFAULT_OUTPUT_PEAK_NITS;
    options->realtime_dither = RASTERM_DEFAULT_REALTIME_DITHER;
    options->adaptive_palette_lock_frames = RASTERM_DEFAULT_ADAPTIVE_PALETTE_LOCK_FRAMES;
    options->scene_cut_threshold = RASTERM_DEFAULT_SCENE_CUT_THRESHOLD;
    options->persist_palette_registers = RASTERM_DEFAULT_PERSIST_PALETTE_REGISTERS;
    options->palette_refresh_frames = RASTERM_DEFAULT_PALETTE_REFRESH_FRAMES;
    options->output_chunk_bytes = RASTERM_DEFAULT_OUTPUT_CHUNK_BYTES;
    options->maximum_encoder_threads = RASTERM_DEFAULT_MAXIMUM_ENCODER_THREADS;
    options->independent_region_quantization =
        RASTERM_DEFAULT_INDEPENDENT_REGION_QUANTIZATION;
}

void rasterm_frame_init(rasterm_frame* frame)
{
    if (frame == nullptr) return;
    std::memset(frame, 0, sizeof(*frame));
    frame->struct_size = sizeof(*frame);
    frame->format = RASTERM_PIXEL_RGB24;
    rasterm_c_detail::defaultMetadata(frame->metadata);
}

void rasterm_indexed_frame_init(rasterm_indexed_frame* frame)
{
    if (frame == nullptr) return;
    std::memset(frame, 0, sizeof(*frame));
    frame->struct_size = sizeof(*frame);
    rasterm_c_detail::defaultMetadata(frame->metadata);
}

void rasterm_render_stats_init(rasterm_render_stats* stats)
{
    if (stats == nullptr) return;
    std::memset(stats, 0, sizeof(*stats));
    stats->struct_size = sizeof(*stats);
}

void rasterm_terminal_capabilities_init(rasterm_terminal_capabilities* capabilities)
{
    if (capabilities == nullptr) return;
    std::memset(capabilities, 0, sizeof(*capabilities));
    capabilities->struct_size = sizeof(*capabilities);
}

void rasterm_presenter_options_init(rasterm_presenter_options* options)
{
    if (options == nullptr) return;
    std::memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(*options);
    rasterm_engine_options_init(&options->engine);
}

void rasterm_presenter_stats_init(rasterm_presenter_stats* stats)
{
    if (stats == nullptr) return;
    std::memset(stats, 0, sizeof(*stats));
    stats->struct_size = sizeof(*stats);
    rasterm_render_stats_init(&stats->latest_render);
}

uint32_t rasterm_c_api_version(void) { return RASTERM_C_API_VERSION; }

void rasterm_version(uint32_t* major, uint32_t* minor, uint32_t* patch)
{
    if (major != nullptr) *major = RASTERM_VERSION_MAJOR;
    if (minor != nullptr) *minor = RASTERM_VERSION_MINOR;
    if (patch != nullptr) *patch = RASTERM_VERSION_PATCH;
}

rasterm_result rasterm_last_error(char* buffer, const size_t capacity,
                                  size_t* required_size)
{
    return rasterm_c_detail::copyError(rasterm_c_detail::globalLastError,
                                       buffer, capacity, required_size);
}

rasterm_result rasterm_engine_create(const rasterm_engine_options* requested,
                                     rasterm_engine** output_engine)
{
    if (output_engine == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "output_engine must not be null.");
    }
    *output_engine = nullptr;
    rasterm_engine_options defaults{};
    rasterm_engine_options_init(&defaults);
    const rasterm_engine_options& options = requested == nullptr ? defaults : *requested;
    if (options.struct_size < RASTERM_ENGINE_OPTIONS_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "rasterm_engine_options is smaller than the v1 prefix.");
    }
    if (options.api_version != RASTERM_C_API_VERSION) {
        return rasterm_c_detail::failure(RASTERM_ERROR_API_VERSION_MISMATCH,
                                         "The requested rasterm C API version is unsupported.");
    }
    if (!rasterm_c_detail::validEngineOptions(options)) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "rasterm_engine_options contains an invalid value.");
    }

    try {
        std::unique_ptr<rasterm_engine> handle(new rasterm_engine);
        rasterm::OutputSink* sink = nullptr;
        if (options.write != nullptr) {
            handle->sink = std::make_unique<rasterm_c_detail::CallbackSink>(
                options.output_context, options.write, options.flush);
            sink = handle->sink.get();
        }
        const rasterm::Status initialized = handle->engine.initialize(
            rasterm_c_detail::engineOptions(options, sink));
        handle->lastError = initialized.message;
        const rasterm_result code = rasterm_c_detail::result(initialized.code);
        *output_engine = handle.release();
        if (code == RASTERM_SUCCESS) rasterm_c_detail::succeeded();
        else rasterm_c_detail::setGlobalError(initialized.message);
        return code;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate an engine handle.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INITIALIZATION_EXCEPTION,
                                         "Unexpected exception while creating an engine.");
    }
}

void rasterm_engine_destroy(rasterm_engine* engine) { delete engine; }

rasterm_result rasterm_engine_render(rasterm_engine* engine, const rasterm_frame* frame,
                                     rasterm_render_stats* stats)
{
    if (engine == nullptr || frame == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine and frame must not be null.");
    }
    if (frame->struct_size < RASTERM_FRAME_V1_SIZE ||
        (stats != nullptr && stats->struct_size < RASTERM_RENDER_STATS_V1_SIZE)) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "A C API structure is smaller than its v1 prefix.");
    }
    if (frame->format < RASTERM_PIXEL_RGB24 ||
        frame->format > RASTERM_PIXEL_RGBA4444 || frame->stride <= 0 ||
        frame->stride > (std::numeric_limits<ptrdiff_t>::max)() ||
        frame->stride < (std::numeric_limits<ptrdiff_t>::min)()) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "Packed frame format or stride is invalid.");
    }
    try {
        rasterm::FrameMetadata metadata;
        if (!rasterm_c_detail::copyMetadata(engine->damage, frame->metadata, metadata)) {
            return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                             "Packed frame metadata is invalid.");
        }
        const rasterm::RenderStats rendered = engine->engine.renderFrame({
            frame->data, frame->width, frame->height, static_cast<ptrdiff_t>(frame->stride),
            rasterm_c_detail::pixelFormat(frame->format), metadata,
        });
        if (stats != nullptr) rasterm_c_detail::fillStats(rendered, *stats);
        engine->lastError = rendered.error == rasterm::ErrorCode::None
            ? std::string{} : engine->engine.status().message;
        const rasterm_result code = rasterm_c_detail::result(rendered.error);
        if (code == RASTERM_SUCCESS) rasterm_c_detail::succeeded();
        else rasterm_c_detail::setGlobalError(engine->lastError);
        return code;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate packed-frame resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while rendering a packed frame.");
    }
}

rasterm_result rasterm_engine_render_indexed(rasterm_engine* engine,
                                             const rasterm_indexed_frame* frame,
                                             rasterm_render_stats* stats)
{
    if (engine == nullptr || frame == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine and indexed frame must not be null.");
    }
    if (frame->struct_size < RASTERM_INDEXED_FRAME_V1_SIZE ||
        (stats != nullptr && stats->struct_size < RASTERM_RENDER_STATS_V1_SIZE)) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "A C API structure is smaller than its v1 prefix.");
    }
    if (frame->stride <= 0 ||
        frame->stride > (std::numeric_limits<ptrdiff_t>::max)() ||
        frame->stride < (std::numeric_limits<ptrdiff_t>::min)() || frame->palette_size > 256 ||
        (frame->palette_size > 0 && frame->palette == nullptr)) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "Indexed frame stride or palette is invalid.");
    }
    try {
        rasterm::FrameMetadata metadata;
        if (!rasterm_c_detail::copyMetadata(engine->damage, frame->metadata, metadata)) {
            return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                             "Indexed frame metadata is invalid.");
        }
        engine->palette.clear();
        engine->palette.reserve(frame->palette_size);
        for (size_t index = 0; index < frame->palette_size; ++index) {
            engine->palette.push_back({ frame->palette[index].red, frame->palette[index].green,
                                        frame->palette[index].blue });
        }
        const rasterm::RenderStats rendered = engine->engine.renderFrame({
            frame->indices, frame->width, frame->height, static_cast<ptrdiff_t>(frame->stride),
            { engine->palette.data(), engine->palette.size() }, metadata,
        });
        if (stats != nullptr) rasterm_c_detail::fillStats(rendered, *stats);
        engine->lastError = rendered.error == rasterm::ErrorCode::None
            ? std::string{} : engine->engine.status().message;
        const rasterm_result code = rasterm_c_detail::result(rendered.error);
        if (code == RASTERM_SUCCESS) rasterm_c_detail::succeeded();
        else rasterm_c_detail::setGlobalError(engine->lastError);
        return code;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate indexed frame resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while rendering an indexed frame.");
    }
}

rasterm_result rasterm_engine_clear(rasterm_engine* engine)
{
    if (engine == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine must not be null.");
    }
    try {
        const rasterm::Status cleared = engine->engine.clear();
        engine->lastError = cleared.message;
        const rasterm_result code = rasterm_c_detail::result(cleared.code);
        if (code == RASTERM_SUCCESS) rasterm_c_detail::succeeded();
        else rasterm_c_detail::setGlobalError(cleared.message);
        return code;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate clear frame resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while clearing the output.");
    }
}

rasterm_result rasterm_engine_reset(rasterm_engine* engine)
{
    if (engine == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine must not be null.");
    }
    try {
        engine->engine.reset();
        rasterm_c_detail::succeeded();
        return RASTERM_SUCCESS;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate reset resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while resetting the engine.");
    }
}

rasterm_result rasterm_engine_get_stats(const rasterm_engine* engine,
                                        rasterm_render_stats* stats)
{
    if (engine == nullptr || stats == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine and stats must not be null.");
    }
    if (stats->struct_size < RASTERM_RENDER_STATS_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "rasterm_render_stats is smaller than the v1 prefix.");
    }
    rasterm_c_detail::fillStats(engine->engine.stats(), *stats);
    rasterm_c_detail::succeeded();
    return RASTERM_SUCCESS;
}

rasterm_result rasterm_engine_get_capabilities(const rasterm_engine* engine,
                                               rasterm_terminal_capabilities* capabilities)
{
    if (engine == nullptr || capabilities == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine and capabilities must not be null.");
    }
    if (capabilities->struct_size < RASTERM_CAPABILITIES_V1_SIZE) {
        return rasterm_c_detail::failure(
            RASTERM_ERROR_BUFFER_TOO_SMALL,
            "rasterm_terminal_capabilities is smaller than the v1 prefix.");
    }
    const rasterm::TerminalCapabilities source = engine->engine.capabilities();
    capabilities->custom_output = source.customOutput;
    capabilities->valid_output_handle = source.validOutputHandle;
    capabilities->console_output = source.consoleOutput;
    capabilities->virtual_terminal_output = source.virtualTerminalOutput;
    capabilities->sixel = rasterm_c_detail::capability(source.sixel);
    capabilities->synchronized_output = rasterm_c_detail::capability(source.synchronizedOutput);
    capabilities->columns = source.geometry.columns;
    capabilities->rows = source.geometry.rows;
    capabilities->pixel_width = source.geometry.pixelWidth;
    capabilities->pixel_height = source.geometry.pixelHeight;
    capabilities->cell_pixel_width = source.geometry.cellPixelWidth;
    capabilities->cell_pixel_height = source.geometry.cellPixelHeight;
    rasterm_c_detail::succeeded();
    return RASTERM_SUCCESS;
}

rasterm_result rasterm_engine_last_error(const rasterm_engine* engine, char* buffer,
                                         const size_t capacity, size_t* required_size)
{
    if (engine == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "engine must not be null.");
    }
    const std::string_view message = rasterm_c_detail::globalLastError[0] != '\0'
        ? std::string_view(rasterm_c_detail::globalLastError)
        : std::string_view(engine->lastError);
    return rasterm_c_detail::copyError(message, buffer, capacity, required_size);
}

rasterm_result rasterm_presenter_create(const rasterm_presenter_options* requested,
                                        rasterm_presenter** output_presenter)
{
    if (output_presenter == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "output_presenter must not be null.");
    }
    *output_presenter = nullptr;
    rasterm_presenter_options defaults{};
    rasterm_presenter_options_init(&defaults);
    const rasterm_presenter_options& options = requested == nullptr ? defaults : *requested;
    if (options.struct_size < RASTERM_PRESENTER_OPTIONS_V1_SIZE ||
        options.engine.struct_size < RASTERM_ENGINE_OPTIONS_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "A Presenter option structure is smaller than its v1 prefix.");
    }
    if (options.engine.api_version != RASTERM_C_API_VERSION) {
        return rasterm_c_detail::failure(RASTERM_ERROR_API_VERSION_MISMATCH,
                                         "The requested rasterm C API version is unsupported.");
    }
    if (!rasterm_c_detail::validEngineOptions(options.engine) ||
        !std::isfinite(options.maximum_frames_per_second) ||
        options.maximum_frames_per_second < 0.0) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "rasterm_presenter_options contains an invalid value.");
    }

    try {
        std::unique_ptr<rasterm_presenter> handle(new rasterm_presenter);
        rasterm::OutputSink* sink = nullptr;
        if (options.engine.write != nullptr) {
            handle->sink = std::make_unique<rasterm_c_detail::CallbackSink>(
                options.engine.output_context, options.engine.write, options.engine.flush);
            sink = handle->sink.get();
        }
        const rasterm::Status initialized = handle->presenter.initialize({
            .engine = rasterm_c_detail::engineOptions(options.engine, sink),
            .maximumFramesPerSecond = options.maximum_frames_per_second,
        });
        handle->lastError = initialized.message;
        const rasterm_result code = rasterm_c_detail::result(initialized.code);
        *output_presenter = handle.release();
        if (code == RASTERM_SUCCESS) rasterm_c_detail::succeeded();
        else rasterm_c_detail::setGlobalError(initialized.message);
        return code;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate a Presenter handle.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_PRESENTER_THREAD_START_FAILED,
                                         "Unexpected exception while creating a Presenter.");
    }
}

void rasterm_presenter_destroy(rasterm_presenter* presenter) { delete presenter; }

rasterm_result rasterm_presenter_wait_until_idle(rasterm_presenter* presenter,
                                                 const uint32_t timeout_milliseconds)
{
    if (presenter == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter must not be null.");
    }
    try {
        if (!presenter->presenter.waitUntilIdle(
                std::chrono::milliseconds(timeout_milliseconds))) {
            const rasterm::Status status = presenter->presenter.status();
            presenter->lastError = status.message;
            return rasterm_c_detail::failure(rasterm_c_detail::result(status.code),
                                             status.message);
        }
        presenter->lastError.clear();
        rasterm_c_detail::succeeded();
        return RASTERM_SUCCESS;
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while draining Presenter.");
    }
}

rasterm_result rasterm_presenter_invalidate(rasterm_presenter* presenter)
{
    if (presenter == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter must not be null.");
    }
    try {
        if (!presenter->presenter.invalidate()) {
            const rasterm::Status status = presenter->presenter.status();
            presenter->lastError = status.message;
            return rasterm_c_detail::failure(rasterm_c_detail::result(status.code),
                                             status.message);
        }
        presenter->lastError.clear();
        rasterm_c_detail::succeeded();
        return RASTERM_SUCCESS;
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while invalidating Presenter.");
    }
}

rasterm_result rasterm_presenter_submit(rasterm_presenter* presenter,
                                        const rasterm_frame* frame)
{
    if (presenter == nullptr || frame == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter and frame must not be null.");
    }
    if (frame->struct_size < RASTERM_FRAME_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "rasterm_frame is smaller than the v1 prefix.");
    }
    if (frame->format < RASTERM_PIXEL_RGB24 || frame->format > RASTERM_PIXEL_RGBA4444 ||
        frame->stride <= 0 ||
        frame->stride > (std::numeric_limits<ptrdiff_t>::max)() ||
        frame->stride < (std::numeric_limits<ptrdiff_t>::min)()) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "Packed frame format or stride is invalid.");
    }
    try {
        rasterm::FrameMetadata metadata;
        if (!rasterm_c_detail::copyMetadata(presenter->damage, frame->metadata, metadata)) {
            return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                             "Packed frame metadata is invalid.");
        }
        const bool submitted = presenter->presenter.submit({
            frame->data, frame->width, frame->height, static_cast<ptrdiff_t>(frame->stride),
            rasterm_c_detail::pixelFormat(frame->format), metadata,
        });
        if (!submitted) {
            const rasterm::Status status = presenter->presenter.status();
            const rasterm_result code = status.code == rasterm::ErrorCode::None
                ? RASTERM_ERROR_INVALID_ARGUMENT : rasterm_c_detail::result(status.code);
            presenter->lastError = status.message.empty()
                ? "The Presenter rejected an invalid frame or is not running."
                : status.message;
            return rasterm_c_detail::failure(code, presenter->lastError);
        }
        presenter->lastError.clear();
        rasterm_c_detail::succeeded();
        return RASTERM_SUCCESS;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate Presenter frame resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while submitting a frame.");
    }
}

rasterm_result rasterm_presenter_submit_indexed(rasterm_presenter* presenter,
                                                const rasterm_indexed_frame* frame)
{
    if (presenter == nullptr || frame == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter and indexed frame must not be null.");
    }
    if (frame->struct_size < RASTERM_INDEXED_FRAME_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "rasterm_indexed_frame is smaller than the v1 prefix.");
    }
    if (frame->stride <= 0 ||
        frame->stride > (std::numeric_limits<ptrdiff_t>::max)() ||
        frame->stride < (std::numeric_limits<ptrdiff_t>::min)() ||
        frame->palette_size > 256 ||
        (frame->palette_size > 0 && frame->palette == nullptr)) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "Indexed frame stride or palette is invalid.");
    }
    try {
        rasterm::FrameMetadata metadata;
        if (!rasterm_c_detail::copyMetadata(presenter->damage, frame->metadata, metadata)) {
            return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                             "Indexed frame metadata is invalid.");
        }
        presenter->palette.clear();
        presenter->palette.reserve(frame->palette_size);
        for (size_t index = 0; index < frame->palette_size; ++index) {
            presenter->palette.push_back({ frame->palette[index].red, frame->palette[index].green,
                                           frame->palette[index].blue });
        }
        const bool submitted = presenter->presenter.submit({
            frame->indices, frame->width, frame->height, static_cast<ptrdiff_t>(frame->stride),
            { presenter->palette.data(), presenter->palette.size() }, metadata,
        });
        if (!submitted) {
            const rasterm::Status status = presenter->presenter.status();
            const rasterm_result code = status.code == rasterm::ErrorCode::None
                ? RASTERM_ERROR_INVALID_ARGUMENT : rasterm_c_detail::result(status.code);
            presenter->lastError = status.message.empty()
                ? "The Presenter rejected an invalid indexed frame or is not running."
                : status.message;
            return rasterm_c_detail::failure(code, presenter->lastError);
        }
        presenter->lastError.clear();
        rasterm_c_detail::succeeded();
        return RASTERM_SUCCESS;
    }
    catch (const std::bad_alloc&) {
        return rasterm_c_detail::failure(RASTERM_ERROR_OUT_OF_MEMORY,
                                         "rasterm could not allocate indexed Presenter resources.");
    }
    catch (...) {
        return rasterm_c_detail::failure(RASTERM_ERROR_RENDERING_EXCEPTION,
                                         "Unexpected exception while submitting an indexed frame.");
    }
}

rasterm_result rasterm_presenter_get_stats(const rasterm_presenter* presenter,
                                           rasterm_presenter_stats* stats)
{
    if (presenter == nullptr || stats == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter and stats must not be null.");
    }
    if (stats->struct_size < RASTERM_PRESENTER_STATS_V1_SIZE ||
        stats->latest_render.struct_size < RASTERM_RENDER_STATS_V1_SIZE) {
        return rasterm_c_detail::failure(RASTERM_ERROR_BUFFER_TOO_SMALL,
                                         "A Presenter stats structure is smaller than its v1 prefix.");
    }
    rasterm_c_detail::fillPresenterStats(presenter->presenter.stats(), *stats);
    rasterm_c_detail::succeeded();
    return RASTERM_SUCCESS;
}

rasterm_result rasterm_presenter_last_error(const rasterm_presenter* presenter, char* buffer,
                                            const size_t capacity, size_t* required_size)
{
    if (presenter == nullptr) {
        return rasterm_c_detail::failure(RASTERM_ERROR_INVALID_ARGUMENT,
                                         "presenter must not be null.");
    }
    std::string message = presenter->lastError;
    if (message.empty()) message = presenter->presenter.status().message;
    if (rasterm_c_detail::globalLastError[0] != '\0') {
        message = rasterm_c_detail::globalLastError;
    }
    return rasterm_c_detail::copyError(message, buffer, capacity, required_size);
}

}
