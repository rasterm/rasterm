/* SPDX-License-Identifier: Apache-2.0 */

#include <color/ColorConverter.hpp>
#include <encoder/SixelEncoder.hpp>
#include <encoder/SixelSimd.hpp>

#include <validation/tests/fixtures/ColorReferenceFixtures.hpp>
#include <validation/tests/support/ColorMetrics.hpp>
#include <validation/tests/support/MicrosoftSixelHarness.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

bool closeChannel(const std::uint8_t actual, const std::uint8_t expected, const int tolerance)
{
    return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <= tolerance;
}

bool closeColor(const rasterm::RgbColor actual, const rasterm::RgbColor expected,
                const int tolerance)
{
    return closeChannel(actual.red, expected.red, tolerance) &&
        closeChannel(actual.green, expected.green, tolerance) &&
        closeChannel(actual.blue, expected.blue, tolerance);
}

std::string registerDefinition(const std::size_t index, const rasterm::RgbColor color,
                               const int offset)
{
    const auto percent = [](const std::uint8_t value) {
        return (static_cast<int>(value) * 100 + 127) / 255;
    };
    return "#" + std::to_string(index + static_cast<std::size_t>(offset)) + ";2;" +
        std::to_string(percent(color.red)) + ";" + std::to_string(percent(color.green)) + ";" +
        std::to_string(percent(color.blue));
}

std::vector<std::string> paletteDefinitions(const std::string& sixel)
{
    std::vector<std::string> result;
    std::size_t cursor = 0;
    while ((cursor = sixel.find('#', cursor)) != std::string::npos) {
        std::size_t end = cursor + 1;
        while (end < sixel.size() && sixel[end] >= '0' && sixel[end] <= '9') ++end;
        if (sixel.compare(end, 3, ";2;") != 0) {
            cursor = end;
            continue;
        }
        end += 3;
        int separators = 0;
        while (end < sixel.size() && separators < 2) {
            if (sixel[end] == ';') ++separators;
            else if (sixel[end] < '0' || sixel[end] > '9') break;
            ++end;
        }
        while (end < sixel.size() && sixel[end] >= '0' && sixel[end] <= '9') ++end;
        result.emplace_back(sixel.substr(cursor, end - cursor));
        cursor = end;
    }
    return result;
}

std::vector<std::uint8_t> makeTemporalFrame(const int base, const int phase = 0)
{
    constexpr int width = 72;
    constexpr int height = 24;
    std::vector<std::uint8_t> pixels(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * width + x) * 3;
            if (y < 6) {
                const int value = std::clamp(base + (x + phase) * 2, 0, 255);
                pixels[offset] = static_cast<std::uint8_t>(value);
                pixels[offset + 1] = static_cast<std::uint8_t>(value);
                pixels[offset + 2] = static_cast<std::uint8_t>(value);
            }
            else if (y < 12) {
                pixels[offset] = static_cast<std::uint8_t>(std::clamp(150 + phase, 0, 255));
                pixels[offset + 1] = static_cast<std::uint8_t>(95 + (x & 7));
                pixels[offset + 2] = static_cast<std::uint8_t>(70 + (x & 3));
            }
            else if (y < 18) {
                const int value = 12 + ((x + phase) & 15);
                pixels[offset] = static_cast<std::uint8_t>(value);
                pixels[offset + 1] = static_cast<std::uint8_t>(value + 2);
                pixels[offset + 2] = static_cast<std::uint8_t>(value + 4);
            }
            else {
                pixels[offset] = static_cast<std::uint8_t>(((x + phase) / 8 & 1) ? 225 : 24);
                pixels[offset + 1] = static_cast<std::uint8_t>((y & 1) ? 90 : 180);
                pixels[offset + 2] = static_cast<std::uint8_t>(220 - pixels[offset]);
            }
        }
    }
    return pixels;
}

}

