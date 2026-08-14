/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstdint>

namespace {

std::array<std::uint8_t, 3> scalePixel(const std::uint8_t* pixels,
                                       const rasterm::PixelFormat format)
{
    const auto source = rasterm::FrameView::tightlyPacked(pixels, 1, 1, format);
    const rasterm::OwnedFrame result = rasterm::scaleFrame(source, { 1, 1 });
    if (!result.isValid()) return {};
    const rasterm::FrameView view = result.view();
    return { view.data[0], view.data[1], view.data[2] };
}

}

int main()
{
    const std::array<std::uint8_t, 6> endpoints{ 255, 0, 0, 0, 0, 255 };
    const auto source = rasterm::FrameView::tightlyPacked(
        endpoints.data(), 2, 1, rasterm::PixelFormat::RGB24);
    if (!source.isValid() || source.stride != 6 ||
        rasterm::FrameView::tightlyPacked(nullptr, 2, 1,
                                          rasterm::PixelFormat::RGB24).isValid()) {
        return 1;
    }

    const std::array<rasterm::RgbColor, 2> palette{{ { 0, 0, 0 }, { 255, 255, 255 } }};
    const std::array<std::uint8_t, 2> indices{ 0, 1 };
    const auto indexed = rasterm::IndexedFrameView::tightlyPacked(
        indices.data(), 2, 1, rasterm::PaletteView::from(palette));
    if (!indexed.hasValidIndices() || indexed.stride != 2) return 2;

    rasterm::ImageOptions options;
    options.scaling.policy = rasterm::ScalePolicy::Stretch;
    options.scaling.filter = rasterm::ScaleFilter::Nearest;
    const rasterm::OwnedFrame scaled = rasterm::scaleFrame(source, { 4, 2 }, options);
    const rasterm::FrameView scaledView = scaled.view();
    if (!scaled.isValid() || scaledView.width != 4 || scaledView.height != 2 ||
        scaledView.stride != 12 || scaledView.format != rasterm::PixelFormat::RGB24 ||
        scaledView.data[0] != 255 || scaledView.data[3] != 255 ||
        scaledView.data[6] != 0 || scaledView.data[8] != 255) {
        return 3;
    }

    const std::array<std::uint8_t, 3> rgb{ 1, 2, 3 };
    const std::array<std::uint8_t, 3> bgr{ 3, 2, 1 };
    const std::array<std::uint8_t, 4> rgba{ 1, 2, 3, 99 };
    const std::array<std::uint8_t, 4> bgra{ 3, 2, 1, 99 };
    const std::array<std::uint8_t, 2> rgb565{ 0x00, 0xf8 };
    const std::array<std::uint8_t, 2> xrgb1555{ 0x00, 0x7c };
    const std::array<std::uint8_t, 2> rgba4444{ 0x0f, 0xf0 };
    if (scalePixel(rgb.data(), rasterm::PixelFormat::RGB24) != rgb ||
        scalePixel(bgr.data(), rasterm::PixelFormat::BGR24) != rgb ||
        scalePixel(rgba.data(), rasterm::PixelFormat::RGBA32) != rgb ||
        scalePixel(bgra.data(), rasterm::PixelFormat::BGRA32) != rgb ||
        scalePixel(rgb565.data(), rasterm::PixelFormat::RGB565) !=
            std::array<std::uint8_t, 3>{ 255, 0, 0 } ||
        scalePixel(xrgb1555.data(), rasterm::PixelFormat::XRGB1555) !=
            std::array<std::uint8_t, 3>{ 255, 0, 0 } ||
        scalePixel(rgba4444.data(), rasterm::PixelFormat::RGBA4444) !=
            std::array<std::uint8_t, 3>{ 255, 0, 0 }) {
        return 4;
    }

    const std::array<std::uint8_t, 12> corners{
        0, 0, 0, 255, 0, 0,
        0, 255, 0, 0, 0, 255,
    };
    options.scaling.filter = rasterm::ScaleFilter::Area;
    const auto areaSource = rasterm::FrameView::tightlyPacked(
        corners.data(), 2, 2, rasterm::PixelFormat::RGB24);
    const rasterm::OwnedFrame averaged = rasterm::scaleFrame(areaSource, { 1, 1 }, options);
    const rasterm::FrameView averagedView = averaged.view();
    if (!averaged.isValid() || averagedView.data[0] != 64 || averagedView.data[1] != 64 ||
        averagedView.data[2] != 64) {
        return 5;
    }

    options.scaling.policy = rasterm::ScalePolicy::Fit;
    options.scaling.filter = rasterm::ScaleFilter::Nearest;
    options.scaling.background = { 7, 8, 9 };
    const rasterm::OwnedFrame letterboxed = rasterm::scaleFrame(source, { 4, 4 }, options);
    const rasterm::FrameView letterboxedView = letterboxed.view();
    if (!letterboxed.isValid() || letterboxedView.data[0] != 7 ||
        letterboxedView.data[1] != 8 || letterboxedView.data[2] != 9 ||
        letterboxedView.data[12] != 255) {
        return 6;
    }

    const rasterm::DamageRect damage{ 0, 0, 1, 1 };
    rasterm::FrameMetadata metadata;
    metadata.damage = { &damage, 1, true };
    const auto damaged = rasterm::FrameView::tightlyPacked(
        endpoints.data(), 2, 1, rasterm::PixelFormat::RGB24, metadata);
    const rasterm::OwnedFrame damageScaled = rasterm::scaleFrame(damaged, { 2, 1 });
    if (!damageScaled.isValid() || damageScaled.view().metadata.damage.supplied) return 7;

    options.contrast = -1.0f;
    if (rasterm::scaleFrame(source, { 4, 2 }, options).isValid()) return 8;

    return 0;
}
