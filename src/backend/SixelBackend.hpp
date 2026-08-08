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
    [[nodiscard]] int lastColorCount() const noexcept override;

private:
    VideoSixelEncoder encoder;
};

}