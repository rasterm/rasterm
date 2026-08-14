/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelPalette.hpp>

#include <validation/benchmarks/Corpus.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

double milliseconds(const std::chrono::steady_clock::duration duration) noexcept
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

int benchmarkIterations()
{
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    _dupenv_s(&value, &length, "RASTERM_BENCH_ITERATIONS");
    const int result = value != nullptr ? std::atoi(value) : 10;
    std::free(value);
    return std::max(3, result);
#else
    const char* value = std::getenv("RASTERM_BENCH_ITERATIONS");
    return std::max(3, std::atoi(value != nullptr ? value : "10"));
#endif
}

}

int main()
{
    const int iterations = benchmarkIterations();
    rasterm::FixedPaletteMapper palette(256);
    constexpr int compactLevels = 16;
    std::array<std::uint8_t, compactLevels * compactLevels * compactLevels> compact{};
    for (int red = 0; red < compactLevels; ++red) {
        for (int green = 0; green < compactLevels; ++green) {
            for (int blue = 0; blue < compactLevels; ++blue) {
                const int sourceRed = std::min(31, red * 2 + 1);
                const int sourceGreen = std::min(31, green * 2 + 1);
                const int sourceBlue = std::min(31, blue * 2 + 1);
                compact[(red * compactLevels + green) * compactLevels + blue] =
                    palette.rgbLookup[(sourceRed * 32 + sourceGreen) * 32 + sourceBlue];
            }
        }
    }

    std::cout << "case,pixels,current_ms,compact_ms,index_mismatch_percent,"
                 "mean_palette_channel_delta,current_lookup_bytes,compact_lookup_bytes,checksum\n";
    for (const auto& item : rasterm::benchmark::makeCorpus()) {
        if (item.indexed || item.pixels.empty()) continue;
        const auto layout = rasterm::pixelLayout(item.format);
        const int stride = rasterm::pixelStride(layout);
        const std::size_t pixelCount = static_cast<std::size_t>(item.width) * item.height;
        std::vector<std::uint8_t> current(pixelCount);
        std::vector<std::uint8_t> candidate(pixelCount);
        const auto map = [&](const bool useCompact, std::vector<std::uint8_t>& destination) {
            std::size_t pixelIndex = 0;
            for (int y = 0; y < item.height; ++y) {
                const auto* row = item.pixels.data() +
                    static_cast<std::size_t>(y * item.width * stride);
                for (int x = 0; x < item.width; ++x, ++pixelIndex) {
                    const auto color = rasterm::readPixel(row + x * stride, layout);
                    destination[pixelIndex] = useCompact
                        ? compact[((color.red >> 4) * 16 + (color.green >> 4)) * 16 +
                                  (color.blue >> 4)]
                        : palette.rgbLookup[((color.red >> 3) * 32 + (color.green >> 3)) * 32 +
                                            (color.blue >> 3)];
                }
            }
        };
        map(false, current);
        map(true, candidate);
        std::size_t mismatches = 0;
        double channelDelta = 0.0;
        for (std::size_t index = 0; index < pixelCount; ++index) {
            if (current[index] != candidate[index]) ++mismatches;
            const auto source = palette.colorNumToRgb[current[index]];
            const auto compressed = palette.colorNumToRgb[candidate[index]];
            channelDelta += std::abs(static_cast<int>((source >> 16) & 0xff) -
                                     static_cast<int>((compressed >> 16) & 0xff));
            channelDelta += std::abs(static_cast<int>((source >> 8) & 0xff) -
                                     static_cast<int>((compressed >> 8) & 0xff));
            channelDelta += std::abs(static_cast<int>(source & 0xff) -
                                     static_cast<int>(compressed & 0xff));
        }
        const auto currentStart = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterations; ++iteration) map(false, current);
        const auto currentDuration = std::chrono::steady_clock::now() - currentStart;
        const auto compactStart = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterations; ++iteration) map(true, candidate);
        const auto compactDuration = std::chrono::steady_clock::now() - compactStart;
        std::uint64_t checksum = 0;
        for (const auto value : current) checksum += value;
        for (const auto value : candidate) checksum += value;
        std::cout << item.name << ',' << pixelCount << ','
                  << milliseconds(currentDuration) / iterations << ','
                  << milliseconds(compactDuration) / iterations << ','
                  << 100.0 * static_cast<double>(mismatches) / pixelCount << ','
                  << channelDelta / static_cast<double>(pixelCount * 3) << ','
                  << palette.rgbLookup.size() << ',' << compact.size() << ',' << checksum << '\n';
    }
    return 0;
}
