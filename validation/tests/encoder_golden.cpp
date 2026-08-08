/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelEncoder.hpp>

#include <rasterm/rasterm.hpp>

#include <validation/tests/fixtures/SixelFixtures.hpp>
#include <validation/tests/support/MicrosoftSixelHarness.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Oklab {
    double l;
    double a;
    double b;
};

double linear(const double channel)
{
    const double normalized = channel / 255.0;
    return normalized <= 0.04045 ? normalized / 12.92
                                 : std::pow((normalized + 0.055) / 1.055, 2.4);
}

Oklab toOklab(const rasterm::RgbColor color)
{
    const double red = linear(color.red);
    const double green = linear(color.green);
    const double blue = linear(color.blue);
    const double l = std::cbrt(0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue);
    const double m = std::cbrt(0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue);
    const double s = std::cbrt(0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue);
    return {
        0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
    };
}

double perceptualError(const rasterm::RgbColor left, const rasterm::RgbColor right)
{
    const Oklab a = toOklab(left);
    const Oklab b = toOklab(right);
    return std::sqrt((a.l - b.l) * (a.l - b.l) +
                     (a.a - b.a) * (a.a - b.a) +
                     (a.b - b.b) * (a.b - b.b));
}

rasterm::RgbColor sixelQuantized(const rasterm::RgbColor color)
{
    const auto channel = [](const std::uint8_t value) {
        const int percent = (static_cast<int>(value) * 100 + 127) / 255;
        return static_cast<std::uint8_t>((percent * 255 + 50) / 100);
    };
    return { channel(color.red), channel(color.green), channel(color.blue) };
}

bool exactRoundTrip(rasterm::VideoSixelEncoder& encoder,
                    const rasterm::IndexedFrameView frame)
{
    const auto decoded = rasterm::test::MicrosoftSixelHarness{}.parse(encoder.encodeFrame(frame));
    if (!decoded.complete || decoded.width != frame.width || decoded.height != frame.height ||
        decoded.pixels.size() != static_cast<std::size_t>(frame.width * frame.height)) {
        return false;
    }
    for (int row = 0; row < frame.height; ++row) {
        for (int column = 0; column < frame.width; ++column) {
            const auto index = frame.indices[static_cast<std::ptrdiff_t>(row) * frame.stride + column];
            if (decoded.pixels[static_cast<std::size_t>(row) * frame.width + column] !=
                sixelQuantized(frame.palette.colors[index])) {
                return false;
            }
        }
    }
    return true;
}

constexpr std::array<rasterm::RgbColor, 64> nesPalette{{
    {84,84,84},{0,30,116},{8,16,144},{48,0,136},{68,0,100},{92,0,48},{84,4,0},{60,24,0},
    {32,42,0},{8,58,0},{0,64,0},{0,60,0},{0,50,60},{0,0,0},{0,0,0},{0,0,0},
    {152,150,152},{8,76,196},{48,50,236},{92,30,228},{136,20,176},{160,20,100},{152,34,32},{120,60,0},
    {84,90,0},{40,114,0},{8,124,0},{0,118,40},{0,102,120},{0,0,0},{0,0,0},{0,0,0},
    {236,238,236},{76,154,236},{120,124,236},{176,98,236},{228,84,236},{236,88,180},{236,106,100},{212,136,32},
    {160,170,0},{116,196,0},{76,208,32},{56,204,108},{56,180,204},{60,60,60},{0,0,0},{0,0,0},
    {236,238,236},{168,204,236},{188,188,236},{212,178,236},{236,174,236},{236,174,212},{236,180,176},{228,196,144},
    {204,210,120},{180,222,120},{168,226,144},{152,226,180},{160,214,228},{160,162,160},{0,0,0},{0,0,0},
}};

}

