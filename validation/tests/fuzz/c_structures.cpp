/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
int32_t writeBytes(void*, const char*, size_t) { return 1; }
int32_t flushBytes(void*) { return 1; }
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size < 32) return 0;
    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    options.write = writeBytes;
    options.flush = flushBytes;
    options.quality = static_cast<std::int8_t>(data[0]);
    options.tone_map = static_cast<std::int8_t>(data[1]);
    options.realtime_dither = static_cast<std::int8_t>(data[2]);
    options.struct_size = data[3] == 0 ? RASTERM_ENGINE_OPTIONS_V1_SIZE - 1
                                      : RASTERM_ENGINE_OPTIONS_V1_SIZE;
    rasterm_engine* engine = nullptr;
    const rasterm_result created = rasterm_engine_create(&options, &engine);
    if (created == RASTERM_SUCCESS && engine != nullptr) {
        std::array<std::uint8_t, 512> pixels{};
        rasterm_frame frame;
        rasterm_frame_init(&frame);
        frame.data = pixels.data();
        frame.width = data[4] % 5;
        frame.height = data[5] % 5;
        frame.stride = static_cast<std::int8_t>(data[6]);
        frame.format = static_cast<std::int8_t>(data[7]);
        (void)rasterm_engine_render(engine, &frame, nullptr);
    }
    rasterm_engine_destroy(engine);
    return 0;
}
