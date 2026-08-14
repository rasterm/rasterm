/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <backend/EncodeContracts.hpp>

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <cstddef>

namespace rasterm {

class GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;

    virtual void beginFrame(const FrameView& frame, FramePreparation preparation) = 0;
    virtual void beginFrame(const IndexedFrameView& frame, FramePreparation preparation) = 0;
    [[nodiscard]] virtual EncodedUpdate encode(const FrameView& frame,
                                               const EncodeRequest& request) = 0;
    [[nodiscard]] virtual EncodedUpdate encode(const IndexedFrameView& frame,
                                               const EncodeRequest& request) = 0;
    virtual void invalidate(BackendInvalidation invalidation) noexcept = 0;
    [[nodiscard]] virtual std::size_t outputCapacity() const noexcept = 0;
    [[nodiscard]] virtual std::size_t scratchCapacity() const noexcept = 0;
};

}
