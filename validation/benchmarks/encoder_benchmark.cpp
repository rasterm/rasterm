/* SPDX-License-Identifier: Apache-2.0 */

#include <validation/benchmarks/Corpus.hpp>

#include <rasterm/rasterm.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

std::atomic<std::uint64_t> allocationCount{};

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        bytesWritten += bytes.size();
        return true;
    }
    bool flush() noexcept override { return true; }
    std::uint64_t bytesWritten = 0;
};

double percentile(std::vector<double> values, const double fraction)
{
    std::sort(values.begin(), values.end());
    const auto rank = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size())));
    const auto index = std::min(values.size() - 1, std::max<std::size_t>(1, rank) - 1);
    return values[index];
}

std::string environment(const char* name, const char* fallback)
{
    char value[512]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(std::size(value)));
    return length > 0 && length < std::size(value) ? std::string(value, length) : std::string(fallback);
}

}

void* operator new(const std::size_t size)
{
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    const int iterations = std::max(5, std::atoi(environment("RASTERM_BENCH_ITERATIONS", "30").c_str()));
    std::cout << "# corpus_version=" << rasterm::benchmark::corpusVersion
              << " machine=\"" << environment("RASTERM_BENCH_MACHINE", "unreported")
              << "\" terminal=\"" << environment("RASTERM_BENCH_TERMINAL_VERSION", "memory-sink")
              << "\" iterations=" << iterations << '\n';
    std::cout << "case,width,height,format,source_bytes,encode_p50_ms,encode_p95_ms,encode_p99_ms,"
                 "present_p50_ms,present_p95_ms,present_p99_ms,e2e_p50_ms,e2e_p95_ms,e2e_p99_ms,"
                 "payload_p50_bytes,payload_p95_bytes,payload_p99_bytes,alloc_p50,alloc_p95,alloc_p99\n";

    for (auto& item : rasterm::benchmark::makeCorpus()) {
        MemorySink sink;
        rasterm::EngineOptions options;
        options.output = &sink;
        options.preserveCursor = false;
        options.useSynchronizedOutput = false;
        options.enableDirtyRegions = item.uiDamage;
        rasterm::Engine engine;
        if (!engine.initialize(options)) return 1;

        std::vector<double> encode;
        std::vector<double> present;
        std::vector<double> endToEnd;
        std::vector<double> payload;
        std::vector<double> allocations;
        encode.reserve(iterations);
        present.reserve(iterations);
        endToEnd.reserve(iterations);
        payload.reserve(iterations);
        allocations.reserve(iterations);
        rasterm::DamageRect damage{ 0, 0, std::min(80, item.width), std::min(40, item.height) };

        for (int iteration = 0; iteration < iterations + 2; ++iteration) {
            rasterm::RenderStats stats;
            const auto allocationStart = allocationCount.load(std::memory_order_relaxed);
            const auto started = std::chrono::steady_clock::now();
            if (item.indexed) {
                rasterm::IndexedFrameView frame{
                    item.indices.data(), item.width, item.height, item.width,
                    { item.palette.data(), item.palette.size() },
                };
                stats = engine.renderFrame(frame);
            }
            else {
                rasterm::FrameMetadata metadata;
                if (item.uiDamage && iteration > 0) {
                    metadata.damage = { &damage, 1, true };
                    item.pixels[static_cast<std::size_t>(iteration) % item.pixels.size()] ^= 1;
                }
                rasterm::FrameView frame{
                    item.pixels.data(), item.width, item.height,
                    static_cast<std::ptrdiff_t>(item.width * rasterm::bytesPerPixel(item.format)),
                    item.format, metadata,
                };
                stats = engine.renderFrame(frame);
            }
            const double elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            const auto usedAllocations = allocationCount.load(std::memory_order_relaxed) - allocationStart;
            if (iteration >= 2) {
                encode.push_back(stats.encodeMilliseconds);
                present.push_back(stats.presentMilliseconds);
                endToEnd.push_back(elapsed);
                payload.push_back(static_cast<double>(stats.payloadBytes));
                allocations.push_back(static_cast<double>(usedAllocations));
            }
        }
        engine.shutdown();
        const std::size_t sourceBytes = item.indexed
            ? item.indices.size() + item.palette.size() * sizeof(rasterm::RgbColor)
            : item.pixels.size();
        std::cout << item.name << ',' << item.width << ',' << item.height << ','
                  << static_cast<int>(item.format) << ',' << sourceBytes << ','
                  << std::fixed << std::setprecision(3)
                  << percentile(encode, .50) << ',' << percentile(encode, .95) << ',' << percentile(encode, .99) << ','
                  << percentile(present, .50) << ',' << percentile(present, .95) << ',' << percentile(present, .99) << ','
                  << percentile(endToEnd, .50) << ',' << percentile(endToEnd, .95) << ',' << percentile(endToEnd, .99) << ','
                  << percentile(payload, .50) << ',' << percentile(payload, .95) << ',' << percentile(payload, .99) << ','
                  << percentile(allocations, .50) << ',' << percentile(allocations, .95) << ','
                  << percentile(allocations, .99) << '\n';
    }
    return 0;
}