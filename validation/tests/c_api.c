/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

#include <stdint.h>
#include <string.h>

typedef struct memory_sink {
    size_t bytes;
    size_t flushes;
} memory_sink;

static int32_t write_bytes(void* context, const char* bytes, size_t size)
{
    memory_sink* sink = (memory_sink*)context;
    if (bytes == NULL && size != 0) return 0;
    sink->bytes += size;
    return 1;
}

static int32_t flush_bytes(void* context)
{
    memory_sink* sink = (memory_sink*)context;
    ++sink->flushes;
    return 1;
}

int main(void)
{
    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    memory_sink sink = { 0, 0 };
    options.output_context = &sink;
    options.write = write_bytes;
    options.flush = flush_bytes;

    rasterm_engine* engine = NULL;
    if (rasterm_engine_create(&options, &engine) != RASTERM_SUCCESS || engine == NULL) return 1;
    if (rasterm_c_api_version() != 1u) return 2;

    uint8_t pixels[6] = { 255, 0, 0, 0, 255, 0 };
    rasterm_frame frame;
    rasterm_frame_init(&frame);
    frame.data = pixels;
    frame.width = 2;
    frame.height = 1;
    frame.stride = 6;
    frame.metadata.frame_id = 7;

    rasterm_render_stats stats;
    rasterm_render_stats_init(&stats);
    if (rasterm_engine_render(engine, &frame, &stats) != RASTERM_SUCCESS ||
        !stats.rendered || stats.width != 2 || sink.bytes == 0) return 3;

    rasterm_terminal_capabilities capabilities;
    rasterm_terminal_capabilities_init(&capabilities);
    if (rasterm_engine_get_capabilities(engine, &capabilities) != RASTERM_SUCCESS ||
        !capabilities.custom_output) return 4;

    rasterm_rgb_color palette[2] = { {255, 0, 0}, {0, 0, 255} };
    uint8_t indices[2] = { 0, 1 };
    rasterm_indexed_frame indexed;
    rasterm_indexed_frame_init(&indexed);
    indexed.indices = indices;
    indexed.width = 2;
    indexed.height = 1;
    indexed.stride = 2;
    indexed.palette = palette;
    indexed.palette_size = 2;
    if (rasterm_engine_render_indexed(engine, &indexed, &stats) != RASTERM_SUCCESS) return 5;

    indices[1] = 2;
    if (rasterm_engine_render_indexed(engine, &indexed, &stats) !=
        RASTERM_ERROR_INVALID_ARGUMENT) return 6;
    indices[1] = 1;
    indexed.palette_size = 257;
    if (rasterm_engine_render_indexed(engine, &indexed, &stats) !=
        RASTERM_ERROR_INVALID_ARGUMENT) return 7;
    indexed.palette_size = 2;
    indexed.stride = -2;
    if (rasterm_engine_render_indexed(engine, &indexed, &stats) !=
        RASTERM_ERROR_INVALID_ARGUMENT) return 8;
    indexed.stride = 2;

    frame.data = NULL;
    if (rasterm_engine_render(engine, &frame, &stats) != RASTERM_ERROR_INVALID_ARGUMENT) return 9;
    {
        size_t required = 0;
        char error[128];
        if (rasterm_engine_last_error(engine, NULL, 0, &required) !=
                RASTERM_ERROR_BUFFER_TOO_SMALL || required <= 1 ||
            rasterm_engine_last_error(engine, error, sizeof(error), NULL) != RASTERM_SUCCESS ||
            strlen(error) == 0 ||
            rasterm_last_error(error, sizeof(error), NULL) != RASTERM_SUCCESS) return 10;
    }

    rasterm_engine_destroy(engine);
    options.api_version = RASTERM_C_API_VERSION + 1;
    engine = NULL;
    if (rasterm_engine_create(&options, &engine) != RASTERM_ERROR_API_VERSION_MISMATCH ||
        engine != NULL) return 11;

    {
        rasterm_presenter_options presenter_options;
        rasterm_presenter* presenter = NULL;
        rasterm_presenter_stats presenter_stats;
        rasterm_presenter_options_init(&presenter_options);
        presenter_options.engine.output_context = &sink;
        presenter_options.engine.write = write_bytes;
        presenter_options.engine.flush = flush_bytes;
        if (rasterm_presenter_create(&presenter_options, &presenter) != RASTERM_SUCCESS ||
            presenter == NULL) return 12;
        rasterm_frame_init(&frame);
        frame.data = pixels;
        frame.width = 2;
        frame.height = 1;
        frame.stride = 6;
        if (rasterm_presenter_submit(presenter, &frame) != RASTERM_SUCCESS) return 13;
        rasterm_presenter_stats_init(&presenter_stats);
        if (rasterm_presenter_get_stats(presenter, &presenter_stats) != RASTERM_SUCCESS ||
            presenter_stats.submitted_frames != 1) return 14;
        rasterm_presenter_destroy(presenter);
    }
    return 0;
}