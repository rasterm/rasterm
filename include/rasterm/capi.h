/* SPDX-License-Identifier: Apache-2.0 */

#ifndef RASTERM_CAPI_H
#define RASTERM_CAPI_H

#include <stddef.h>
#include <stdint.h>

/* rasterm 1.0 ships as a static library. these macros are reserved so a
 * future shared library ABI can be introduced without changing declarations. */

#define RASTERM_STATIC_LIBRARY 1
#define RASTERM_C_API
#define RASTERM_CALL

#ifdef __cplusplus
extern "C" {
#endif

#define RASTERM_C_API_VERSION 1u
#define RASTERM_C_UNKNOWN_TIMESTAMP INT64_MIN

typedef struct rasterm_engine rasterm_engine;
typedef struct rasterm_presenter rasterm_presenter;

typedef enum rasterm_result {
    RASTERM_SUCCESS = 0,
    RASTERM_ERROR_INVALID_ARGUMENT = 1,
    RASTERM_ERROR_INVALID_OUTPUT_HANDLE = 2,
    RASTERM_ERROR_OUTPUT_IS_NOT_CONSOLE = 3,
    RASTERM_ERROR_CONSOLE_MODE_QUERY_FAILED = 4,
    RASTERM_ERROR_CONSOLE_MODE_ENABLE_FAILED = 5,
    RASTERM_ERROR_TERMINAL_DIMENSIONS_UNAVAILABLE = 6,
    RASTERM_ERROR_UNSUPPORTED_TERMINAL = 7,
    RASTERM_ERROR_STDOUT_MODE_FAILED = 8,
    RASTERM_ERROR_CONTROL_HANDLER_REGISTRATION_FAILED = 9,
    RASTERM_ERROR_DIAGNOSTIC_OPEN_FAILED = 10,
    RASTERM_ERROR_OUTPUT_WRITE_FAILED = 11,
    RASTERM_ERROR_OUTPUT_FLUSH_FAILED = 12,
    RASTERM_ERROR_OUTPUT_BUFFER_LIMIT_EXCEEDED = 13,
    RASTERM_ERROR_PRESENTER_THREAD_START_FAILED = 14,
    RASTERM_ERROR_INITIALIZATION_EXCEPTION = 15,
    RASTERM_ERROR_RENDERING_EXCEPTION = 16,
    RASTERM_ERROR_BUFFER_TOO_SMALL = 17,
    RASTERM_ERROR_API_VERSION_MISMATCH = 18,
    RASTERM_ERROR_OUT_OF_MEMORY = 19
} rasterm_result;

typedef enum rasterm_pixel_format {
    RASTERM_PIXEL_RGB24 = 0,
    RASTERM_PIXEL_BGR24 = 1,
    RASTERM_PIXEL_RGBA32 = 2,
    RASTERM_PIXEL_BGRA32 = 3,
    RASTERM_PIXEL_RGB565 = 4,
    RASTERM_PIXEL_0RGB1555 = 5,
    RASTERM_PIXEL_RGBA4444 = 6
} rasterm_pixel_format;

typedef enum rasterm_quality_profile {
    RASTERM_QUALITY_REALTIME = 0,
    RASTERM_QUALITY_ADAPTIVE_VIDEO = 1,
    RASTERM_QUALITY_HIGH = 2
} rasterm_quality_profile;

typedef enum rasterm_capability_support {
    RASTERM_CAPABILITY_UNKNOWN = 0,
    RASTERM_CAPABILITY_UNSUPPORTED = 1,
    RASTERM_CAPABILITY_SUPPORTED = 2
} rasterm_capability_support;

typedef enum rasterm_color_primaries {
    RASTERM_PRIMARIES_UNSPECIFIED = 0,
    RASTERM_PRIMARIES_BT709 = 1,
    RASTERM_PRIMARIES_BT2020 = 2,
    RASTERM_PRIMARIES_DISPLAY_P3 = 3
} rasterm_color_primaries;

typedef enum rasterm_transfer_function {
    RASTERM_TRANSFER_UNSPECIFIED = 0,
    RASTERM_TRANSFER_SRGB = 1,
    RASTERM_TRANSFER_LINEAR = 2,
    RASTERM_TRANSFER_BT709 = 3,
    RASTERM_TRANSFER_GAMMA22 = 4,
    RASTERM_TRANSFER_PQ = 5,
    RASTERM_TRANSFER_HLG = 6
} rasterm_transfer_function;

typedef enum rasterm_matrix_coefficients {
    RASTERM_MATRIX_UNSPECIFIED = 0,
    RASTERM_MATRIX_IDENTITY = 1,
    RASTERM_MATRIX_BT601 = 2,
    RASTERM_MATRIX_BT709 = 3,
    RASTERM_MATRIX_BT2020_NCL = 4
} rasterm_matrix_coefficients;

typedef enum rasterm_color_range {
    RASTERM_RANGE_UNSPECIFIED = 0,
    RASTERM_RANGE_LIMITED = 1,
    RASTERM_RANGE_FULL = 2
} rasterm_color_range;

typedef enum rasterm_tone_map_operator {
    RASTERM_TONE_MAP_NONE = 0,
    RASTERM_TONE_MAP_REINHARD = 1,
    RASTERM_TONE_MAP_HABLE = 2,
    RASTERM_TONE_MAP_ACES = 3
} rasterm_tone_map_operator;

typedef enum rasterm_dither_mode {
    RASTERM_DITHER_NONE = 0,
    RASTERM_DITHER_ORDERED_BAYER_4X4 = 1,
    RASTERM_DITHER_FLOYD_STEINBERG = 2
} rasterm_dither_mode;

/* callbacks are invoked synchronously by Engine or on Presenter's worker.
 * the byte span is borrowed for the duration of the callback only. a nonzero
 * return accepts every byte so zero means no byte was accepted and reports a
 * stable output failure. do not destroy or shut down the active handle here. */

typedef int32_t (RASTERM_CALL *rasterm_write_callback)(void* context, const char* bytes, size_t size);
typedef int32_t (RASTERM_CALL *rasterm_flush_callback)(void* context);

typedef struct rasterm_color_metadata {
    int32_t primaries;
    int32_t transfer;
    int32_t matrix;
    int32_t range;
    float reference_white_nits;
    float mastering_peak_nits;
    uint64_t reserved[2];
} rasterm_color_metadata;

typedef struct rasterm_damage_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} rasterm_damage_rect;

