/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view data) noexcept override
    {
        std::lock_guard lock(mutex);
        bytes.append(data);
        return true;
    }
    bool flush() noexcept override { return true; }
private:
    std::mutex mutex;
    std::string bytes;
};

struct SharedIndexedStorage {
    std::array<std::uint8_t, 2> indices{ 0, 1 };
    std::array<rasterm::RgbColor, 2> palette{{ { 255, 0, 0 }, { 0, 0, 255 } }};
    rasterm::DamageRect damage{ 0, 0, 2, 1 };
};

void eventCallback(const rasterm::Event& event, void* context) noexcept
{
    auto* dropped = static_cast<std::uint64_t*>(context);
    if (event.type == rasterm::EventType::FrameDropped) ++*dropped;
}
}

int main()
{
    const rasterm::DamageRect full{ 0, 0, 2, 1 };
    std::array<std::uint8_t, 6> pixels{ 255, 0, 0, 0, 255, 0 };
    rasterm::FrameView frame{
        pixels.data(), 2, 1, 6, rasterm::PixelFormat::RGB24,
        { .frameId = 42, .timestampNanoseconds = 123,
          .damage = { &full, 1, true } },
    };
    if (!frame.isValid() || rasterm::FrameView{
            pixels.data(), 2, 1, 6, rasterm::PixelFormat::RGB24,
            { .damage = { nullptr, 1, true } } }.isValid()) return 1;

    rasterm::OwnedFrame owned = rasterm::OwnedFrame::copyOf(frame);
    if (!owned.isValid() || owned.view().metadata.frameId != 42) return 2;

    MemorySink sink;
    rasterm::Engine engine;
    if (!engine.initialize({ .enableDirtyRegions = true, .output = &sink })) return 3;
    if (!engine.renderFrame(frame).rendered) return 4;

    pixels[0] = 0;
    frame.metadata.damage = { nullptr, 0, true };
    if (engine.renderFrame(frame).rendered) return 5;
    frame.metadata.damage = { &full, 1, true };
    if (!engine.renderFrame(frame).rendered) return 6;
    engine.shutdown();

    std::uint64_t droppedEvents = 0;
    rasterm::Presenter presenter;
    if (!presenter.initialize({
            .engine = { .events = { eventCallback, &droppedEvents }, .output = &sink },
            .maximumFramesPerSecond = 1.0,
        })) return 7;
    rasterm::OwnedFrame moved = rasterm::OwnedFrame::copyOf(frame);
    if (!presenter.submit(std::move(moved))) return 8;
    auto sharedPixels = std::make_shared<std::vector<std::uint8_t>>(pixels.begin(), pixels.end());
    rasterm::SharedFrameView shared{
        { sharedPixels->data(), 2, 1, 6, rasterm::PixelFormat::RGB24 }, sharedPixels,
    };
    if (!presenter.submitShared(shared)) return 9;
    for (std::uint64_t id = 1; id < 20; ++id) {
        frame.metadata.frameId = id;
        if (!presenter.submit(frame)) return 10;
    }
    presenter.shutdown();
    if (presenter.stats().replacedFrames == 0 || droppedEvents == 0) return 11;

    rasterm::Presenter sharedPresenter;
    if (!sharedPresenter.initialize({ .engine = { .output = &sink } })) return 12;
    auto storage = std::make_shared<SharedIndexedStorage>();
    std::weak_ptr<SharedIndexedStorage> weak = storage;
    rasterm::SharedIndexedFrameView sharedIndexed{
        {
            storage->indices.data(), 2, 1, 2,
            { storage->palette.data(), storage->palette.size() },
            { .frameId = 99, .timestampNanoseconds = 456,
              .damage = { &storage->damage, 1, true } },
        },
        storage,
    };
    if (!sharedPresenter.submitShared(sharedIndexed)) return 13;
    storage.reset();
    sharedIndexed.lifetime.reset();
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (sharedPresenter.stats().presentedFrames == 0 &&
           std::chrono::steady_clock::now() < limit) {
        std::this_thread::yield();
    }
    if (sharedPresenter.stats().presentedFrames != 1 || weak.expired()) return 14;
    sharedPresenter.shutdown();
    if (!weak.expired()) return 15;
    return 0;
}