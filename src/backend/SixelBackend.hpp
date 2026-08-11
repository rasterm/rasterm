/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <backend/GraphicsBackend.hpp>

#include <encoder/SixelEncoder.hpp>

namespace rasterm {

class SixelBackend final : public GraphicsBackend {
public:
    explicit SixelBackend(SixelOptions options);

    std::string_view encodeFrame(const FrameView& frame) override;
    std::string_view encodeFrame(const IndexedFrameView& frame) override;
    std::string_view encodeRegion(const FrameView& frame, DamageRegion region) override;
    std::string_view encodeRegion(const IndexedFrameView& frame, DamageRegion region) override;
    void prepareFrame(const FrameView& frame) override;
    void prepareFrame(const IndexedFrameView& frame) override;
    void prepareRegionalFrame(const FrameView& frame) override;
    void prepareRegionalFrame(const IndexedFrameView& frame) override;
    void prepareRegion(const FrameView& frame, DamageRegion region) override;
    void prepareRegion(const IndexedFrameView& frame, DamageRegion region) override;
    void reset() noexcept override;
    void setOutputLimit(std::size_t maximumBytes) noexcept override;
    [[nodiscard]] bool outputLimitExceeded() const noexcept override;
    [[nodiscard]] std::size_t outputCapacity() const noexcept override;
    [[nodiscard]] std::size_t scratchCapacity() const noexcept override;
    [[nodiscard]] int lastColorCount() const noexcept override;

private:
    VideoSixelEncoder encoder;
};

}
