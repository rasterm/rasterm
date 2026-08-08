/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/Presenter.hpp>
#include <rasterm/Engine.hpp>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace rasterm {

class Presenter::Impl {
public:
    ~Impl()
    {
        shutdown();
    }

    Status initialize(const PresenterOptions& requestedOptions)
    {
        shutdown();
        if (!std::isfinite(requestedOptions.maximumFramesPerSecond) ||
            requestedOptions.maximumFramesPerSecond < 0.0) {
            latestStatus = Status::failure(ErrorCode::InvalidArgument,
                                           "Presenter frame rate limit is invalid.");
            return latestStatus;
        }
        const Status engineStatus = engine.initialize(requestedOptions.engine);
        if (!engineStatus) {
            latestStatus = engineStatus;
            return latestStatus;
        }

        options = requestedOptions;
        statistics = {};
        pendingGeneration = 0;
        consumedGeneration = 0;
        stopping = false;
        running = true;
        try {
            worker = std::thread(&Impl::presentLoop, this);
        }
        catch (const std::bad_alloc&) {
            running = false;
            engine.shutdown();
            latestStatus = Status::failure(ErrorCode::OutOfMemory,
                                           "Out of memory.");
            return latestStatus;
        }
        catch (const std::exception& error) {
            running = false;
            engine.shutdown();
            latestStatus = Status::failure(ErrorCode::PresenterThreadStartFailed, error.what());
            return latestStatus;
        }
        catch (...) {
            running = false;
            engine.shutdown();
            latestStatus = Status::failure(ErrorCode::PresenterThreadStartFailed,
                                           "Unable to start the presenter thread.");
            return latestStatus;
        }
        latestStatus = Status::success();
        return latestStatus;
    }

    void shutdown() noexcept
    {
        {
            std::lock_guard lock(mutex);
            if (worker.joinable() && worker.get_id() == std::this_thread::get_id()) {
                stopping = true;
                latestStatus.code = ErrorCode::InvalidArgument;
                try {
                    latestStatus.message = "Presenter shutdown is forbidden from a worker callback.";
                }
                catch (...) {
                    latestStatus.message.clear();
                }
                ready.notify_one();
                return;
            }
            if (running) {
                stopping = true;
            }
        }
        ready.notify_one();
        if (worker.joinable()) {
            worker.join();
        }

        {
            std::lock_guard lock(mutex);
            running = false;
            stopping = false;
            pendingGeneration = 0;
            consumedGeneration = 0;
            pending.clear();
            rendering.clear();
            pendingPalette.clear();
            renderingPalette.clear();
            pendingDamage.clear();
            renderingDamage.clear();
            pendingLifetime.reset();
            renderingLifetime.reset();
        }
        engine.shutdown();
    }

    bool isInitialized() const noexcept
    {
        std::lock_guard lock(mutex);
        return running;
    }

    bool submit(const FrameView& frame)
    {
        if (!frame.isValid()) {
            return false;
        }

        try {
            const std::size_t rowBytes = static_cast<std::size_t>(frame.width) *
                bytesPerPixel(frame.format);
            const std::size_t frameBytes = rowBytes * static_cast<std::size_t>(frame.height);
            bool replaced = false;
            {
                std::lock_guard lock(mutex);
                if (!running || stopping) {
                    return false;
                }
                if (pendingGeneration != consumedGeneration) {
                    ++statistics.replacedFrames;
                    replaced = true;
                }

                pending.resize(frameBytes);
                for (int row = 0; row < frame.height; ++row) {
                    std::memcpy(pending.data() + static_cast<std::size_t>(row) * rowBytes,
                                frame.data + static_cast<std::ptrdiff_t>(row) * frame.stride,
                                rowBytes);
                }
                pendingFrame = {
                    .data = pending.data(),
                    .width = frame.width,
                    .height = frame.height,
                    .stride = static_cast<std::ptrdiff_t>(rowBytes),
                    .format = frame.format,
                    .metadata = frame.metadata,
                };
                copyDamage(frame.metadata.damage, pendingDamage, pendingFrame.metadata.damage);
                pendingKind = FrameKind::Packed;
                pendingPalette.clear();
                pendingLifetime.reset();
                ++pendingGeneration;
                ++statistics.submittedFrames;
            }
            if (replaced) emitDropped(frame.metadata);
            ready.notify_one();
            return true;
        }
        catch (const std::bad_alloc&) {
            std::lock_guard lock(mutex);
            latestStatus = Status::failure(ErrorCode::OutOfMemory,
                                           "Out of memory.");
            return false;
        }
    }

