/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>
#include <rasterm/rasterm.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>

namespace {
std::atomic<bool> failNextAllocation{ false };
}

void* operator new(const std::size_t size)
{
    if (failNextAllocation.exchange(false, std::memory_order_relaxed)) throw std::bad_alloc{};
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

class Sink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override { return true; }
    bool flush() noexcept override { return true; }
};

int32_t writeBytes(void*, const char*, size_t) { return 1; }
int32_t flushBytes(void*) { return 1; }

}

int main()
{
    rasterm_engine_options cOptions;
    rasterm_engine_options_init(&cOptions);
    cOptions.write = writeBytes;
    cOptions.flush = flushBytes;
    rasterm_engine* cEngine = nullptr;
    failNextAllocation = true;
    const rasterm_result engineHandle = rasterm_engine_create(&cOptions, &cEngine);
    if (engineHandle != RASTERM_ERROR_OUT_OF_MEMORY || cEngine != nullptr) return 1;

    rasterm_presenter_options cPresenterOptions;
    rasterm_presenter_options_init(&cPresenterOptions);
    cPresenterOptions.engine.write = writeBytes;
    cPresenterOptions.engine.flush = flushBytes;
    rasterm_presenter* cPresenter = nullptr;
    failNextAllocation = true;
    const rasterm_result presenterHandle =
        rasterm_presenter_create(&cPresenterOptions, &cPresenter);
    if (presenterHandle != RASTERM_ERROR_OUT_OF_MEMORY || cPresenter != nullptr) return 2;
    Sink sink;
    bool engineConstructorFailed = false;
    failNextAllocation = true;
    try {
        rasterm::Engine initialization;
    }
    catch (const std::bad_alloc&) {
        engineConstructorFailed = true;
    }
    if (!engineConstructorFailed) return 3;

    rasterm::Engine encoder;
    if (!encoder.initialize({ .output = &sink })) return 4;
    std::array<std::uint8_t, 64 * 64 * 3> pixels{};
    failNextAllocation = true;
    const rasterm::RenderStats failedRender = encoder.renderFrame(
        pixels.data(), 64, 64, 64 * 3, rasterm::PixelFormat::RGB24);
    encoder.shutdown();
    if (failedRender.error != rasterm::ErrorCode::OutOfMemory) return 5;

    rasterm::Presenter presenter;
    if (!presenter.initialize({ .engine = { .output = &sink } })) return 6;
    failNextAllocation = true;
    const bool submitted = presenter.submit({
        pixels.data(), 64, 64, 64 * 3, rasterm::PixelFormat::RGB24
    });
    const rasterm::Status presenterStatus = presenter.status();
    presenter.shutdown();
    if (submitted || presenterStatus.code != rasterm::ErrorCode::OutOfMemory) return 7;
    return 0;
}