/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view chunk) noexcept override
    {
        std::lock_guard lock(mutex);
        bytes.append(chunk);
        return true;
    }

    bool flush() noexcept override
    {
        return true;
    }

    [[nodiscard]] bool contains(const std::string_view text) const
    {
        std::lock_guard lock(mutex);
        return bytes.find(text) != std::string::npos;
    }

private:
    mutable std::mutex mutex;
    std::string bytes;
};

}

int main()
{
    constexpr int width = 32;
    constexpr int height = 24;
    std::vector<std::uint8_t> rgba(width * height * 4, 255);
    MemorySink output;

    rasterm::Engine engine;
    if (!engine.initialize({ .output = &output })) {
        return 1;
    }
    const rasterm::RenderStats rendered = engine.renderFrame(
        rgba.data(), width, height, width * 4, rasterm::PixelFormat::RGBA32);
    engine.shutdown();
    if (!rendered.rendered || rendered.payloadBytes == 0 || !output.contains("\x1bP")) {
        return 2;
    }

    if (!engine.initialize({ .quality = rasterm::QualityProfile::HighQuality, .output = &output })) {
        return 3;
    }
    const auto highQuality = engine.renderFrame(
        rgba.data(), width, height, width * 4, rasterm::PixelFormat::BGRA32);
    engine.shutdown();
    if (!highQuality.rendered || highQuality.payloadBytes == 0) {
        return 4;
    }

    rasterm::Presenter presenter;
    if (!presenter.initialize({ .engine = { .output = &output } })) {
        return 5;
    }
    for (int frame = 0; frame < 8; ++frame) {
        rgba[0] = static_cast<std::uint8_t>(frame * 16);
        if (!presenter.submit({ rgba.data(), width, height, width * 4,
                                rasterm::PixelFormat::RGBA32 })) {
            return 6;
        }
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (presenter.stats().presentedFrames == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    presenter.shutdown();
    const rasterm::PresenterStats stats = presenter.stats();
    if (stats.submittedFrames != 8 || stats.presentedFrames == 0 || stats.replacedFrames == 0) {
        return 7;
    }

    std::vector<std::uint8_t> indices(width * height);
    for (std::size_t index = 0; index < indices.size(); ++index) {
        indices[index] = static_cast<std::uint8_t>(index % 4);
    }
    std::vector<rasterm::RgbColor> palette{
        { 255, 0, 0 },
        { 165, 42, 42 },
        { 128, 128, 128 },
        { 0, 0, 255 },
    };
    MemorySink indexedOutput;
    if (!engine.initialize({ .enableDirtyRegions = true, .output = &indexedOutput })) {
        return 8;
    }
    rasterm::IndexedFrameView indexed{
        .indices = indices.data(),
        .width = width,
        .height = height,
        .stride = width,
        .palette = { palette.data(), palette.size() },
    };
    const auto exact = engine.renderFrame(indexed);
    if (!exact.rendered || !exact.fullFrame || exact.colorsUsed != 4 ||
        !indexedOutput.contains("#1;2;100;0;0") ||
        !indexedOutput.contains("#2;2;65;16;16")) {
        return 9;
    }

    palette[1] = { 255, 255, 0 };
    const auto paletteChange = engine.renderFrame(indexed);
    if (!paletteChange.rendered || !paletteChange.fullFrame ||
        !indexedOutput.contains("#2;2;100;100;0")) {
        return 10;
    }

    indices[0] = 4;
    if (engine.renderFrame(indexed).rendered) {
        return 11;
    }
    engine.shutdown();

    MemorySink indexedPresenterOutput;
    if (!presenter.initialize({ .engine = { .output = &indexedPresenterOutput } })) {
        return 12;
    }
    if (presenter.submit(indexed)) {
        return 13;
    }
    indices[0] = 0;
    if (!presenter.submit(indexed)) {
        return 14;
    }
    const auto indexedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (presenter.stats().presentedFrames == 0 &&
           std::chrono::steady_clock::now() < indexedDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    presenter.shutdown();
    if (presenter.stats().presentedFrames != 1 ||
        !indexedPresenterOutput.contains("#1;2;100;0;0")) {
        return 15;
    }
    return 0;
}