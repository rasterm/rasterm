/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Frame.hpp>
#include <rasterm/IndexedFrame.hpp>

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace rasterm {

class OwnedFrame {
public:
    OwnedFrame() = default;
    OwnedFrame(std::vector<std::uint8_t> pixels, const int width, const int height,
               const std::ptrdiff_t stride, const PixelFormat format,
               const FrameMetadata metadata = {}) :
        storage(std::move(pixels)), widthValue(width), heightValue(height),
        strideValue(stride), formatValue(format), metadataValue(metadata)
    {
        ownDamage();
    }

    [[nodiscard]] static OwnedFrame copyOf(const FrameView& frame)
    {
        if (!frame.isValid()) return {};
        const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * bytesPerPixel(frame.format);
        std::vector<std::uint8_t> pixels(rowBytes * frame.height);
        for (int row = 0; row < frame.height; ++row) {
            std::memcpy(pixels.data() + static_cast<std::size_t>(row) * rowBytes,
                        frame.data + static_cast<std::ptrdiff_t>(row) * frame.stride, rowBytes);
        }
        return { std::move(pixels), frame.width, frame.height,
                 static_cast<std::ptrdiff_t>(rowBytes), frame.format, frame.metadata };
    }

    [[nodiscard]] FrameView view() const noexcept
    {
        return { storage.data(), widthValue, heightValue, strideValue, formatValue, metadataValue };
    }
    [[nodiscard]] bool isValid() const noexcept
    {
        const FrameView frame = view();
        if (!frame.isValid()) return false;
        const std::size_t rowBytes = static_cast<std::size_t>(widthValue) * bytesPerPixel(formatValue);
        const std::size_t stride = static_cast<std::size_t>(strideValue);
        return static_cast<std::size_t>(heightValue - 1) <=
                ((std::numeric_limits<std::size_t>::max)() - rowBytes) / stride &&
            storage.size() >= static_cast<std::size_t>(heightValue - 1) * stride + rowBytes;
    }

private:
    void ownDamage()
    {
        const DamageView source = metadataValue.damage;
        if (!source.supplied) {
            if (source.rectangles == nullptr && source.count == 0) metadataValue.damage = {};
            return;
        }
        if (source.count > 0) {
            if (source.rectangles == nullptr) return;
            damage.assign(source.rectangles, source.rectangles + source.count);
        }
        metadataValue.damage = { damage.empty() ? nullptr : damage.data(), damage.size(), true };
    }

    std::vector<std::uint8_t> storage;
    std::vector<DamageRect> damage;
    int widthValue = 0;
    int heightValue = 0;
    std::ptrdiff_t strideValue = 0;
    PixelFormat formatValue = PixelFormat::RGB24;
    FrameMetadata metadataValue{};
};

class OwnedIndexedFrame {
public:
    OwnedIndexedFrame() = default;
    OwnedIndexedFrame(std::vector<std::uint8_t> indices, std::vector<RgbColor> palette,
                      const int width, const int height, const std::ptrdiff_t stride,
                      const FrameMetadata metadata = {}) :
        storage(std::move(indices)), colors(std::move(palette)), widthValue(width),
        heightValue(height), strideValue(stride), metadataValue(metadata)
    {
        ownDamage();
    }

    [[nodiscard]] IndexedFrameView view() const noexcept
    {
        return { storage.data(), widthValue, heightValue, strideValue,
                 { colors.data(), colors.size() }, metadataValue };
    }
    [[nodiscard]] bool isValid() const noexcept
    {
        const IndexedFrameView frame = view();
        if (!frame.hasValidIndices()) return false;
        const std::size_t stride = static_cast<std::size_t>(strideValue);
        return static_cast<std::size_t>(heightValue - 1) <=
                ((std::numeric_limits<std::size_t>::max)() - static_cast<std::size_t>(widthValue)) /
                    stride &&
            storage.size() >= static_cast<std::size_t>(heightValue - 1) * stride +
                static_cast<std::size_t>(widthValue);
    }

private:
    void ownDamage()
    {
        const DamageView source = metadataValue.damage;
        if (!source.supplied) {
            if (source.rectangles == nullptr && source.count == 0) metadataValue.damage = {};
            return;
        }
        if (source.count > 0) {
            if (source.rectangles == nullptr) return;
            damage.assign(source.rectangles, source.rectangles + source.count);
        }
        metadataValue.damage = { damage.empty() ? nullptr : damage.data(), damage.size(), true };
    }

    std::vector<std::uint8_t> storage;
    std::vector<RgbColor> colors;
    std::vector<DamageRect> damage;
    int widthValue = 0;
    int heightValue = 0;
    std::ptrdiff_t strideValue = 0;
    FrameMetadata metadataValue{};
};

struct SharedFrameView {
    FrameView frame{};
    std::shared_ptr<const void> lifetime;
    [[nodiscard]] bool isValid() const noexcept { return lifetime && frame.isValid(); }
};

struct SharedIndexedFrameView {
    IndexedFrameView frame{};
    std::shared_ptr<const void> lifetime;
    [[nodiscard]] bool isValid() const noexcept { return lifetime && frame.hasValidIndices(); }
};

}