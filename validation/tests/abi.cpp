/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

int32_t writeBytes(void*, const char*, size_t) { return 1; }
int32_t flushBytes(void*) { return 1; }

bool matchesLayout(const char* path)
{
    const std::unordered_map<std::string, std::size_t> actual{
        { "rasterm_color_metadata", sizeof(rasterm_color_metadata) },
        { "rasterm_damage_rect", sizeof(rasterm_damage_rect) },
        { "rasterm_frame_metadata", sizeof(rasterm_frame_metadata) },
        { "rasterm_engine_options", sizeof(rasterm_engine_options) },
        { "rasterm_frame", sizeof(rasterm_frame) },
        { "rasterm_presenter_options", sizeof(rasterm_presenter_options) },
        { "rasterm_rgb_color", sizeof(rasterm_rgb_color) },
        { "rasterm_indexed_frame", sizeof(rasterm_indexed_frame) },
        { "rasterm_render_stats", sizeof(rasterm_render_stats) },
        { "rasterm_terminal_capabilities", sizeof(rasterm_terminal_capabilities) },
        { "rasterm_presenter_stats", sizeof(rasterm_presenter_stats) },
    };
    std::ifstream input(path);
    std::string line;
    std::size_t checked = 0;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) return false;
        const auto found = actual.find(line.substr(0, separator));
        if (found == actual.end() || found->second != std::stoull(line.substr(separator + 1))) {
            return false;
        }
        ++checked;
    }
    return input.eof() && checked == actual.size();
}

}

int main(int argc, char** argv)
{
    if (argc != 2 || !matchesLayout(argv[1])) return 1;
    static_assert(RASTERM_SUCCESS == 0 && RASTERM_ERROR_OUT_OF_MEMORY == 19);
    static_assert(RASTERM_PIXEL_RGB24 == 0 && RASTERM_PIXEL_RGBA4444 == 6);
    static_assert(RASTERM_ENGINE_OPTIONS_V1_SIZE == sizeof(rasterm_engine_options));
    static_assert(RASTERM_FRAME_V1_SIZE == sizeof(rasterm_frame));
    static_assert(RASTERM_INDEXED_FRAME_V1_SIZE == sizeof(rasterm_indexed_frame));
    static_assert(RASTERM_RENDER_STATS_V1_SIZE == sizeof(rasterm_render_stats));
    static_assert(RASTERM_CAPABILITIES_V1_SIZE == sizeof(rasterm_terminal_capabilities));
    static_assert(RASTERM_PRESENTER_OPTIONS_V1_SIZE == sizeof(rasterm_presenter_options));
    static_assert(RASTERM_PRESENTER_STATS_V1_SIZE == sizeof(rasterm_presenter_stats));

    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    options.output_context = nullptr;
    options.write = writeBytes;
    options.flush = flushBytes;

    rasterm_engine* engine = nullptr;
    if (rasterm_engine_create(&options, &engine) != RASTERM_SUCCESS || engine == nullptr) return 2;

    rasterm_engine_options larger = options;
    larger.struct_size = sizeof(larger) + 64;
    rasterm_engine* compatible = nullptr;
    if (rasterm_engine_create(&larger, &compatible) != RASTERM_SUCCESS || compatible == nullptr) {
        rasterm_engine_destroy(engine);
        return 3;
    }
    rasterm_engine_destroy(compatible);

    options.struct_size = RASTERM_ENGINE_OPTIONS_V1_SIZE - 1;
    compatible = nullptr;
    if (rasterm_engine_create(&options, &compatible) != RASTERM_ERROR_BUFFER_TOO_SMALL ||
        compatible != nullptr) {
        rasterm_engine_destroy(engine);
        return 4;
    }

    std::array<std::uint8_t, 3> pixels{};
    rasterm_frame frame;
    rasterm_frame_init(&frame);
    frame.data = pixels.data();
    frame.width = 1;
    frame.height = 1;
    frame.stride = 3;
    frame.struct_size = RASTERM_FRAME_V1_SIZE - 1;
    if (rasterm_engine_render(engine, &frame, nullptr) != RASTERM_ERROR_BUFFER_TOO_SMALL) {
        rasterm_engine_destroy(engine);
        return 5;
    }

    rasterm_indexed_frame indexed;
    rasterm_indexed_frame_init(&indexed);
    indexed.struct_size = RASTERM_INDEXED_FRAME_V1_SIZE - 1;
    if (rasterm_engine_render_indexed(engine, &indexed, nullptr) !=
        RASTERM_ERROR_BUFFER_TOO_SMALL) {
        rasterm_engine_destroy(engine);
        return 6;
    }

    rasterm_render_stats stats;
    rasterm_render_stats_init(&stats);
    stats.struct_size = RASTERM_RENDER_STATS_V1_SIZE - 1;
    if (rasterm_engine_get_stats(engine, &stats) != RASTERM_ERROR_BUFFER_TOO_SMALL) {
        rasterm_engine_destroy(engine);
        return 7;
    }

    rasterm_terminal_capabilities capabilities;
    rasterm_terminal_capabilities_init(&capabilities);
    capabilities.struct_size = RASTERM_CAPABILITIES_V1_SIZE - 1;
    if (rasterm_engine_get_capabilities(engine, &capabilities) !=
        RASTERM_ERROR_BUFFER_TOO_SMALL) {
        rasterm_engine_destroy(engine);
        return 8;
    }

    rasterm_presenter_options presenterOptions;
    rasterm_presenter_options_init(&presenterOptions);
    presenterOptions.engine = options;
    presenterOptions.engine.struct_size = RASTERM_ENGINE_OPTIONS_V1_SIZE;
    presenterOptions.struct_size = RASTERM_PRESENTER_OPTIONS_V1_SIZE - 1;
    rasterm_presenter* presenter = nullptr;
    if (rasterm_presenter_create(&presenterOptions, &presenter) !=
        RASTERM_ERROR_BUFFER_TOO_SMALL || presenter != nullptr) {
        rasterm_engine_destroy(engine);
        return 9;
    }
    presenterOptions.struct_size = RASTERM_PRESENTER_OPTIONS_V1_SIZE;
    if (rasterm_presenter_create(&presenterOptions, &presenter) != RASTERM_SUCCESS ||
        presenter == nullptr) {
        rasterm_engine_destroy(engine);
        return 10;
    }
    rasterm_presenter_stats presenterStats;
    rasterm_presenter_stats_init(&presenterStats);
    presenterStats.struct_size = RASTERM_PRESENTER_STATS_V1_SIZE - 1;
    if (rasterm_presenter_get_stats(presenter, &presenterStats) !=
        RASTERM_ERROR_BUFFER_TOO_SMALL) {
        rasterm_presenter_destroy(presenter);
        rasterm_engine_destroy(engine);
        return 11;
    }
    rasterm_presenter_destroy(presenter);

    size_t required = 0;
    if (rasterm_last_error(nullptr, 0, &required) != RASTERM_ERROR_BUFFER_TOO_SMALL ||
        required <= 1) {
        rasterm_engine_destroy(engine);
        return 12;
    }
    rasterm_engine_destroy(engine);
    return 0;
}