int main()
{
    rasterm::VideoSixelEncoder encoder;
    rasterm::test::MicrosoftSixelHarness parser;

    std::uint8_t redIndex = 0;
    rasterm::RgbColor red{ 255, 0, 0 };
    rasterm::IndexedFrameView one{
        .indices = &redIndex, .width = 1, .height = 1, .stride = 1,
        .palette = { &red, 1 },
    };
    if (encoder.encodeFrame(one) != rasterm::test::fixtures::solidRed1x1) {
        return 1;
    }

    std::array<std::uint8_t, 3> repeated{};
    one.indices = repeated.data();
    one.width = 2;
    one.stride = 2;
    if (encoder.encodeFrame(one).find(rasterm::test::fixtures::solidRed2x1Band) == std::string_view::npos) {
        return 2;
    }
    one.width = 3;
    one.stride = 3;
    if (encoder.encodeFrame(one).find(rasterm::test::fixtures::solidRed3x1Band) == std::string_view::npos) {
        return 3;
    }

    constexpr int oddWidth = 7;
    constexpr int oddHeight = 7;
    std::array<rasterm::RgbColor, 2> colors{{ { 255, 0, 0 }, { 0, 0, 255 } }};
    std::array<std::uint8_t, oddWidth * oddHeight> odd{};
    for (int y = 0; y < oddHeight; ++y) {
        for (int x = 0; x < oddWidth; ++x) {
            odd[y * oddWidth + x] = static_cast<std::uint8_t>((x + y) & 1);
        }
    }
    const rasterm::IndexedFrameView oddFrame{
        odd.data(), oddWidth, oddHeight, oddWidth, { colors.data(), colors.size() }
    };
    const auto decodedOdd = parser.parse(encoder.encodeFrame(oddFrame));
    if (!decodedOdd.complete || decodedOdd.width != oddWidth || decodedOdd.height != oddHeight ||
        decodedOdd.pixels.front().red < 250 || decodedOdd.pixels.back().red < 250) {
        return 4;
    }

    std::array<rasterm::RgbColor, 16> gradientPalette{};
    std::array<std::uint8_t, 16 * 6> gradientIndices{};
    for (std::size_t index = 0; index < gradientPalette.size(); ++index) {
        const auto value = static_cast<std::uint8_t>(index * 17);
        gradientPalette[index] = { value, value, value };
    }
    for (std::size_t index = 0; index < gradientIndices.size(); ++index) {
        gradientIndices[index] = static_cast<std::uint8_t>(index % gradientPalette.size());
    }
    const rasterm::IndexedFrameView gradientFrame{
        gradientIndices.data(), 16, 6, 16,
        { gradientPalette.data(), gradientPalette.size() }
    };
    const auto decodedGradient = parser.parse(encoder.encodeFrame(gradientFrame));
    if (!decodedGradient.complete || decodedGradient.pixels.front().red != 0 ||
        decodedGradient.pixels[15].red < 250) {
        return 5;
    }

    std::array<rasterm::RgbColor, 256> maximumPalette{};
    for (std::size_t index = 0; index < maximumPalette.size(); ++index) {
        maximumPalette[index] = {
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255 - index),
            static_cast<std::uint8_t>((index * 37) & 0xFF),
        };
    }
    std::uint8_t maximumIndex = 255;
    const rasterm::IndexedFrameView maximumFrame{
        &maximumIndex, 1, 1, 1, { maximumPalette.data(), maximumPalette.size() }
    };
    if (!parser.parse(encoder.encodeFrame(maximumFrame)).complete || encoder.lastColorCount() != 256) {
        return 6;
    }
    if (rasterm::PaletteView{ maximumPalette.data(), 257 }.isValid()) {
        return 7;
    }

    for (const std::size_t paletteSize : { 1u, 2u, 255u, 256u }) {
        std::array<rasterm::RgbColor, 256> boundaryPalette{};
        std::array<std::uint8_t, 256> boundaryIndices{};
        for (std::size_t index = 0; index < paletteSize; ++index) {
            boundaryPalette[index] = {
                static_cast<std::uint8_t>(index),
                static_cast<std::uint8_t>((index * 71) & 0xFF),
                static_cast<std::uint8_t>(255 - index),
            };
            boundaryIndices[index] = static_cast<std::uint8_t>(index);
        }
        const rasterm::IndexedFrameView boundaryFrame{
            boundaryIndices.data(), static_cast<int>(paletteSize), 1,
            static_cast<std::ptrdiff_t>(paletteSize),
            { boundaryPalette.data(), paletteSize },
        };
        if (!exactRoundTrip(encoder, boundaryFrame) ||
            encoder.lastColorCount() != static_cast<int>(paletteSize)) return 13;
    }

    for (const int height : { 1, 5, 6, 7, 11, 12, 13 }) {
        std::array<std::uint8_t, 4 * 13> boundaryIndices{};
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < 4; ++column) {
                boundaryIndices[static_cast<std::size_t>(row) * 4 + column] =
                    static_cast<std::uint8_t>((row + column) & 1);
            }
        }
        const rasterm::IndexedFrameView boundaryFrame{
            boundaryIndices.data(), 4, height, 4, { colors.data(), colors.size() },
        };
        if (!exactRoundTrip(encoder, boundaryFrame)) return 14;
    }

    for (const int width : { 1, 2, 3, 255 }) {
        std::array<std::uint8_t, 255> run{};
        const rasterm::IndexedFrameView runFrame{
            run.data(), width, 1, width, { &red, 1 },
        };
        const std::string encoded(encoder.encodeFrame(runFrame));
        if (!exactRoundTrip(encoder, runFrame)) return 15;
        const bool hasRepeat = encoded.find("!" + std::to_string(width) + "@") != std::string::npos;
        if (hasRepeat != (width >= 3)) return 16;
    }

    std::array<std::uint8_t, 64> nesIndices{};
    for (std::size_t index = 0; index < nesIndices.size(); ++index) {
        nesIndices[index] = static_cast<std::uint8_t>(index);
    }
    const rasterm::IndexedFrameView nesFrame{
        nesIndices.data(), 8, 8, 8, { nesPalette.data(), nesPalette.size() }
    };
    const auto decodedNes = parser.parse(encoder.encodeFrame(nesFrame));
    if (!decodedNes.complete || decodedNes.pixels.size() != nesIndices.size()) {
        return 8;
    }
    double maximumError = 0.0;
    double totalError = 0.0;
    for (std::size_t index = 0; index < nesPalette.size(); ++index) {
        const double error = perceptualError(nesPalette[index], decodedNes.pixels[index]);
        maximumError = std::max(maximumError, error);
        totalError += error;
    }
    if (maximumError > 0.015 || totalError / nesPalette.size() > 0.005) {
        return 9;
    }

    constexpr int simdWidth = 64;
    constexpr int simdHeight = 6;
    std::array<std::uint8_t, simdWidth * simdHeight * 3> rgb{};
    std::array<std::uint8_t, simdWidth * simdHeight * 3> bgr{};
    for (int pixel = 0; pixel < simdWidth * simdHeight; ++pixel) {
        rgb[pixel * 3] = static_cast<std::uint8_t>((pixel * 17) & 0xFF);
        rgb[pixel * 3 + 1] = static_cast<std::uint8_t>((pixel * 31) & 0xFF);
        rgb[pixel * 3 + 2] = static_cast<std::uint8_t>((pixel * 47) & 0xFF);
        bgr[pixel * 3] = rgb[pixel * 3 + 2];
        bgr[pixel * 3 + 1] = rgb[pixel * 3 + 1];
        bgr[pixel * 3 + 2] = rgb[pixel * 3];
    }
    const rasterm::FrameView rgbFrame{
        rgb.data(), simdWidth, simdHeight, simdWidth * 3, rasterm::PixelFormat::RGB24
    };
    const rasterm::FrameView bgrFrame{
        bgr.data(), simdWidth, simdHeight, simdWidth * 3, rasterm::PixelFormat::BGR24
    };
    const std::string rgbSixel(encoder.encodeFrame(rgbFrame));
    const std::string bgrSixel(encoder.encodeFrame(bgrFrame));
    if (rgbSixel != bgrSixel) {
        return 10;
    }

    constexpr std::array<rasterm::RgbColor, 24> colorChecker{{
        {115,82,68},{194,150,130},{98,122,157},{87,108,67},{133,128,177},{103,189,170},
        {214,126,44},{80,91,166},{193,90,99},{94,60,108},{157,188,64},{224,163,46},
        {56,61,150},{70,148,73},{175,54,60},{231,199,31},{187,86,149},{8,133,161},
        {243,243,242},{200,200,200},{160,160,160},{122,122,121},{85,85,85},{52,52,52},
    }};
    std::array<std::uint8_t, colorChecker.size() * 3> chartPixels{};
    for (std::size_t index = 0; index < colorChecker.size(); ++index) {
        chartPixels[index * 3] = colorChecker[index].red;
        chartPixels[index * 3 + 1] = colorChecker[index].green;
        chartPixels[index * 3 + 2] = colorChecker[index].blue;
    }
    const rasterm::FrameView chartFrame{
        chartPixels.data(), 6, 4, 18, rasterm::PixelFormat::RGB24
    };
    const auto decodedChart = parser.parse(encoder.encodeFrame(chartFrame));
    if (!decodedChart.complete || decodedChart.pixels.size() != colorChecker.size()) return 11;
    double chartMaximum = 0.0;
    double chartTotal = 0.0;
    for (std::size_t index = 0; index < colorChecker.size(); ++index) {
        const double error = perceptualError(colorChecker[index], decodedChart.pixels[index]);
        chartMaximum = std::max(chartMaximum, error);
        chartTotal += error;
    }
    if (chartMaximum > 0.16 || chartTotal / colorChecker.size() > 0.075) {
        return 12;
    }
    return 0;
}