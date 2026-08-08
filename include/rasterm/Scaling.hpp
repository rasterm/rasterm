/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Geometry.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rasterm {

enum class ScalePolicy : std::int32_t { Fit = 0, Fill = 1, Stretch = 2, IntegerScale = 3, Crop = 4, NoScale = 5 };
enum class ScaleFilter : std::int32_t { Nearest = 0, Linear = 1, Cubic = 2, Area = 3 };

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct ScalingOptions {
    ScalePolicy policy = ScalePolicy::Fit;
    ScaleFilter filter = ScaleFilter::Area;
    RgbColor background{ 0, 0, 0 };
};

struct ImageOptions {
    ScalingOptions scaling{};
    float sharpening = 0.0f;
    float contrast = 1.0f;
};

struct ScaleLayout {
    Rect source{};
    Rect destination{};
    Extent canvas{};
};

[[nodiscard]] inline ScaleLayout calculateScaleLayout(const Extent source,
                                                       const Extent bounds,
                                                       const ScalePolicy policy) noexcept
{
    if (source.width <= 0 || source.height <= 0 || bounds.width <= 0 || bounds.height <= 0) {
        return {};
    }
    if (policy != ScalePolicy::Fit && policy != ScalePolicy::Fill &&
        policy != ScalePolicy::Stretch && policy != ScalePolicy::IntegerScale &&
        policy != ScalePolicy::Crop && policy != ScalePolicy::NoScale) {
        return {};
    }
    ScaleLayout result{ { 0, 0, source.width, source.height }, {}, bounds };
    if (policy == ScalePolicy::Stretch) {
        result.destination = { 0, 0, bounds.width, bounds.height };
        return result;
    }

    double scale = 1.0;
    if (policy == ScalePolicy::Fit) {
        scale = std::min(static_cast<double>(bounds.width) / source.width,
                         static_cast<double>(bounds.height) / source.height);
    }
    else if (policy == ScalePolicy::Fill) {
        scale = std::max(static_cast<double>(bounds.width) / source.width,
                         static_cast<double>(bounds.height) / source.height);
    }
    else if (policy == ScalePolicy::IntegerScale) {
        scale = std::max(1.0, std::floor(std::min(static_cast<double>(bounds.width) / source.width,
                                                 static_cast<double>(bounds.height) / source.height)));
    }
    else if (policy == ScalePolicy::Crop || policy == ScalePolicy::NoScale) {
        scale = 1.0;
    }

    const double scaledWidthValue = source.width * scale;
    const double scaledHeightValue = source.height * scale;
    if (!std::isfinite(scaledWidthValue) || !std::isfinite(scaledHeightValue) ||
        scaledWidthValue > (std::numeric_limits<int>::max)() ||
        scaledHeightValue > (std::numeric_limits<int>::max)()) {
        return {};
    }
    const int scaledWidth = std::max(1, static_cast<int>(std::lround(scaledWidthValue)));
    const int scaledHeight = std::max(1, static_cast<int>(std::lround(scaledHeightValue)));
    result.destination = {
        (bounds.width - scaledWidth) / 2,
        (bounds.height - scaledHeight) / 2,
        scaledWidth,
        scaledHeight,
    };

    if (policy == ScalePolicy::Crop || policy == ScalePolicy::NoScale) {
        result.destination.width = std::min(source.width, bounds.width);
        result.destination.height = std::min(source.height, bounds.height);
        const bool centered = policy == ScalePolicy::Crop;
        result.destination.x = centered ? (bounds.width - result.destination.width) / 2 : 0;
        result.destination.y = centered ? (bounds.height - result.destination.height) / 2 : 0;
        result.source.x = centered ? (source.width - result.destination.width) / 2 : 0;
        result.source.y = centered ? (source.height - result.destination.height) / 2 : 0;
        result.source.width = result.destination.width;
        result.source.height = result.destination.height;
    }
    return result;
}

}