typedef struct rasterm_frame_metadata {
    uint64_t frame_id;
    int64_t timestamp_nanoseconds;
    rasterm_color_metadata color;
    rasterm_color_metadata source_color;
    const rasterm_damage_rect* damage_rects;
    size_t damage_count;
    int32_t damage_supplied;
    uint64_t reserved[4];
} rasterm_frame_metadata;

typedef struct rasterm_engine_options {
    uint32_t struct_size;
    uint32_t api_version;
    int32_t quality;
    int32_t use_alternate_screen;
    int32_t preserve_cursor;
    int32_t enable_dirty_regions;
    int32_t require_sixel_support;
    int32_t use_synchronized_output;
    uint64_t maximum_output_bytes;
    double backpressure_threshold_milliseconds;
    int32_t convert_to_srgb;
    int32_t tone_map;
    float output_peak_nits;
    int32_t realtime_dither;
    int32_t adaptive_palette_lock_frames;
    float scene_cut_threshold;
    void* output_context;
    rasterm_write_callback write;
    rasterm_flush_callback flush;
    uint64_t reserved[8];
} rasterm_engine_options;

typedef struct rasterm_frame {
    uint32_t struct_size;
    const uint8_t* data;
    int32_t width;
    int32_t height;
    int64_t stride;
    int32_t format;
    rasterm_frame_metadata metadata;
    uint64_t reserved[4];
} rasterm_frame;

typedef struct rasterm_presenter_options {
    uint32_t struct_size;
    rasterm_engine_options engine;
    double maximum_frames_per_second;
    uint64_t reserved[8];
} rasterm_presenter_options;

typedef struct rasterm_rgb_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rasterm_rgb_color;

typedef struct rasterm_indexed_frame {
    uint32_t struct_size;
    const uint8_t* indices;
    int32_t width;
    int32_t height;
    int64_t stride;
    const rasterm_rgb_color* palette;
    size_t palette_size;
    rasterm_frame_metadata metadata;
    uint64_t reserved[4];
} rasterm_indexed_frame;

