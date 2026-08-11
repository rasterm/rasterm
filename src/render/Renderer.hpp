/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageTracker.hpp>
#include <damage/DamageRegion.hpp>

#include <backend/GraphicsBackend.hpp>
#include <encoder/SixelEncoder.hpp>

#include <output/TerminalRenderer.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace rasterm {

struct RendererOptions {
    bool useAlternateScreen = false;
    bool preserveCursor = true;
    bool enableDirtyRegions = true;
    bool useSynchronizedOutput = true;
    std::size_t maximumOutputBytes = 0;
    std::size_t outputChunkBytes = 64 * 1024;
    TerminalCellPixels cellPixels{};
    DamageOptions damage{};
    SixelOptions sixel = SixelOptions::ForRealtimeVideo();
};

struct RenderResult {
    ErrorCode error = ErrorCode::None;
    bool rendered = false;
    bool usedFullFrame = false;
    std::size_t outputBytes = 0;
    std::size_t wireBytes = 0;
    std::size_t scratchBytes = 0;
    std::size_t outputCapacityBytes = 0;
    std::uint32_t dirtyRegionCount = 0;
    int colorsUsed = 0;
    std::chrono::microseconds encodeDuration{};
    std::chrono::microseconds presentDuration{};
};

class Renderer {
public:
    explicit Renderer(OutputSink& output, RendererOptions options = {});

    ErrorCode clear();
    void reset();
    void updateCellPixels(TerminalCellPixels cellPixels);
    [[nodiscard]] bool good() const noexcept { return terminal.good(); }
    [[nodiscard]] ErrorCode error() const noexcept { return terminal.error(); }
    RenderResult render(const FrameView& frame);
    RenderResult render(const IndexedFrameView& frame);
    RenderResult renderValidated(const IndexedFrameView& frame);

private:
    struct EncodedRegion {
        DamageRegion region{};
        std::size_t offset = 0;
        std::size_t size = 0;
        int colorsUsed = 0;
    };

    template<typename Frame>
    RenderResult renderFrame(const Frame& frame);
    [[nodiscard]] std::size_t scratchCapacity() const noexcept;
    [[nodiscard]] std::size_t outputCapacity() const noexcept;

    RendererOptions options;
    TerminalRenderer terminal;
    std::unique_ptr<GraphicsBackend> backend;
    DamageTracker damage;
    std::vector<DamageRegion> suppliedDamage;
    std::vector<DamageRegion> presentationDamage;
    std::string transactionBytes;
    std::vector<EncodedRegion> encodedRegions;
    bool usingSuppliedDamage = false;
    int previousWidth = 0;
    int previousHeight = 0;
    int previousFormatTag = -1;
    std::vector<RgbColor> previousPalette;
};

}
