/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

#include <stddef.h>
#include <stdint.h>

typedef struct output_counter { size_t bytes; } output_counter;

static int32_t write_output(void* context, const char* bytes, size_t size)
{
    output_counter* output = (output_counter*)context;
    if (bytes == NULL && size != 0) return 0;
    output->bytes += size;
    return 1;
}

static int32_t flush_output(void* context)
{
    (void)context;
    return 1;
}

int main(void)
{
    output_counter output = { 0 };
    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    options.output_context = &output;
    options.write = write_output;
    options.flush = flush_output;

    rasterm_engine* engine = NULL;
    if (rasterm_engine_create(&options, &engine) != RASTERM_SUCCESS) return 1;
    {
        const uint8_t red[3] = { 255, 0, 0 };
        rasterm_frame frame;
        rasterm_frame_init(&frame);
        frame.data = red;
        frame.width = 1;
        frame.height = 1;
        frame.stride = 3;
        if (rasterm_engine_render(engine, &frame, NULL) != RASTERM_SUCCESS) return 2;
    }
    rasterm_engine_destroy(engine);
    return output.bytes == 0 ? 3 : 0;
}