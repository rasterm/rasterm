/* SPDX-License-Identifier: Apache-2.0 */

#include <validation/benchmarks/Corpus.hpp>

#include <rasterm/rasterm.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <new>
#include <memory>
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
        ++writeCalls;
        return true;
    }
    bool flush() noexcept override { return true; }
    std::uint64_t bytesWritten = 0;
    std::uint64_t writeCalls = 0;
};

struct IndexedOwner {
    std::vector<std::uint8_t> indices;
    std::vector<rasterm::RgbColor> palette;
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
    if (void* memory = HeapAlloc(GetProcessHeap(), 0, size == 0 ? 1 : size)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept
{
    if (memory != nullptr) HeapFree(GetProcessHeap(), 0, memory);
}

void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

int main()
{
    const int iterations = std::max(5, std::atoi(environment("RASTERM_BENCH_ITERATIONS", "30").c_str()));
    std::cout << "# corpus_version=" << rasterm::benchmark::corpusVersion
              << " machine=\"" << environment("RASTERM_BENCH_MACHINE", "unreported")
              << "\" terminal=\"" << environment("RASTERM_BENCH_TERMINAL_VERSION", "memory-sink")
              << "\" iterations=" << iterations << '\n';
    std::cout << "case,width,height,format,source_bytes,encode_p50_ms,encode_p95_ms,encode_p99_ms,"
                 "present_p50_ms,present_p95_ms,present_p99_ms,e2e_p50_ms,e2e_p95_ms,e2e_p99_ms,"
                 "validation_p50_ms,conversion_p50_ms,submit_p50_ms,wire_p50_bytes,"
                 "scratch_p50_bytes,output_capacity_p50_bytes,"
                 "payload_p50_bytes,payload_p95_bytes,payload_p99_bytes,sink_writes_p50,"
                 "alloc_p50,alloc_p95,alloc_p99\n";

    for (auto& item : rasterm::benchmark::makeCorpus()) {
        MemorySink sink;
        rasterm::EngineOptions options;
        options.output = &sink;
        options.preserveCursor = false;
        options.useSynchronizedOutput = false;
        options.enableDirtyRegions = item.uiDamage || item.lowEntropyMotion;
        options.maximumOutputBytes = item.outputLimit;
        options.encoder.persistPaletteRegisters = item.persistPaletteRegisters;
        options.encoder.outputChunkBytes = item.outputChunkBytes;
        options.encoder.maximumThreads = item.maximumThreads;
        options.encoder.independentRegionQuantization = item.independentRegionQuantization;
        if (item.multiRegion) options.quality = rasterm::QualityProfile::AdaptiveVideo;
        rasterm::Engine engine;
        rasterm::Presenter presenter;
        const bool usesPresenter = item.presenterSubmission !=
            rasterm::benchmark::PresenterSubmission::None;
        if (usesPresenter) {
            if (!presenter.initialize({ .engine = options })) return 1;
        }
        else if (!engine.initialize(options)) return 1;
        std::shared_ptr<IndexedOwner> sharedOwner;
        if (item.presenterSubmission == rasterm::benchmark::PresenterSubmission::Shared) {
            sharedOwner = std::make_shared<IndexedOwner>(IndexedOwner{ item.indices, item.palette });
        }

        std::vector<double> encode;
        std::vector<double> present;
        std::vector<double> endToEnd;
        std::vector<double> payload;
        std::vector<double> allocations;
        std::vector<double> validation;
        std::vector<double> conversion;
        std::vector<double> submit;
        std::vector<double> wire;
        std::vector<double> scratch;
        std::vector<double> outputCapacity;
        std::vector<double> sinkWrites;
        encode.reserve(iterations);
        present.reserve(iterations);
        endToEnd.reserve(iterations);
        payload.reserve(iterations);
        allocations.reserve(iterations);
        validation.reserve(iterations);
        conversion.reserve(iterations);
        submit.reserve(iterations);
        wire.reserve(iterations);
        scratch.reserve(iterations);
        outputCapacity.reserve(iterations);
        sinkWrites.reserve(iterations);
        rasterm::DamageRect damage{ 0, 0, std::min(80, item.width), std::min(40, item.height) };
        std::array<rasterm::DamageRect, 4> multipleDamage{{
            { 0, 0, 40, 20 }, { item.width - 40, 0, 40, 20 },
            { 0, item.height - 20, 40, 20 },
            { item.width - 40, item.height - 20, 40, 20 },
        }};

        for (int iteration = 0; iteration < iterations + 2; ++iteration) {
            if (item.lowEntropyMotion) {
                constexpr int objectWidth = 200;
                constexpr int objectHeight = 200;
                constexpr int objectY = 260;
                const int previousX = 80 + (std::max(0, iteration - 1) * 24) % 800;
                const int currentX = 80 + (iteration * 24) % 800;
                const auto paint = [&](const int originX, const std::uint8_t blue,
                                       const std::uint8_t green, const std::uint8_t red) {
                    for (int y = objectY; y < objectY + objectHeight; ++y) {
                        for (int x = originX; x < originX + objectWidth; ++x) {
                            const std::size_t offset =
                                static_cast<std::size_t>(y * item.width + x) * 4;
                            item.pixels[offset] = blue;
                            item.pixels[offset + 1] = green;
                            item.pixels[offset + 2] = red;
                            item.pixels[offset + 3] = 0xff;
                        }
                    }
                };
                if (iteration > 0) paint(previousX, 0x11, 0x11, 0x11);
                paint(currentX, 0xfe, 0xa8, 0x6e);
                const int damageX = std::min(previousX, currentX);
                damage = { damageX, objectY,
                           std::max(previousX, currentX) + objectWidth - damageX,
                           objectHeight };
            }

            rasterm::RenderStats stats;
            const auto allocationStart = allocationCount.load(std::memory_order_relaxed);
            const auto writeStart = sink.writeCalls;
            const auto started = std::chrono::steady_clock::now();
            double submitElapsed = 0.0;
            if (usesPresenter) {
                const auto submitStarted = std::chrono::steady_clock::now();
                bool submitted = false;
                if (item.presenterSubmission == rasterm::benchmark::PresenterSubmission::Copy) {
                    submitted = presenter.submit({
                        item.indices.data(), item.width, item.height, item.width,
                        { item.palette.data(), item.palette.size() },
                    });
                }
                else if (item.presenterSubmission == rasterm::benchmark::PresenterSubmission::Owned) {
                    submitted = presenter.submit(rasterm::OwnedIndexedFrame(
                        item.indices, item.palette, item.width, item.height, item.width));
                }
                else {
                    submitted = presenter.submitShared({
                        { sharedOwner->indices.data(), item.width, item.height, item.width,
                          { sharedOwner->palette.data(), sharedOwner->palette.size() } },
                        sharedOwner,
                    });
                }
                submitElapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - submitStarted).count();
                if (!submitted || !presenter.waitUntilIdle(std::chrono::seconds(5))) return 2;
                stats = presenter.stats().latestRender;
            }
            else if (item.indexed) {
                rasterm::IndexedFrameView frame{
                    item.indices.data(), item.width, item.height, item.width,
                    { item.palette.data(), item.palette.size() },
                };
                stats = engine.renderFrame(frame);
            }
            else {
                rasterm::FrameMetadata metadata;
                if (item.nonSrgbDamage) {
                    metadata.color.transfer = rasterm::TransferFunction::Linear;
                }
                if ((item.uiDamage || item.lowEntropyMotion) && iteration > 0) {
                    metadata.damage = item.multiRegion
                        ? rasterm::DamageView{ multipleDamage.data(), multipleDamage.size(), true }
                        : rasterm::DamageView{ &damage, 1, true };
                    if (item.uiDamage) {
                        item.pixels[static_cast<std::size_t>(iteration) % item.pixels.size()] ^= 1;
                    }
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
                validation.push_back(stats.validationMilliseconds);
                conversion.push_back(stats.conversionMilliseconds);
                submit.push_back(submitElapsed);
                wire.push_back(static_cast<double>(stats.wireBytes));
                scratch.push_back(static_cast<double>(stats.scratchBytes));
                outputCapacity.push_back(static_cast<double>(stats.outputCapacityBytes));
                sinkWrites.push_back(static_cast<double>(sink.writeCalls - writeStart));
            }
        }
        if (usesPresenter) presenter.shutdown();
        else engine.shutdown();
        const std::size_t sourceBytes = item.indexed
            ? item.indices.size() + item.palette.size() * sizeof(rasterm::RgbColor)
            : item.pixels.size();
        std::cout << item.name << ',' << item.width << ',' << item.height << ','
                  << static_cast<int>(item.format) << ',' << sourceBytes << ','
                  << std::fixed << std::setprecision(3)
                  << percentile(encode, .50) << ',' << percentile(encode, .95) << ',' << percentile(encode, .99) << ','
                  << percentile(present, .50) << ',' << percentile(present, .95) << ',' << percentile(present, .99) << ','
                  << percentile(endToEnd, .50) << ',' << percentile(endToEnd, .95) << ',' << percentile(endToEnd, .99) << ','
                  << percentile(validation, .50) << ',' << percentile(conversion, .50) << ','
                  << percentile(submit, .50) << ',' << percentile(wire, .50) << ','
                  << percentile(scratch, .50) << ',' << percentile(outputCapacity, .50) << ','
                  << percentile(payload, .50) << ',' << percentile(payload, .95) << ',' << percentile(payload, .99) << ','
                  << percentile(sinkWrites, .50) << ','
                  << percentile(allocations, .50) << ',' << percentile(allocations, .95) << ','
                  << percentile(allocations, .99) << '\n';
    }
    return 0;
}
