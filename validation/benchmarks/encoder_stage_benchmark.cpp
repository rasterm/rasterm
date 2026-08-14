/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelEncoder.hpp>
#include <encoder/sixel/SixelSimd.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

struct Mode {
    std::string_view name;
    int value;
    bool supported;
};

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

double milliseconds(const std::chrono::nanoseconds duration) noexcept
{
    return static_cast<double>(duration.count()) / 1'000'000.0;
}

int benchmarkIterations()
{
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    _dupenv_s(&value, &length, "RASTERM_BENCH_ITERATIONS");
    const int result = value != nullptr ? std::atoi(value) : 15;
    std::free(value);
    return std::max(5, result);
#else
    const char* value = std::getenv("RASTERM_BENCH_ITERATIONS");
    return std::max(5, std::atoi(value != nullptr ? value : "15"));
#endif
}

}

int main()
{
    const int iterations = benchmarkIterations();
    constexpr std::array widths{ 32, 64, 128, 256, 512, 1024, 1920 };
    const std::array modes{
        Mode{ "scalar", 0, true },
        Mode{ "avx2", 1, rasterm::sixelAvx2Supported() },
        Mode{ "avx512", 2, rasterm::sixelAvx512Supported() },
    };
    std::array<std::array<double, widths.size()>, 3> mappingMedians{};
    std::cout << "mode,width,height,analysis_p50_ms,mapping_p50_ms,writing_p50_ms,"
                 "budgeting_p50_ms,total_p50_ms\n";
    for (std::size_t modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
        if (!modes[modeIndex].supported) continue;
        rasterm::setSixelSimdModeForTesting(modes[modeIndex].value);
        for (std::size_t sizeIndex = 0; sizeIndex < widths.size(); ++sizeIndex) {
            const int width = widths[sizeIndex];
            const int height = std::min(540, std::max(24, width / 2));
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
            std::uint32_t noise = 0x9e3779b9u;
            for (std::size_t index = 0; index < pixels.size(); ++index) {
                noise = noise * 1664525u + 1013904223u;
                pixels[index] = static_cast<std::uint8_t>((noise >> 24) ^ (index / 19));
            }
            const rasterm::FrameView frame{
                pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24
            };
            auto options = rasterm::SixelOptions::forVideo();
            options.maximumThreads = 1;
            rasterm::SixelEncoder encoder(options);
            for (int warmup = 0; warmup < 2; ++warmup) {
                encoder.prepareFrame(frame);
                (void)encoder.encodeFrame(frame);
            }
            std::vector<double> analysis;
            std::vector<double> mapping;
            std::vector<double> writing;
            std::vector<double> budgeting;
            std::vector<double> total;
            for (int iteration = 0; iteration < iterations; ++iteration) {
                const auto start = std::chrono::steady_clock::now();
                encoder.prepareFrame(frame);
                (void)encoder.encodeFrame(frame);
                const auto elapsed = std::chrono::steady_clock::now() - start;
                const auto stages = encoder.lastStageTimings();
                analysis.push_back(milliseconds(stages.analysis));
                mapping.push_back(milliseconds(stages.mapping));
                writing.push_back(milliseconds(stages.writing));
                budgeting.push_back(milliseconds(stages.budgeting));
                total.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
            }
            mappingMedians[modeIndex][sizeIndex] = median(mapping);
            std::cout << modes[modeIndex].name << ',' << width << ',' << height << ','
                      << median(analysis) << ',' << mappingMedians[modeIndex][sizeIndex] << ','
                      << median(writing) << ',' << median(budgeting) << ',' << median(total)
                      << '\n';
        }
    }
    for (std::size_t modeIndex = 1; modeIndex < modes.size(); ++modeIndex) {
        if (!modes[modeIndex].supported) continue;
        int crossover = 0;
        for (std::size_t index = 0; index < widths.size(); ++index) {
            if (widths[index] >= 64 &&
                mappingMedians[modeIndex][index] < mappingMedians[0][index] * 0.98) {
                crossover = widths[index];
                break;
            }
        }
        std::cout << "# " << modes[modeIndex].name << "_mapping_crossover_width="
                  << crossover << '\n';
    }
    rasterm::setSixelSimdModeForTesting(-1);
    return 0;
}
