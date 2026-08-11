/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <string_view>
#include <cstddef>

namespace rasterm {

class GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;

    virtual std::string_view encodeFrame(const FrameView& frame) = 0;
    virtual std::string_view encodeFrame(const IndexedFrameView& frame) = 0;
    virtual std::string_view encodeRegion(const FrameView& frame, DamageRegion region) = 0;
    virtual std::string_view encodeRegion(const IndexedFrameView& frame, DamageRegion region) = 0;
    virtual void prepareFrame(const FrameView& frame) = 0;
    virtual void prepareFrame(const IndexedFrameView&) {}
    virtual void prepareRegionalFrame(const FrameView& frame) { prepareFrame(frame); }
    virtual void prepareRegionalFrame(const IndexedFrameView& frame) { prepareFrame(frame); }
    virtual void prepareRegion(const FrameView&, DamageRegion) {}
    virtual void prepareRegion(const IndexedFrameView&, DamageRegion) {}
    virtual void reset() noexcept = 0;
    virtual void setOutputLimit(std::size_t maximumBytes) noexcept = 0;
    [[nodiscard]] virtual bool outputLimitExceeded() const noexcept = 0;
    [[nodiscard]] virtual std::size_t outputCapacity() const noexcept = 0;
    [[nodiscard]] virtual std::size_t scratchCapacity() const noexcept = 0;
    [[nodiscard]] virtual int lastColorCount() const noexcept = 0;
};

}
