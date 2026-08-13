/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Capabilities.hpp>
#include <rasterm/Config.hpp>
#include <rasterm/Error.hpp>
#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>
#include <rasterm/Statistics.hpp>

namespace rasterm {

class Presenter;

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    [[nodiscard]] Status initialize(const EngineOptions& options = {});
    void shutdown() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

    RenderStats renderFrame(const FrameView& frame);
    RenderStats renderFrame(const IndexedFrameView& frame);
    RenderStats renderFrame(const std::uint8_t* data, int width, int height,
                            std::ptrdiff_t stride, PixelFormat format);

    Status clear();
    void reset();
    [[nodiscard]] TerminalGeometry terminalGeometry() const noexcept;
    [[nodiscard]] TerminalCapabilities capabilities() const noexcept;
    [[nodiscard]] Status status() const;
    [[nodiscard]] RenderStats stats() const noexcept;

private:
    friend class Presenter;
    RenderStats renderValidatedIndexedFrame(const IndexedFrameView& frame);

    class Impl;
    Impl* impl = nullptr;
};

}
