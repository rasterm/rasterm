/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/AdaptiveResolution.hpp>

#include <algorithm>

namespace rasterm::rPlayer {

AdaptiveResolutionOptions fixedResolutionProfile(const double scale) noexcept
{
    AdaptiveResolutionOptions result;
    result.enabled = false;
    result.initialScale = std::clamp(scale, 0.01, 1.0);
    result.minimumScale = result.initialScale;
    result.maximumScale = result.initialScale;
    return result;
}

AdaptiveResolutionOptions stableAdaptiveProfile() noexcept
{
    AdaptiveResolutionOptions result;
    result.initialScale = 0.50;
    result.minimumScale = 0.30;
    result.maximumScale = 0.75;
    result.downscaleFrames = 12;
    result.upscaleFrames = 240;
    result.resizeCooldownFrames = 240;
    return result;
}

AdaptiveResolutionOptions qualityAdaptiveProfile() noexcept
{
    AdaptiveResolutionOptions result;
    result.initialScale = 0.75;
    result.minimumScale = 0.40;
    result.maximumScale = 1.00;
    result.downscaleFrames = 8;
    result.upscaleFrames = 180;
    result.resizeCooldownFrames = 180;
    return result;
}

AdaptiveResolution::AdaptiveResolution(const double frameBudgetSeconds,
                                       AdaptiveResolutionOptions options) :
    options(options),
    frameBudgetSeconds(frameBudgetSeconds),
    currentScale(std::clamp(options.initialScale, options.minimumScale, options.maximumScale)),
    renderTimeAverage(frameBudgetSeconds)
{
}

bool AdaptiveResolution::observe(const double renderSeconds)
{
    if (!options.enabled) {
        return false;
    }
    renderTimeAverage = renderTimeAverage * 0.95 + renderSeconds * 0.05;
    ++observedFrames;
    if (cooldownFrames > 0) {
        --cooldownFrames;
    }
    if (observedFrames <= options.warmupFrames || cooldownFrames > 0) {
        return false;
    }

    if (renderTimeAverage > frameBudgetSeconds * 0.90) {
        fastFrames = 0;
        if (++slowFrames >= options.downscaleFrames && currentScale > options.minimumScale) {
            currentScale = std::max(options.minimumScale, currentScale * 0.90);
            slowFrames = 0;
            cooldownFrames = options.resizeCooldownFrames;
            return true;
        }
    }
    else if (renderTimeAverage < frameBudgetSeconds * 0.70) {
        slowFrames = 0;
        if (++fastFrames >= options.upscaleFrames && currentScale < options.maximumScale) {
            currentScale = std::min(options.maximumScale, currentScale * 1.06);
            fastFrames = 0;
            cooldownFrames = options.resizeCooldownFrames;
            return true;
        }
    }
    else {
        slowFrames = 0;
        fastFrames = 0;
    }
    return false;
}

}