    bool submit(const IndexedFrameView& frame)
    {
        if (!frame.hasValidIndices()) {
            return false;
        }

        try {
            const std::size_t rowBytes = static_cast<std::size_t>(frame.width);
            const std::size_t frameBytes = rowBytes * static_cast<std::size_t>(frame.height);
            bool replaced = false;
            {
                std::lock_guard lock(mutex);
                if (!running || stopping) {
                    return false;
                }
                if (pendingGeneration != consumedGeneration) {
                    ++statistics.replacedFrames;
                    replaced = true;
                }

                pending.resize(frameBytes);
                for (int row = 0; row < frame.height; ++row) {
                    std::memcpy(pending.data() + static_cast<std::size_t>(row) * rowBytes,
                                frame.indices + static_cast<std::ptrdiff_t>(row) * frame.stride,
                                rowBytes);
                }
                pendingPalette.assign(frame.palette.colors, frame.palette.colors + frame.palette.size);
                pendingIndexedFrame = {
                    .indices = pending.data(),
                    .width = frame.width,
                    .height = frame.height,
                    .stride = static_cast<std::ptrdiff_t>(rowBytes),
                    .palette = { pendingPalette.data(), pendingPalette.size() },
                    .metadata = frame.metadata,
                };
                copyDamage(frame.metadata.damage, pendingDamage, pendingIndexedFrame.metadata.damage);
                pendingKind = FrameKind::Indexed;
                pendingLifetime.reset();
                ++pendingGeneration;
                ++statistics.submittedFrames;
            }
            if (replaced) emitDropped(frame.metadata);
            ready.notify_one();
            return true;
        }
        catch (const std::bad_alloc&) {
            std::lock_guard lock(mutex);
            latestStatus = Status::failure(ErrorCode::OutOfMemory,
                                           "Out of memory.");
            return false;
        }
    }

    bool submit(OwnedFrame&& frame)
    {
        if (!frame.isValid()) return false;
        try {
            auto owner = std::make_shared<OwnedFrame>(std::move(frame));
            return submitShared(SharedFrameView{ owner->view(), std::move(owner) });
        }
        catch (const std::bad_alloc&) {
            return allocationFailure();
        }
    }

    bool submit(OwnedIndexedFrame&& frame)
    {
        if (!frame.isValid()) return false;
        try {
            auto owner = std::make_shared<OwnedIndexedFrame>(std::move(frame));
            return submitShared(SharedIndexedFrameView{ owner->view(), std::move(owner) });
        }
        catch (const std::bad_alloc&) {
            return allocationFailure();
        }
    }

    bool submitShared(const SharedFrameView& shared)
    {
        if (!shared.isValid()) return false;
        bool replaced = false;
        {
            std::lock_guard lock(mutex);
            if (!running || stopping) return false;
            if (pendingGeneration != consumedGeneration) {
                ++statistics.replacedFrames;
                replaced = true;
            }
            pending.clear();
            pendingPalette.clear();
            pendingDamage.clear();
            pendingFrame = shared.frame;
            pendingLifetime = shared.lifetime;
            pendingKind = FrameKind::Packed;
            ++pendingGeneration;
            ++statistics.submittedFrames;
        }
        if (replaced) emitDropped(shared.frame.metadata);
        ready.notify_one();
        return true;
    }

    bool submitShared(const SharedIndexedFrameView& shared)
    {
        if (!shared.isValid()) return false;
        bool replaced = false;
        {
            std::lock_guard lock(mutex);
            if (!running || stopping) return false;
            if (pendingGeneration != consumedGeneration) {
                ++statistics.replacedFrames;
                replaced = true;
            }
            pending.clear();
            pendingPalette.clear();
            pendingDamage.clear();
            pendingIndexedFrame = shared.frame;
            pendingLifetime = shared.lifetime;
            pendingKind = FrameKind::Indexed;
            ++pendingGeneration;
            ++statistics.submittedFrames;
        }
        if (replaced) emitDropped(shared.frame.metadata);
        ready.notify_one();
        return true;
    }

    PresenterStats stats() const noexcept
    {
        std::lock_guard lock(mutex);
        return statistics;
    }

    Status status() const
    {
        std::lock_guard lock(mutex);
        return latestStatus;
    }

private:
    enum class FrameKind {
        Packed,
        Indexed,
    };

    bool allocationFailure()
    {
        std::lock_guard lock(mutex);
        latestStatus = Status::failure(ErrorCode::OutOfMemory, "Out of memory.");
        return false;
    }

    static void copyDamage(const DamageView source, std::vector<DamageRect>& storage,
                           DamageView& destination)
    {
        storage.clear();
        if (source.supplied && source.count > 0) {
            storage.assign(source.rectangles, source.rectangles + source.count);
        }
        destination = { storage.empty() ? nullptr : storage.data(), storage.size(), source.supplied };
    }

    void emitDropped(const FrameMetadata metadata) const noexcept
    {
        if (options.engine.events.callback != nullptr) {
            options.engine.events.callback({
                .type = EventType::FrameDropped,
                .frameId = metadata.frameId,
                .timestampNanoseconds = metadata.timestampNanoseconds,
            }, options.engine.events.context);
        }
    }

