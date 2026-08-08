/* SPDX-License-Identifier: Apache-2.0 */

#include <backend/SixelBackend.hpp>

#include <utility>

namespace rasterm {

SixelBackend::SixelBackend(SixelOptions options) : encoder(std::move(options)) {}

std::string_view SixelBackend::encodeFrame(const FrameView& frame)
{
    return encoder.encodeFrame(frame);
}

std::string_view SixelBackend::encodeFrame(const IndexedFrameView& frame)
{
    return encoder.encodeFrame(frame);
}

std::string_view SixelBackend::encodeRegion(const FrameView& frame, const DamageRegion region)
{
    return encoder.encodeRegion(frame, region);
}

std::string_view SixelBackend::encodeRegion(const IndexedFrameView& frame,
                                            const DamageRegion region)
{
    return encoder.encodeRegion(frame, region);
}

int SixelBackend::lastColorCount() const noexcept
{
    return encoder.lastColorCount();
}

}