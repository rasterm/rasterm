/* SPDX-License-Identifier: Apache-2.0 */

#include <color/ColorConverter.hpp>

#include <encoder/SixelEncoder.hpp>

#include <rasterm/rasterm.hpp>

#include <array>
#include <cmath>
#include <string>

int main()
{
    const auto fit = rasterm::calculateScaleLayout({ 1920, 1080 }, { 1000, 1000 },
                                                    rasterm::ScalePolicy::Fit);
    const auto fill = rasterm::calculateScaleLayout({ 1920, 1080 }, { 1000, 1000 },
                                                     rasterm::ScalePolicy::Fill);
    const auto stretch = rasterm::calculateScaleLayout({ 320, 240 }, { 800, 600 },
                                                        rasterm::ScalePolicy::Stretch);
    const auto integer = rasterm::calculateScaleLayout({ 256, 240 }, { 1024, 960 },
                                                        rasterm::ScalePolicy::IntegerScale);
    const auto crop = rasterm::calculateScaleLayout({ 1920, 1080 }, { 640, 480 },
                                                     rasterm::ScalePolicy::Crop);
    if (fit.destination.width != 1000 || fit.destination.height != 563 ||
        fill.destination.width != 1778 || fill.destination.height != 1000 ||
        stretch.destination.width != 800 || stretch.destination.height != 600 ||
        integer.destination.width != 1024 || integer.destination.height != 960 ||
        crop.source.width != 640 || crop.source.height != 480) return 2;

    std::array<std::uint8_t, 3> linearPixel{ 128, 64, 32 };
    rasterm::FrameView linearFrame{
        linearPixel.data(), 1, 1, 3, rasterm::PixelFormat::RGB24,
        { .color = { .transfer = rasterm::TransferFunction::Linear } },
    };
    rasterm::ColorConverter converter;
    const rasterm::FrameView srgb = converter.toSrgb(linearFrame, {});
    if (srgb.data[0] <= linearPixel[0] || srgb.metadata.color.transfer != rasterm::TransferFunction::Srgb) {
        return 3;
    }

    std::array<std::uint8_t, 64 * 6 * 3> gradient{};
    for (std::size_t index = 0; index < gradient.size(); ++index) {
        gradient[index] = static_cast<std::uint8_t>((index * 17) & 0xff);
    }
    rasterm::SixelOptions options = rasterm::SixelOptions::ForRealtimeVideo();
    options.dither = rasterm::DitherMode::OrderedBayer4x4;
    rasterm::VideoSixelEncoder encoder(options);
    const rasterm::FrameView frame{ gradient.data(), 64, 6, 64 * 3, rasterm::PixelFormat::RGB24 };
    const std::string first(encoder.encodeFrame(frame));
    const std::string second(encoder.encodeFrame(frame));
    if (first != second) return 4;

    return 0;
}