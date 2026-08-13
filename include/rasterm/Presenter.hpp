/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Config.hpp>
#include <rasterm/Buffer.hpp>
#include <rasterm/Error.hpp>
#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>
#include <rasterm/Statistics.hpp>

#include <chrono>

namespace rasterm {

class Engine;

class Presenter {
public:
    Presenter();
    ~Presenter();

    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;
    Presenter(Presenter&&) noexcept;
    Presenter& operator=(Presenter&&) noexcept;

    [[nodiscard]] Status initialize(const PresenterOptions& options = {});

    /* initialize, shutdown, move, and destruction must not run from callbacks
       invoked by the Presenter worker. read only queries remain reentrant. */

    void shutdown() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool waitUntilIdle(std::chrono::milliseconds timeout);
    [[nodiscard]] bool invalidate();

    /* copies use a capacity one mailbox and never wait for terminal output.
       replacing a compatible pending regional frame preserves accumulated damage.
       shared submissions retain lifetime through presentation lifetime
       must own pixels, palettes, damage rectangles, and referenced metadata. */

    [[nodiscard]] bool submit(const FrameView& frame);
    [[nodiscard]] bool submit(const IndexedFrameView& frame);
    [[nodiscard]] bool submit(OwnedFrame&& frame);
    [[nodiscard]] bool submit(OwnedIndexedFrame&& frame);
    [[nodiscard]] bool submitShared(const SharedFrameView& frame);
    [[nodiscard]] bool submitShared(const SharedIndexedFrameView& frame);
    [[nodiscard]] PresenterStats stats() const noexcept;
    [[nodiscard]] Status status() const;

private:
    static RenderStats renderValidatedIndexed(Engine& engine, const IndexedFrameView& frame);

    class Impl;
    Impl* impl = nullptr;
};

}
