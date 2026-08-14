/* SPDX-License-Identifier: Apache-2.0 */

#include <backend/sixel/SixelBackend.hpp>

#include <algorithm>

namespace rasterm {

SixelOptions SixelBackend::optionsFor(const BackendConfiguration& configuration)
{
    SixelOptions options;
    if (configuration.quality == QualityProfile::HighQuality) {
        options = SixelOptions::forImage();
    }
    else if (configuration.quality == QualityProfile::AdaptiveVideo) {
        options = SixelOptions::forAdaptiveVideo();
        if (configuration.realtimeDither != DitherMode::None) {
            options.dither = configuration.realtimeDither;
        }
    }
    else {
        options = SixelOptions::forVideo();
        options.dither = configuration.realtimeDither;
    }
    options.adaptivePaletteLockFrames = std::max(0, configuration.adaptivePaletteLockFrames);
    options.sceneCutThreshold = std::clamp(configuration.sceneCutThreshold, 0.0f, 1.0f);
    options.persistPaletteRegisters = configuration.tuning.persistPaletteRegisters;
    options.paletteRefreshFrames = configuration.tuning.paletteRefreshFrames;
    options.maximumThreads = configuration.tuning.maximumThreads;
    options.independentRegionQuantization = configuration.tuning.independentRegionQuantization;
    return options;
}

SixelBackend::SixelBackend(const BackendConfiguration& configuration) :
    encoder(optionsFor(configuration)) {}

void SixelBackend::beginFrame(const FrameView& frame, const FramePreparation preparation)
{
    if (preparation == FramePreparation::Regional) encoder.prepareRegionalFrame(frame);
    else encoder.prepareFrame(frame);
}

void SixelBackend::beginFrame(const IndexedFrameView& frame, const FramePreparation preparation)
{
    if (preparation == FramePreparation::Regional) encoder.prepareRegionalFrame(frame);
    else encoder.prepareFrame(frame);
}

EncodedUpdate SixelBackend::encode(const FrameView& frame, const EncodeRequest& request)
{
    if (!request.fullFrame) encoder.prepareRegion(frame, request.region);
    encoder.setOutputLimit(request.maximumBytes);
    const std::string_view bytes = request.fullFrame
        ? encoder.encodeFrame(frame) : encoder.encodeRegion(frame, request.region);
    return {
        .bytes = bytes,
        .region = request.fullFrame
            ? DamageRegion{ 0, 0, frame.width, frame.height } : request.region,
        .stages = encoder.lastStageTimings(),
        .colorsUsed = encoder.lastColorCount(),
        .fullFrame = request.fullFrame,
        .outputLimitExceeded = encoder.outputLimitExceeded(),
    };
}

EncodedUpdate SixelBackend::encode(const IndexedFrameView& frame, const EncodeRequest& request)
{
    if (!request.fullFrame) encoder.prepareRegion(frame, request.region);
    encoder.setOutputLimit(request.maximumBytes);
    const std::string_view bytes = request.fullFrame
        ? encoder.encodeFrame(frame) : encoder.encodeRegion(frame, request.region);
    return {
        .bytes = bytes,
        .region = request.fullFrame
            ? DamageRegion{ 0, 0, frame.width, frame.height } : request.region,
        .stages = encoder.lastStageTimings(),
        .colorsUsed = encoder.lastColorCount(),
        .fullFrame = request.fullFrame,
        .outputLimitExceeded = encoder.outputLimitExceeded(),
    };
}

void SixelBackend::invalidate(const BackendInvalidation invalidation) noexcept
{
    if (invalidation == BackendInvalidation::PaletteRegisters) {
        encoder.invalidatePaletteRegisters();
    }
    else {
        encoder.reset();
    }
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