typedef struct rasterm_render_stats {
    uint32_t struct_size;
    int32_t error;
    int32_t rendered;
    int32_t full_frame;
    uint64_t payload_bytes;
    uint32_t dirty_regions;
    int32_t width;
    int32_t height;
    int32_t colors_used;
    double encode_milliseconds;
    double present_milliseconds;
    double frames_per_second;
    double payload_bytes_per_second;
    double terminal_write_p95_milliseconds;
    double terminal_write_p99_milliseconds;
    uint64_t output_failures;
    uint64_t backpressure_events;
    uint64_t payload_limit_drops;
    uint64_t reserved[8];
} rasterm_render_stats;

typedef struct rasterm_terminal_capabilities {
    uint32_t struct_size;
    int32_t custom_output;
    int32_t valid_output_handle;
    int32_t console_output;
    int32_t virtual_terminal_output;
    int32_t sixel;
    int32_t synchronized_output;
    int32_t columns;
    int32_t rows;
    int32_t pixel_width;
    int32_t pixel_height;
    int32_t cell_pixel_width;
    int32_t cell_pixel_height;
    uint64_t reserved[8];
} rasterm_terminal_capabilities;

typedef struct rasterm_presenter_stats {
    uint32_t struct_size;
    uint64_t submitted_frames;
    uint64_t presented_frames;
    uint64_t replaced_frames;
    rasterm_render_stats latest_render;
    uint64_t reserved[8];
} rasterm_presenter_stats;

#define RASTERM_ENGINE_OPTIONS_V1_SIZE ((uint32_t)(offsetof(rasterm_engine_options, reserved) + sizeof(((rasterm_engine_options*)0)->reserved)))
#define RASTERM_FRAME_V1_SIZE ((uint32_t)(offsetof(rasterm_frame, reserved) + sizeof(((rasterm_frame*)0)->reserved)))
#define RASTERM_INDEXED_FRAME_V1_SIZE ((uint32_t)(offsetof(rasterm_indexed_frame, reserved) + sizeof(((rasterm_indexed_frame*)0)->reserved)))
#define RASTERM_RENDER_STATS_V1_SIZE ((uint32_t)(offsetof(rasterm_render_stats, reserved) + sizeof(((rasterm_render_stats*)0)->reserved)))
#define RASTERM_CAPABILITIES_V1_SIZE ((uint32_t)(offsetof(rasterm_terminal_capabilities, reserved) + sizeof(((rasterm_terminal_capabilities*)0)->reserved)))
#define RASTERM_PRESENTER_OPTIONS_V1_SIZE ((uint32_t)(offsetof(rasterm_presenter_options, reserved) + sizeof(((rasterm_presenter_options*)0)->reserved)))
#define RASTERM_PRESENTER_STATS_V1_SIZE ((uint32_t)(offsetof(rasterm_presenter_stats, reserved) + sizeof(((rasterm_presenter_stats*)0)->reserved)))

#if defined(_MSC_VER) && defined(_M_X64)
#  ifdef __cplusplus
#    define RASTERM_C_ABI_ASSERT(expression, message) static_assert((expression), message)
#  else
#    define RASTERM_C_ABI_JOIN_INNER(a, b) a##b
#    define RASTERM_C_ABI_JOIN(a, b) RASTERM_C_ABI_JOIN_INNER(a, b)
#    define RASTERM_C_ABI_ASSERT(expression, message) \
        typedef char RASTERM_C_ABI_JOIN(rasterm_c_abi_assert_, __LINE__)[(expression) ? 1 : -1]
