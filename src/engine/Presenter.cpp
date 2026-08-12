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
        renderingActive = false;
        resetRequested = false;
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
                if (pendingGeneration != consumedGeneration) {
                    ++statistics.cancelledFrames;
                    consumedGeneration = pendingGeneration;
                }
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
            pendingDamageSupplied = false;
            renderingDamageSupplied = false;
            pendingLifetime.reset();
            renderingLifetime.reset();
            renderingActive = false;
            resetRequested = false;
        }
        idle.notify_all();
        engine.shutdown();
    }

    bool isInitialized() const noexcept
    {
        std::lock_guard lock(mutex);
        return running;
    }

    bool waitUntilIdle(const std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mutex);
        if (worker.joinable() && worker.get_id() == std::this_thread::get_id()) {
            latestStatus = Status::failure(
                ErrorCode::InvalidArgument, "Presenter drain is forbidden from a worker callback.");
            return false;
        }
        if (!running || stopping) {
            latestStatus = Status::failure(ErrorCode::PresenterStopped,
                                           "Presenter is not running.");
            return false;
        }
        if (timeout.count() < 0) {
            latestStatus = Status::failure(ErrorCode::InvalidArgument,
                                           "Presenter drain timeout must not be negative.");
            return false;
        }
        const bool completed = idle.wait_for(lock, timeout, [&] {
            return pendingGeneration == consumedGeneration && !renderingActive && !resetRequested;
        });
        latestStatus = completed
            ? Status::success()
            : Status::failure(ErrorCode::TimedOut, "Presenter did not become idle before the timeout.");
        return completed;
    }

    bool invalidate()
    {
        {
            std::lock_guard lock(mutex);
            if (!running || stopping) {
                ++statistics.rejectedFrames;
                latestStatus = Status::failure(ErrorCode::PresenterStopped,
                                               "Presenter is not running.");
                return false;
            }
            resetRequested = true;
            latestStatus = Status::success();
        }
        ready.notify_one();
        return true;
    }

    bool submit(const FrameView& frame)
    {
        if (!frame.isValid()) {
            return reject(ErrorCode::InvalidArgument, "Packed frame validation failed.");
        }

        try {
            const std::size_t rowBytes = static_cast<std::size_t>(frame.width) *
                bytesPerPixel(frame.format);
            const std::size_t frameBytes = rowBytes * static_cast<std::size_t>(frame.height);
            bool replaced = false;
            FrameMetadata droppedMetadata{};
            {
                std::lock_guard lock(mutex);
                if (!running || stopping) {
                    return rejectLocked(ErrorCode::PresenterStopped, "Presenter is not running.");
                }
                FrameMetadata replacedMetadata{};
                if (pendingGeneration != consumedGeneration) {
                    replacedMetadata = pendingKind == FrameKind::Packed
                        ? pendingFrame.metadata : pendingIndexedFrame.metadata;
                    ++statistics.replacedFrames;
                    replaced = true;
                }
                const bool compatibleDamage = !replaced ||
                    (pendingKind == FrameKind::Packed
                        ? pendingFrame.width == frame.width && pendingFrame.height == frame.height
                        : pendingIndexedFrame.width == frame.width &&
                            pendingIndexedFrame.height == frame.height);

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
                mergePendingDamage(frame.metadata.damage, replaced, compatibleDamage,
                                   pendingFrame.metadata.damage);
                pendingKind = FrameKind::Packed;
                pendingPalette.clear();
                pendingLifetime.reset();
                ++pendingGeneration;
                ++statistics.submittedFrames;
                latestStatus = Status::success();
                if (replaced) droppedMetadata = replacedMetadata;
            }
            if (replaced) emitDropped(droppedMetadata);
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
            return reject(ErrorCode::InvalidArgument, "Indexed frame validation failed.");
        }

        try {
            const std::size_t rowBytes = static_cast<std::size_t>(frame.width);
            const std::size_t frameBytes = rowBytes * static_cast<std::size_t>(frame.height);
            bool replaced = false;
            FrameMetadata droppedMetadata{};
            {
                std::lock_guard lock(mutex);
                if (!running || stopping) {
                    return rejectLocked(ErrorCode::PresenterStopped, "Presenter is not running.");
                }
                FrameMetadata replacedMetadata{};
                if (pendingGeneration != consumedGeneration) {
                    replacedMetadata = pendingKind == FrameKind::Packed
                        ? pendingFrame.metadata : pendingIndexedFrame.metadata;
                    ++statistics.replacedFrames;
                    replaced = true;
                }
                const bool compatibleDamage = !replaced ||
                    (pendingKind == FrameKind::Packed
                        ? pendingFrame.width == frame.width && pendingFrame.height == frame.height
                        : pendingIndexedFrame.width == frame.width &&
                            pendingIndexedFrame.height == frame.height);

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
                mergePendingDamage(frame.metadata.damage, replaced, compatibleDamage,
                                   pendingIndexedFrame.metadata.damage);
                pendingKind = FrameKind::Indexed;
                pendingIndexedTrusted = true;
                pendingLifetime.reset();
                ++pendingGeneration;
                ++statistics.submittedFrames;
                latestStatus = Status::success();
                if (replaced) droppedMetadata = replacedMetadata;
            }
            if (replaced) emitDropped(droppedMetadata);
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
        if (!frame.isValid()) return reject(ErrorCode::InvalidArgument, "Owned frame validation failed.");
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
        if (!frame.isValid()) return reject(ErrorCode::InvalidArgument, "Owned indexed frame validation failed.");
        try {
            auto owner = std::make_shared<OwnedIndexedFrame>(std::move(frame));
            return submitSharedIndexed(
                SharedIndexedFrameView{ owner->view(), std::move(owner) }, true);
        }
        catch (const std::bad_alloc&) {
            return allocationFailure();
        }
    }

    bool submitShared(const SharedFrameView& shared)
    {
        if (!shared.isValid()) return reject(ErrorCode::InvalidArgument, "Shared frame validation failed.");
        bool replaced = false;
        FrameMetadata droppedMetadata{};
        {
            std::lock_guard lock(mutex);
            if (!running || stopping) return rejectLocked(ErrorCode::PresenterStopped, "Presenter is not running.");
            FrameMetadata replacedMetadata{};
            if (pendingGeneration != consumedGeneration) {
                replacedMetadata = pendingKind == FrameKind::Packed
                    ? pendingFrame.metadata : pendingIndexedFrame.metadata;
                ++statistics.replacedFrames;
                replaced = true;
            }
            const bool compatibleDamage = !replaced ||
                (pendingKind == FrameKind::Packed
                    ? pendingFrame.width == shared.frame.width &&
                        pendingFrame.height == shared.frame.height
                    : pendingIndexedFrame.width == shared.frame.width &&
                        pendingIndexedFrame.height == shared.frame.height);
            pending.clear();
            pendingPalette.clear();
            pendingFrame = shared.frame;
            mergePendingDamage(shared.frame.metadata.damage, replaced, compatibleDamage,
                               pendingFrame.metadata.damage);
            pendingLifetime = shared.lifetime;
            pendingKind = FrameKind::Packed;
            ++pendingGeneration;
            ++statistics.submittedFrames;
            latestStatus = Status::success();
            if (replaced) droppedMetadata = replacedMetadata;
        }
        if (replaced) emitDropped(droppedMetadata);
        ready.notify_one();
        return true;
    }

    bool submitShared(const SharedIndexedFrameView& shared)
    {
        return submitSharedIndexed(shared, false);
    }

    bool submitSharedIndexed(const SharedIndexedFrameView& shared, const bool trusted)
    {
        const bool valid = trusted ? shared.lifetime && shared.frame.isValid() : shared.isValid();
        if (!valid) return reject(ErrorCode::InvalidArgument, "Shared indexed frame validation failed.");
        bool replaced = false;
        FrameMetadata droppedMetadata{};
        {
            std::lock_guard lock(mutex);
            if (!running || stopping) return rejectLocked(ErrorCode::PresenterStopped, "Presenter is not running.");
            FrameMetadata replacedMetadata{};
            if (pendingGeneration != consumedGeneration) {
                replacedMetadata = pendingKind == FrameKind::Packed
                    ? pendingFrame.metadata : pendingIndexedFrame.metadata;
                ++statistics.replacedFrames;
                replaced = true;
            }
            const bool compatibleDamage = !replaced ||
                (pendingKind == FrameKind::Packed
                    ? pendingFrame.width == shared.frame.width &&
                        pendingFrame.height == shared.frame.height
                    : pendingIndexedFrame.width == shared.frame.width &&
                        pendingIndexedFrame.height == shared.frame.height);
            pending.clear();
            pendingPalette.clear();
            pendingIndexedFrame = shared.frame;
            mergePendingDamage(shared.frame.metadata.damage, replaced, compatibleDamage,
                               pendingIndexedFrame.metadata.damage);
            pendingLifetime = shared.lifetime;
            pendingKind = FrameKind::Indexed;
            pendingIndexedTrusted = trusted;
            ++pendingGeneration;
            ++statistics.submittedFrames;
            latestStatus = Status::success();
            if (replaced) droppedMetadata = replacedMetadata;
        }
        if (replaced) emitDropped(droppedMetadata);
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
        ++statistics.rejectedFrames;
        latestStatus = Status::failure(ErrorCode::OutOfMemory, "Out of memory.");
        return false;
    }

    bool reject(const ErrorCode code, const char* message)
    {
        std::lock_guard lock(mutex);
        return rejectLocked(code, message);
    }

    bool rejectLocked(const ErrorCode code, const char* message)
    {
        ++statistics.rejectedFrames;
        latestStatus = Status::failure(code, message);
        return false;
    }

    void mergePendingDamage(const DamageView source, const bool replaced,
                            const bool compatible, DamageView& destination)
    {
        if (!replaced) {
            pendingDamage.clear();
            pendingDamageSupplied = source.supplied;
        }
        else if (!compatible || !pendingDamageSupplied || !source.supplied) {
            pendingDamage.clear();
            pendingDamageSupplied = false;
        }
        if (pendingDamageSupplied && source.count > 0) {
            pendingDamage.insert(pendingDamage.end(), source.rectangles,
                                 source.rectangles + source.count);
        }
        destination = { pendingDamage.empty() ? nullptr : pendingDamage.data(),
                        pendingDamage.size(), pendingDamageSupplied };
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
            Clock::time_point presentationStart;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&] {
                    return stopping || resetRequested || pendingGeneration != consumedGeneration;
                });
                if (stopping) {
                    return;
                }
                if (resetRequested) {
                    resetRequested = false;
                    lock.unlock();
                    engine.reset();
                    lock.lock();
                    idle.notify_all();
                    continue;
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
                std::swap(renderingDamageSupplied, pendingDamageSupplied);
                renderingLifetime.swap(pendingLifetime);
                kind = pendingKind;
                renderingIndexedTrusted = pendingIndexedTrusted;
                if (kind == FrameKind::Packed) {
                    frame = pendingFrame;
                    if (!renderingLifetime) frame.data = rendering.data();
                    frame.metadata.damage = {
                        renderingDamage.empty() ? nullptr : renderingDamage.data(),
                        renderingDamage.size(), renderingDamageSupplied,
                    };
                }
                else {
                    indexedFrame = pendingIndexedFrame;
                    if (!renderingLifetime) {
                        indexedFrame.indices = rendering.data();
                        indexedFrame.palette = { renderingPalette.data(), renderingPalette.size() };
                    }
                    indexedFrame.metadata.damage = {
                        renderingDamage.empty() ? nullptr : renderingDamage.data(),
                        renderingDamage.size(), renderingDamageSupplied,
                    };
                }
                consumedGeneration = pendingGeneration;
                renderingActive = true;
                presentationStart = Clock::now();
            }

            const RenderStats rendered = kind == FrameKind::Packed
                ? engine.renderFrame(frame)
                : renderingIndexedTrusted
                    ? Presenter::renderValidatedIndexed(engine, indexedFrame)
                    : engine.renderFrame(indexedFrame);
            nextFrame = presentationStart + minimumInterval;
            {
                std::lock_guard lock(mutex);
                statistics.latestRender = rendered;
                if (rendered.error != ErrorCode::None) {
                    latestStatus = engine.status();
                    ++statistics.failedFrames;
                }
                else if (rendered.rendered) {
                    ++statistics.presentedFrames;
                    latestStatus = Status::success();
                }
                else {
                    ++statistics.unchangedFrames;
                    latestStatus = Status::success();
                }
                renderingActive = false;
            }
            idle.notify_all();
        }
    }

    PresenterOptions options{};
    Engine engine;
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable idle;
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
    bool pendingIndexedTrusted = false;
    bool renderingIndexedTrusted = false;
    bool pendingDamageSupplied = false;
    bool renderingDamageSupplied = false;
    std::uint64_t pendingGeneration = 0;
    std::uint64_t consumedGeneration = 0;
    PresenterStats statistics{};
    Status latestStatus{};
    bool running = false;
    bool stopping = false;
    bool renderingActive = false;
    bool resetRequested = false;
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

bool Presenter::waitUntilIdle(const std::chrono::milliseconds timeout)
{
    return impl && impl->waitUntilIdle(timeout);
}

bool Presenter::invalidate()
{
    return impl && impl->invalidate();
}

RenderStats Presenter::renderValidatedIndexed(Engine& engine, const IndexedFrameView& frame)
{
    return engine.renderValidatedIndexedFrame(frame);
}

}
