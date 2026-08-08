/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

#include <stdint.h>
#include <string.h>

static size_t output_bytes;

static int32_t count_bytes(void* context, const char* bytes, size_t size)
{
    (void)context;
    (void)bytes;
    output_bytes += size;
    return 1;
}

static int32_t flush_output(void* context)
{
    (void)context;
    return 1;
}

int main(void)
{
    uint8_t pixels[2 * 2 * 3] = {
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255,
    };
    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    options.write = count_bytes;
    options.flush = flush_output;

    rasterm_engine* engine = NULL;
    if (rasterm_engine_create(&options, &engine) != RASTERM_SUCCESS) return 1;

    rasterm_frame frame;
    rasterm_frame_init(&frame);
    frame.data = pixels;
    frame.width = 2;
    frame.height = 2;
    frame.stride = 6;
    frame.format = RASTERM_PIXEL_RGB24;

    rasterm_render_stats stats;
    rasterm_render_stats_init(&stats);
    const rasterm_result result = rasterm_engine_render(engine, &frame, &stats);
    rasterm_engine_destroy(engine);
    return result == RASTERM_SUCCESS && output_bytes > 0 ? 0 : 2;
}