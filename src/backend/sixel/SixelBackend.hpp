/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <backend/GraphicsBackend.hpp>

#include <encoder/sixel/SixelEncoder.hpp>

namespace rasterm {

class SixelBackend final : public GraphicsBackend {
public:
    explicit SixelBackend(const BackendConfiguration& configuration);

    void beginFrame(const FrameView& frame, FramePreparation preparation) override;
    void beginFrame(const IndexedFrameView& frame, FramePreparation preparation) override;
    [[nodiscard]] EncodedUpdate encode(const FrameView& frame,
                                       const EncodeRequest& request) override;
    [[nodiscard]] EncodedUpdate encode(const IndexedFrameView& frame,
                                       const EncodeRequest& request) override;
    void invalidate(BackendInvalidation invalidation) noexcept override;
    [[nodiscard]] std::size_t outputCapacity() const noexcept override;
    [[nodiscard]] std::size_t scratchCapacity() const noexcept override;

private:
    static SixelOptions optionsFor(const BackendConfiguration& configuration);
    SixelEncoder encoder;
};

}
