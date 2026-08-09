/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

namespace rasterm::rPlayer {

struct AdaptiveResolutionOptions {
    bool enabled = true;
    double initialScale = 0.70;
    double minimumScale = 0.30;
    double maximumScale = 1.00;
    int warmupFrames = 30;
    int downscaleFrames = 8;
    int upscaleFrames = 60;
    int resizeCooldownFrames = 60;
};

[[nodiscard]] AdaptiveResolutionOptions fixedResolutionProfile(double scale) noexcept;
[[nodiscard]] AdaptiveResolutionOptions stableAdaptiveProfile() noexcept;
[[nodiscard]] AdaptiveResolutionOptions qualityAdaptiveProfile() noexcept;

class AdaptiveResolution {
public:
    AdaptiveResolution(double frameBudgetSeconds, AdaptiveResolutionOptions options = {});

    [[nodiscard]] double scale() const noexcept { return currentScale; }
    bool observe(double renderSeconds);

private:
    AdaptiveResolutionOptions options;
    double frameBudgetSeconds;
    double currentScale;
    double renderTimeAverage;
    int observedFrames = 0;
    int slowFrames = 0;
    int fastFrames = 0;
    int cooldownFrames = 0;
};

}
