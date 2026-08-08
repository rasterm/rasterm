/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <string_view>

namespace rasterm {

class GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;

    virtual std::string_view encodeFrame(const FrameView& frame) = 0;
    virtual std::string_view encodeFrame(const IndexedFrameView& frame) = 0;
    virtual std::string_view encodeRegion(const FrameView& frame, DamageRegion region) = 0;
    virtual std::string_view encodeRegion(const IndexedFrameView& frame, DamageRegion region) = 0;
    [[nodiscard]] virtual int lastColorCount() const noexcept = 0;
};

}