int main()
{
    rasterm::ColorConverter converter;
    for (const auto& fixture : rasterm::test::fixtures::rgbTransfers) {
        std::array<std::uint8_t, 3> pixel{
            fixture.encoded.red, fixture.encoded.green, fixture.encoded.blue,
        };
        rasterm::FrameView frame{
            pixel.data(), 1, 1, 3, rasterm::PixelFormat::RGB24,
            { .color = fixture.metadata, .sourceColor = fixture.metadata },
        };
        const rasterm::FrameView converted = converter.toSrgb(frame, {});
        if (!closeColor({ converted.data[0], converted.data[1], converted.data[2] },
                        fixture.expectedSrgb, fixture.tolerance)) {
            return 1;
        }
    }

    std::array<rasterm::RgbColor, 256> palette{};
    std::array<std::uint8_t, 256> indices{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        palette[index] = {
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>((index * 73) & 0xff),
            static_cast<std::uint8_t>(255 - index),
        };
        indices[index] = static_cast<std::uint8_t>(index);
    }
    rasterm::VideoSixelEncoder indexedEncoder;
    const rasterm::IndexedFrameView indexed{
        indices.data(), 256, 1, 256, { palette.data(), palette.size() },
    };
    const std::string indexedSixel(indexedEncoder.encodeFrame(indexed));
    for (std::size_t index = 0; index < palette.size(); ++index) {
        if (indexedSixel.find(registerDefinition(index, palette[index], 0)) == std::string::npos) {
            return 2;
        }
    }
    const auto decoded = rasterm::test::MicrosoftSixelHarness{}.parse(indexedSixel);
    if (!decoded.complete || decoded.pixels.size() != palette.size()) return 3;
    double maximumError = 0.0;
    double totalError = 0.0;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const double error = rasterm::test::oklabError(palette[index], decoded.pixels[index]);
        maximumError = std::max(maximumError, error);
        totalError += error;
    }
    if (maximumError > rasterm::test::indexedMaximumOklabError ||
        totalError / palette.size() > rasterm::test::indexedMeanOklabError) {
        return 4;
    }

    constexpr int width = 72;
    constexpr int height = 24;
    auto stable = makeTemporalFrame(30);
    auto motion = makeTemporalFrame(30, 1);
    rasterm::SixelOptions ditherOptions = rasterm::SixelOptions::ForRealtimeVideo();
    ditherOptions.dither = rasterm::DitherMode::OrderedBayer4x4;
    rasterm::VideoSixelEncoder ditherEncoder(ditherOptions);
    const auto view = [](const std::vector<std::uint8_t>& pixels) {
        return rasterm::FrameView{
            pixels.data(), width, height, width * 3, rasterm::PixelFormat::RGB24,
        };
    };
    const std::string stableFirst(ditherEncoder.encodeFrame(view(stable)));
    const std::string motionFrame(ditherEncoder.encodeFrame(view(motion)));
    const std::string stableAgain(ditherEncoder.encodeFrame(view(stable)));
    if (stableFirst != stableAgain || stableFirst == motionFrame) return 5;

    rasterm::SixelOptions adaptiveOptions = rasterm::SixelOptions::ForHighQualityVideo();
    adaptiveOptions.dither = rasterm::DitherMode::OrderedBayer4x4;
    adaptiveOptions.adaptivePaletteLockFrames = 8;
    adaptiveOptions.sceneCutThreshold = 0.30f;
    rasterm::VideoSixelEncoder adaptiveEncoder(adaptiveOptions);
    const std::string paletteA(adaptiveEncoder.encodeFrame(view(stable)));
    const std::string paletteARepeat(adaptiveEncoder.encodeFrame(view(stable)));
    if (paletteA != paletteARepeat) return 6;
    auto smallChange = stable;
    smallChange[0] = static_cast<std::uint8_t>(smallChange[0] + 1);
    const std::string paletteSmallChange(adaptiveEncoder.encodeFrame(view(smallChange)));
    if (paletteDefinitions(paletteA) != paletteDefinitions(paletteSmallChange)) return 7;
    auto hardCut = makeTemporalFrame(210);
    const std::string paletteB(adaptiveEncoder.encodeFrame(view(hardCut)));
    if (paletteDefinitions(paletteA) == paletteDefinitions(paletteB)) return 8;

    auto simdPixels = makeTemporalFrame(20);
    rasterm::SixelOptions simdOptions = rasterm::SixelOptions::ForRealtimeVideo();
    rasterm::VideoSixelEncoder scalarEncoder(simdOptions);
    rasterm::setSixelAvx2ModeForTesting(0);
    if (rasterm::sixelAvx2Enabled()) return 9;
    const std::string scalar(scalarEncoder.encodeFrame(view(simdPixels)));
    rasterm::setSixelAvx2ModeForTesting(-1);
    rasterm::VideoSixelEncoder dispatchedEncoder(simdOptions);
    const std::string dispatched(dispatchedEncoder.encodeFrame(view(simdPixels)));
    if (scalar != dispatched) return 10;

    std::array<std::uint8_t, 12> regionalPixels{
        32, 32, 32, 64, 64, 64, 96, 96, 96, 128, 128, 128,
    };
    rasterm::DamageRect conversionDamage{ 0, 0, 4, 1 };
    rasterm::FrameView regionalFrame{
        regionalPixels.data(), 4, 1, 12, rasterm::PixelFormat::RGB24,
        { .color = { .transfer = rasterm::TransferFunction::Linear },
          .damage = { &conversionDamage, 1, true } },
    };
    rasterm::ColorConverter regionalConverter;
    const auto firstConverted = regionalConverter.toSrgb(regionalFrame, {});
    const std::vector<std::uint8_t> firstPixels(
        firstConverted.data, firstConverted.data + firstConverted.stride);
    regionalPixels[6] = regionalPixels[7] = regionalPixels[8] = 200;
    conversionDamage = { 2, 0, 1, 1 };
    const auto secondConverted = regionalConverter.toSrgb(regionalFrame, {});
    if (!std::equal(firstPixels.begin(), firstPixels.begin() + 6, secondConverted.data) ||
        std::equal(firstPixels.begin() + 6, firstPixels.begin() + 9, secondConverted.data + 6) ||
        !std::equal(firstPixels.begin() + 9, firstPixels.end(), secondConverted.data + 9)) return 11;

    return 0;
}
