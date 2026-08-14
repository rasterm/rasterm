/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/Scaling.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace rasterm {
namespace {

using Sample = std::array<double, 3>;

std::uint8_t clampChannel(const double value) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

Sample readPixel(const FrameView& frame, const int x, const int y) noexcept
{
    const auto* pixel = frame.data + static_cast<std::ptrdiff_t>(y) * frame.stride +
        static_cast<std::ptrdiff_t>(x) * bytesPerPixel(frame.format);
    if (frame.format == PixelFormat::RGB24 || frame.format == PixelFormat::RGBA32) {
        return { static_cast<double>(pixel[0]), static_cast<double>(pixel[1]),
                 static_cast<double>(pixel[2]) };
    }
    if (frame.format == PixelFormat::BGR24 || frame.format == PixelFormat::BGRA32) {
        return { static_cast<double>(pixel[2]), static_cast<double>(pixel[1]),
                 static_cast<double>(pixel[0]) };
    }

    const auto packed = static_cast<std::uint16_t>(pixel[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(pixel[1]) << 8U);
    const auto expand5 = [](const std::uint16_t value) {
        return static_cast<double>((value << 3U) | (value >> 2U));
    };
    if (frame.format == PixelFormat::RGB565) {
        const auto expand6 = [](const std::uint16_t value) {
            return static_cast<double>((value << 2U) | (value >> 4U));
        };
        return { expand5((packed >> 11U) & 0x1fU), expand6((packed >> 5U) & 0x3fU),
                 expand5(packed & 0x1fU) };
    }
    if (frame.format == PixelFormat::XRGB1555) {
        return { expand5((packed >> 10U) & 0x1fU), expand5((packed >> 5U) & 0x1fU),
                 expand5(packed & 0x1fU) };
    }
    const auto expand4 = [](const std::uint16_t value) {
        return static_cast<double>((value << 4U) | value);
    };
    return { expand4((packed >> 12U) & 0x0fU), expand4((packed >> 8U) & 0x0fU),
             expand4((packed >> 4U) & 0x0fU) };
}

Sample nearest(const FrameView& frame, const Rect source, const double x, const double y) noexcept
{
    const int sampleX = std::clamp(static_cast<int>(std::floor(x + 0.5)), source.x,
                                   source.x + source.width - 1);
    const int sampleY = std::clamp(static_cast<int>(std::floor(y + 0.5)), source.y,
                                   source.y + source.height - 1);
    return readPixel(frame, sampleX, sampleY);
}

Sample linear(const FrameView& frame, const Rect source, const double x, const double y) noexcept
{
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), source.x,
                              source.x + source.width - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), source.y,
                              source.y + source.height - 1);
    const int x1 = std::min(x0 + 1, source.x + source.width - 1);
    const int y1 = std::min(y0 + 1, source.y + source.height - 1);
    const double fx = std::clamp(x - std::floor(x), 0.0, 1.0);
    const double fy = std::clamp(y - std::floor(y), 0.0, 1.0);
    const Sample topLeft = readPixel(frame, x0, y0);
    const Sample topRight = readPixel(frame, x1, y0);
    const Sample bottomLeft = readPixel(frame, x0, y1);
    const Sample bottomRight = readPixel(frame, x1, y1);
    Sample result{};
    for (int channel = 0; channel < 3; ++channel) {
        const double top = topLeft[channel] + (topRight[channel] - topLeft[channel]) * fx;
        const double bottom = bottomLeft[channel] +
            (bottomRight[channel] - bottomLeft[channel]) * fx;
        result[channel] = top + (bottom - top) * fy;
    }
    return result;
}

double cubicWeight(const double value) noexcept
{
    const double distance = std::abs(value);
    if (distance <= 1.0) return (1.5 * distance - 2.5) * distance * distance + 1.0;
    if (distance < 2.0) return ((-0.5 * distance + 2.5) * distance - 4.0) * distance + 2.0;
    return 0.0;
}

Sample cubic(const FrameView& frame, const Rect source, const double x, const double y) noexcept
{
    const int baseX = static_cast<int>(std::floor(x));
    const int baseY = static_cast<int>(std::floor(y));
    Sample result{};
    double totalWeight = 0.0;
    for (int offsetY = -1; offsetY <= 2; ++offsetY) {
        const int sampleY = std::clamp(baseY + offsetY, source.y,
                                       source.y + source.height - 1);
        const double weightY = cubicWeight(y - (baseY + offsetY));
        for (int offsetX = -1; offsetX <= 2; ++offsetX) {
            const int sampleX = std::clamp(baseX + offsetX, source.x,
                                           source.x + source.width - 1);
            const double weight = weightY * cubicWeight(x - (baseX + offsetX));
            const Sample sample = readPixel(frame, sampleX, sampleY);
            for (int channel = 0; channel < 3; ++channel) {
                result[channel] += sample[channel] * weight;
            }
            totalWeight += weight;
        }
    }
    if (totalWeight != 0.0) {
        for (double& channel : result) channel /= totalWeight;
    }
    return result;
}

Sample area(const FrameView& frame, const Rect source, const double left, const double top,
            const double right, const double bottom) noexcept
{
    Sample result{};
    double totalWeight = 0.0;
    const int firstX = std::max(source.x, static_cast<int>(std::floor(left)));
    const int lastX = std::min(source.x + source.width - 1,
                               static_cast<int>(std::ceil(right)) - 1);
    const int firstY = std::max(source.y, static_cast<int>(std::floor(top)));
    const int lastY = std::min(source.y + source.height - 1,
                               static_cast<int>(std::ceil(bottom)) - 1);
    for (int y = firstY; y <= lastY; ++y) {
        const double vertical = std::max(0.0, std::min(bottom, y + 1.0) - std::max(top, double(y)));
        for (int x = firstX; x <= lastX; ++x) {
            const double horizontal = std::max(0.0,
                std::min(right, x + 1.0) - std::max(left, double(x)));
            const double weight = horizontal * vertical;
            const Sample sample = readPixel(frame, x, y);
            for (int channel = 0; channel < 3; ++channel) result[channel] += sample[channel] * weight;
            totalWeight += weight;
        }
    }
    if (totalWeight != 0.0) {
        for (double& channel : result) channel /= totalWeight;
    }
    return result;
}