#  endif
RASTERM_C_ABI_ASSERT(sizeof(rasterm_color_metadata) == 40, "rasterm_color_metadata ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_damage_rect) == 16, "rasterm_damage_rect ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_frame_metadata) == 152, "rasterm_frame_metadata ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_engine_options) == 160, "rasterm_engine_options ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_frame) == 224, "rasterm_frame ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_presenter_options) == 240, "rasterm_presenter_options ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_rgb_color) == 3, "rasterm_rgb_color ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_indexed_frame) == 232, "rasterm_indexed_frame ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_render_stats) == 176, "rasterm_render_stats ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_terminal_capabilities) == 120, "rasterm_terminal_capabilities ABI changed");
RASTERM_C_ABI_ASSERT(sizeof(rasterm_presenter_stats) == 272, "rasterm_presenter_stats ABI changed");
RASTERM_C_ABI_ASSERT(offsetof(rasterm_engine_options, reserved) == 96, "rasterm_engine_options prefix changed");
RASTERM_C_ABI_ASSERT(offsetof(rasterm_frame, metadata) == 40, "rasterm_frame prefix changed");
RASTERM_C_ABI_ASSERT(offsetof(rasterm_indexed_frame, metadata) == 48, "rasterm_indexed_frame prefix changed");
RASTERM_C_ABI_ASSERT(offsetof(rasterm_presenter_stats, latest_render) == 32, "rasterm_presenter_stats prefix changed");
#  undef RASTERM_C_ABI_ASSERT
#  ifndef __cplusplus
#    undef RASTERM_C_ABI_JOIN
#    undef RASTERM_C_ABI_JOIN_INNER
#  endif
#endif

RASTERM_C_API void rasterm_engine_options_init(rasterm_engine_options* options);
RASTERM_C_API void rasterm_frame_init(rasterm_frame* frame);
RASTERM_C_API void rasterm_indexed_frame_init(rasterm_indexed_frame* frame);
RASTERM_C_API void rasterm_render_stats_init(rasterm_render_stats* stats);
RASTERM_C_API void rasterm_terminal_capabilities_init(rasterm_terminal_capabilities* capabilities);
RASTERM_C_API void rasterm_presenter_options_init(rasterm_presenter_options* options);
RASTERM_C_API void rasterm_presenter_stats_init(rasterm_presenter_stats* stats);

RASTERM_C_API uint32_t rasterm_c_api_version(void);
RASTERM_C_API void rasterm_version(uint32_t* major, uint32_t* minor, uint32_t* patch);
RASTERM_C_API rasterm_result rasterm_last_error(char* buffer, size_t capacity,
                                                size_t* required_size);
RASTERM_C_API rasterm_result rasterm_engine_create(const rasterm_engine_options* options,
                                                   rasterm_engine** output_engine);
RASTERM_C_API void rasterm_engine_destroy(rasterm_engine* engine);
RASTERM_C_API rasterm_result rasterm_engine_render(rasterm_engine* engine,
                                                   const rasterm_frame* frame,
                                                   rasterm_render_stats* stats);
RASTERM_C_API rasterm_result rasterm_engine_render_indexed(rasterm_engine* engine,
                                                           const rasterm_indexed_frame* frame,
                                                           rasterm_render_stats* stats);
RASTERM_C_API rasterm_result rasterm_engine_clear(rasterm_engine* engine);
RASTERM_C_API rasterm_result rasterm_engine_reset(rasterm_engine* engine);
RASTERM_C_API rasterm_result rasterm_engine_get_stats(const rasterm_engine* engine,
                                                      rasterm_render_stats* stats);
RASTERM_C_API rasterm_result rasterm_engine_get_capabilities(
    const rasterm_engine* engine, rasterm_terminal_capabilities* capabilities);
RASTERM_C_API rasterm_result rasterm_engine_last_error(const rasterm_engine* engine,
                                                       char* buffer, size_t capacity,
                                                       size_t* required_size);
RASTERM_C_API rasterm_result rasterm_presenter_create(const rasterm_presenter_options* options,
                                                      rasterm_presenter** output_presenter);
RASTERM_C_API void rasterm_presenter_destroy(rasterm_presenter* presenter);
RASTERM_C_API rasterm_result rasterm_presenter_submit(rasterm_presenter* presenter,
                                                      const rasterm_frame* frame);
RASTERM_C_API rasterm_result rasterm_presenter_submit_indexed(
    rasterm_presenter* presenter, const rasterm_indexed_frame* frame);
RASTERM_C_API rasterm_result rasterm_presenter_get_stats(
    const rasterm_presenter* presenter, rasterm_presenter_stats* stats);
RASTERM_C_API rasterm_result rasterm_presenter_last_error(
    const rasterm_presenter* presenter, char* buffer, size_t capacity, size_t* required_size);

#ifdef __cplusplus
}
#endif

#endif