/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/AdaptiveResolution.hpp>

int main()
{
    rasterm::rPlayer::AdaptiveResolution fixed(
        1.0 / 60.0, rasterm::rPlayer::fixedResolutionProfile(0.80));
    for (int frame = 0; frame < 1000; ++frame) {
        if (fixed.observe(1.0) || fixed.scale() != 0.80) return 4;
    }

    const auto stable = rasterm::rPlayer::stableAdaptiveProfile();
    const auto quality = rasterm::rPlayer::qualityAdaptiveProfile();
    if (stable.resizeCooldownFrames <= quality.resizeCooldownFrames ||
        stable.maximumScale >= quality.maximumScale) return 5;

    rasterm::rPlayer::AdaptiveResolution resolution(1.0 / 24.0);
    if (resolution.scale() < 0.69 || resolution.scale() > 0.71) {
        return 1;
    }
    for (int frame = 0; frame < 2400; ++frame) {
        resolution.observe(0.010);
    }
    if (resolution.scale() < 0.99 || resolution.scale() > 1.0) {
        return 2;
    }
    for (int frame = 0; frame < 120; ++frame) {
        resolution.observe(0.060);
    }
    if (resolution.scale() >= 1.0 || resolution.scale() < 0.75) {
        return 3;
    }
    return 0;
}