    void presentLoop()
    {
        using Clock = std::chrono::steady_clock;
        const auto minimumInterval = options.maximumFramesPerSecond > 0.0
            ? std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / options.maximumFramesPerSecond))
            : Clock::duration::zero();
        auto nextFrame = Clock::now();

        for (;;) {
            FrameKind kind = FrameKind::Packed;
            FrameView frame;
            IndexedFrameView indexedFrame;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&] { return stopping || pendingGeneration != consumedGeneration; });
                if (stopping) {
                    return;
                }
                if (minimumInterval != Clock::duration::zero()) {
                    ready.wait_until(lock, nextFrame, [&] { return stopping; });
                    if (stopping) {
                        return;
                    }
                }

                rendering.swap(pending);
                renderingPalette.swap(pendingPalette);
                renderingDamage.swap(pendingDamage);
                renderingLifetime.swap(pendingLifetime);
                kind = pendingKind;
                if (kind == FrameKind::Packed) {
                    frame = pendingFrame;
                    if (!renderingLifetime) {
                        frame.data = rendering.data();
                        frame.metadata.damage = {
                            renderingDamage.empty() ? nullptr : renderingDamage.data(),
                            renderingDamage.size(),
                            pendingFrame.metadata.damage.supplied,
                        };
                    }
                }
                else {
                    indexedFrame = pendingIndexedFrame;
                    if (!renderingLifetime) {
                        indexedFrame.indices = rendering.data();
                        indexedFrame.palette = { renderingPalette.data(), renderingPalette.size() };
                        indexedFrame.metadata.damage = {
                            renderingDamage.empty() ? nullptr : renderingDamage.data(),
                            renderingDamage.size(),
                            pendingIndexedFrame.metadata.damage.supplied,
                        };
                    }
                }
                consumedGeneration = pendingGeneration;
            }

            const RenderStats rendered = kind == FrameKind::Packed
                ? engine.renderFrame(frame)
                : engine.renderFrame(indexedFrame);
            nextFrame = Clock::now() + minimumInterval;
            {
                std::lock_guard lock(mutex);
                statistics.latestRender = rendered;
                if (rendered.error != ErrorCode::None) {
                    latestStatus = engine.status();
                }
                if (rendered.rendered) {
                    ++statistics.presentedFrames;
                }
            }
        }
    }

    PresenterOptions options{};
    Engine engine;
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::vector<std::uint8_t> pending;
    std::vector<std::uint8_t> rendering;
    std::vector<RgbColor> pendingPalette;
    std::vector<RgbColor> renderingPalette;
    std::vector<DamageRect> pendingDamage;
    std::vector<DamageRect> renderingDamage;
    std::shared_ptr<const void> pendingLifetime;
    std::shared_ptr<const void> renderingLifetime;
    FrameView pendingFrame{};
    IndexedFrameView pendingIndexedFrame{};
    FrameKind pendingKind = FrameKind::Packed;
    std::uint64_t pendingGeneration = 0;
    std::uint64_t consumedGeneration = 0;
    PresenterStats statistics{};
    Status latestStatus{};
    bool running = false;
    bool stopping = false;
};

Presenter::Presenter() : impl(new Impl) {}
Presenter::~Presenter() { delete impl; }

Presenter::Presenter(Presenter&& other) noexcept : impl(std::exchange(other.impl, nullptr)) {}

Presenter& Presenter::operator=(Presenter&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = std::exchange(other.impl, nullptr);
    }
    return *this;
}

Status Presenter::initialize(const PresenterOptions& options)
{
    return impl ? impl->initialize(options)
                : Status::failure(ErrorCode::InitializationException,
                                  "Presenter implementation is unavailable.");
}

void Presenter::shutdown() noexcept
{
    if (impl) {
        impl->shutdown();
    }
}

bool Presenter::isInitialized() const noexcept
{
    return impl && impl->isInitialized();
}

bool Presenter::submit(const FrameView& frame)
{
    return impl && impl->submit(frame);
}

bool Presenter::submit(const IndexedFrameView& frame)
{
    return impl && impl->submit(frame);
}

bool Presenter::submit(OwnedFrame&& frame)
{
    return impl && impl->submit(std::move(frame));
}

bool Presenter::submit(OwnedIndexedFrame&& frame)
{
    return impl && impl->submit(std::move(frame));
}

bool Presenter::submitShared(const SharedFrameView& frame)
{
    return impl && impl->submitShared(frame);
}

bool Presenter::submitShared(const SharedIndexedFrameView& frame)
{
    return impl && impl->submitShared(frame);
}

PresenterStats Presenter::stats() const noexcept
{
    return impl ? impl->stats() : PresenterStats{};
}

Status Presenter::status() const
{
    return impl ? impl->status()
                : Status::failure(ErrorCode::InitializationException,
                                  "Presenter implementation is unavailable.");
}

}