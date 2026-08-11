/* SPDX-License-Identifier: Apache-2.0 */

#include <backend/SixelBackend.hpp>

#include <utility>

namespace rasterm {

SixelBackend::SixelBackend(SixelOptions options) : encoder(std::move(options)) {}

void SixelBackend::prepareFrame(const FrameView& frame)
{
    encoder.prepareFrame(frame);
}

void SixelBackend::prepareFrame(const IndexedFrameView& frame)
{
    encoder.prepareFrame(frame);
}

void SixelBackend::prepareRegionalFrame(const FrameView& frame)
{
    encoder.prepareRegionalFrame(frame);
}

void SixelBackend::prepareRegionalFrame(const IndexedFrameView& frame)
{
    encoder.prepareRegionalFrame(frame);
}

void SixelBackend::prepareRegion(const FrameView& frame, const DamageRegion region)
{
    encoder.prepareRegion(frame, region);
}

void SixelBackend::prepareRegion(const IndexedFrameView& frame, const DamageRegion region)
{
    encoder.prepareRegion(frame, region);
}

void SixelBackend::reset() noexcept
{
    encoder.reset();
}

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

void SixelBackend::setOutputLimit(const std::size_t maximumBytes) noexcept
{
    encoder.setOutputLimit(maximumBytes);
}

bool SixelBackend::outputLimitExceeded() const noexcept
{
    return encoder.outputLimitExceeded();
}

std::size_t SixelBackend::outputCapacity() const noexcept
{
    return encoder.outputCapacity();
}

std::size_t SixelBackend::scratchCapacity() const noexcept
{
    return encoder.scratchCapacity();
}

}