bool validOptions(const ImageOptions& options) noexcept
{
    return (options.scaling.policy == ScalePolicy::Fit ||
            options.scaling.policy == ScalePolicy::Fill ||
            options.scaling.policy == ScalePolicy::Stretch ||
            options.scaling.policy == ScalePolicy::IntegerScale ||
            options.scaling.policy == ScalePolicy::Crop ||
            options.scaling.policy == ScalePolicy::NoScale) &&
        (options.scaling.filter == ScaleFilter::Nearest ||
         options.scaling.filter == ScaleFilter::Linear ||
         options.scaling.filter == ScaleFilter::Cubic ||
         options.scaling.filter == ScaleFilter::Area) &&
        std::isfinite(options.sharpening) && options.sharpening >= 0.0f &&
        options.sharpening <= 1.0f && std::isfinite(options.contrast) &&
        options.contrast >= 0.0f && options.contrast <= 4.0f;
}

void applyAdjustments(std::vector<std::uint8_t>& pixels, const int width, const int height,
                      const float contrast, const float sharpening)
{
    if (contrast != 1.0f) {
        for (std::uint8_t& channel : pixels) {
            channel = clampChannel((channel - 127.5) * contrast + 127.5);
        }
    }
    if (sharpening == 0.0f || width < 3 || height < 3) return;
    const std::vector<std::uint8_t> source = pixels;
    for (int y = 1; y + 1 < height; ++y) {
        for (int x = 1; x + 1 < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3 + channel;
                const double edge = 4.0 * source[index] - source[index - 3] - source[index + 3] -
                    source[index - static_cast<std::size_t>(width) * 3] -
                    source[index + static_cast<std::size_t>(width) * 3];
                pixels[index] = clampChannel(source[index] + sharpening * edge);
            }
        }
    }
}

}

OwnedFrame scaleFrame(const FrameView& frame, const Extent bounds, const ImageOptions& options)
{
    if (!frame.isValid() || bounds.width <= 0 || bounds.height <= 0 || !validOptions(options) ||
        static_cast<std::size_t>(bounds.width) >
            (std::numeric_limits<std::size_t>::max)() / 3U / static_cast<std::size_t>(bounds.height)) {
        return {};
    }
    const ScaleLayout layout = calculateScaleLayout({ frame.width, frame.height }, bounds,
                                                     options.scaling.policy);
    if (layout.destination.width <= 0 || layout.destination.height <= 0 ||
        layout.source.width <= 0 || layout.source.height <= 0) {
        return {};
    }

    const std::size_t rowBytes = static_cast<std::size_t>(bounds.width) * 3;
    std::vector<std::uint8_t> pixels(rowBytes * bounds.height);
    for (std::size_t offset = 0; offset < pixels.size(); offset += 3) {
        pixels[offset] = options.scaling.background.red;
        pixels[offset + 1] = options.scaling.background.green;
        pixels[offset + 2] = options.scaling.background.blue;
    }

    const int firstX = std::max(0, layout.destination.x);
    const int lastX = std::min(bounds.width, layout.destination.x + layout.destination.width);
    const int firstY = std::max(0, layout.destination.y);
    const int lastY = std::min(bounds.height, layout.destination.y + layout.destination.height);
    const double scaleX = static_cast<double>(layout.source.width) / layout.destination.width;
    const double scaleY = static_cast<double>(layout.source.height) / layout.destination.height;
    for (int y = firstY; y < lastY; ++y) {
        for (int x = firstX; x < lastX; ++x) {
            const double sourceLeft = layout.source.x + (x - layout.destination.x) * scaleX;
            const double sourceTop = layout.source.y + (y - layout.destination.y) * scaleY;
            const double sourceX = sourceLeft + scaleX * 0.5 - 0.5;
            const double sourceY = sourceTop + scaleY * 0.5 - 0.5;
            Sample sample{};
            if (options.scaling.filter == ScaleFilter::Nearest) {
                sample = nearest(frame, layout.source, sourceX, sourceY);
            }
            else if (options.scaling.filter == ScaleFilter::Cubic) {
                sample = cubic(frame, layout.source, sourceX, sourceY);
            }
            else if (options.scaling.filter == ScaleFilter::Area &&
                     (scaleX > 1.0 || scaleY > 1.0)) {
                sample = area(frame, layout.source, sourceLeft, sourceTop,
                              sourceLeft + scaleX, sourceTop + scaleY);
            }
            else {
                sample = linear(frame, layout.source, sourceX, sourceY);
            }
            const std::size_t destination = static_cast<std::size_t>(y) * rowBytes + x * 3;
            pixels[destination] = clampChannel(sample[0]);
            pixels[destination + 1] = clampChannel(sample[1]);
            pixels[destination + 2] = clampChannel(sample[2]);
        }
    }
    applyAdjustments(pixels, bounds.width, bounds.height, options.contrast, options.sharpening);

    FrameMetadata metadata = frame.metadata;
    metadata.damage = {};
    return { std::move(pixels), bounds.width, bounds.height,
             static_cast<std::ptrdiff_t>(rowBytes), PixelFormat::RGB24, metadata };
}

}
