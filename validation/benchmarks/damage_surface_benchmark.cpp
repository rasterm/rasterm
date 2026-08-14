/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelEncoder.hpp>
#include <render/DamagePlanner.hpp>
#include <render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

struct Surface {
    std::string_view name;
    int width;
    int height;
};

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

int benchmarkIterations()
{
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    _dupenv_s(&value, &length, "RASTERM_BENCH_ITERATIONS");
    const int result = value != nullptr ? std::atoi(value) : 7;
    std::free(value);
    return std::max(3, result);
#else
    const char* value = std::getenv("RASTERM_BENCH_ITERATIONS");
    return std::max(3, std::atoi(value != nullptr ? value : "7"));
#endif
}

}

int main()
{
    const int iterations = benchmarkIterations();
    constexpr std::array surfaces{
        Surface{ "small", 320, 180 }, Surface{ "medium", 800, 450 },
        Surface{ "fullscreen", 1920, 1080 }, Surface{ "high-dpi", 2800, 1400 },
    };
    std::cout << "surface,width,height,mode,region_pixels,encode_p50_ms,payload_p50_bytes,"
                 "analysis_p50_ms,mapping_p50_ms,writing_p50_ms,budgeting_p50_ms,"
                 "estimated_present_bytes\n";
    for (const auto surface : surfaces) {
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(surface.width) * surface.height * 3);
        for (int y = 0; y < surface.height; ++y) {
            for (int x = 0; x < surface.width; ++x) {
                const std::size_t offset = static_cast<std::size_t>(y * surface.width + x) * 3;
                const bool panel = x > surface.width / 4 && x < surface.width * 3 / 4 &&
                    y > surface.height / 4 && y < surface.height * 3 / 4;
                pixels[offset] = static_cast<std::uint8_t>(panel ? 44 : 18);
                pixels[offset + 1] = static_cast<std::uint8_t>(panel ? 92 : 20);
                pixels[offset + 2] = static_cast<std::uint8_t>(panel ? 156 : 24);
            }
        }
        const rasterm::FrameView frame{
            pixels.data(), surface.width, surface.height, surface.width * 3,
            rasterm::PixelFormat::RGB24
        };
        const rasterm::CellPixelSize cell{ 10, 20 };
        const auto damage = rasterm::alignDamageToCells({
            surface.width * 9 / 20, surface.height * 9 / 20,
            std::max(1, surface.width / 10), std::max(1, surface.height / 10),
        }, surface.width, surface.height, cell);
        for (const bool fullFrame : { true, false }) {
            auto options = rasterm::SixelOptions::forVideo();
            options.persistPaletteRegisters = false;
            rasterm::SixelEncoder encoder(options);
            std::vector<double> elapsed;
            std::vector<double> payload;
            std::vector<double> analysis;
            std::vector<double> mapping;
            std::vector<double> writing;
            std::vector<double> budgeting;
            for (int iteration = 0; iteration < iterations; ++iteration) {
                const auto start = std::chrono::steady_clock::now();
                if (fullFrame) {
                    encoder.prepareFrame(frame);
                }
                else {
                    encoder.prepareRegion(frame, damage);
                }
                const auto output = fullFrame ? encoder.encodeFrame(frame)
                                              : encoder.encodeRegion(frame, damage);
                const auto end = std::chrono::steady_clock::now();
                const auto stages = encoder.lastStageTimings();
                elapsed.push_back(std::chrono::duration<double, std::milli>(end - start).count());
                payload.push_back(static_cast<double>(output.size()));
                analysis.push_back(stages.analysis.count() / 1'000'000.0);
                mapping.push_back(stages.mapping.count() / 1'000'000.0);
                writing.push_back(stages.writing.count() / 1'000'000.0);
                budgeting.push_back(stages.budgeting.count() / 1'000'000.0);
            }
            std::vector<rasterm::DamageRegion> modeledRegions{ damage };
            const auto model = rasterm::planDamagePresentation(
                modeledRegions, surface.width, surface.height, cell, true, 204);
            std::cout << surface.name << ',' << surface.width << ',' << surface.height << ','
                      << (fullFrame ? "full" : "region") << ','
                      << (fullFrame ? static_cast<std::uint64_t>(surface.width) * surface.height
                                    : static_cast<std::uint64_t>(damage.width) * damage.height)
                      << ',' << median(elapsed) << ',' << median(payload) << ','
                      << median(analysis) << ',' << median(mapping) << ',' << median(writing)
                      << ',' << median(budgeting) << ','
                      << (fullFrame ? model.fullFrame.total() : model.regional.total()) << '\n';
        }
    }
    return 0;
}
