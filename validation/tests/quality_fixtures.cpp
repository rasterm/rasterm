/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelEncoder.hpp>

#include <validation/tests/fixtures/QualityFixtures.hpp>
#include <validation/tests/support/MicrosoftSixelHarness.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main()
{
    rasterm::test::MicrosoftSixelHarness decoder;
    int failure = 1;
    for (const auto& fixture : rasterm::test::makeQualityFixtures()) {
        const auto frame = fixture.frame();
        rasterm::SixelEncoder first(rasterm::SixelOptions::forImage());
        rasterm::SixelEncoder second(rasterm::SixelOptions::forImage());
        first.prepareFrame(frame);
        second.prepareFrame(frame);
        const std::string firstOutput(first.encodeFrame(frame));
        const std::string secondOutput(second.encodeFrame(frame));
        if (firstOutput.empty() || firstOutput != secondOutput) return failure;

        std::vector<std::uint8_t> bgrPixels = fixture.pixels;
        for (std::size_t offset = 0; offset < bgrPixels.size(); offset += 3) {
            std::swap(bgrPixels[offset], bgrPixels[offset + 2]);
        }
        const rasterm::FrameView bgrFrame{
            bgrPixels.data(), fixture.width, fixture.height, fixture.width * 3,
            rasterm::PixelFormat::BGR24,
        };
        rasterm::SixelEncoder bgrEncoder(rasterm::SixelOptions::forImage());
        bgrEncoder.prepareFrame(bgrFrame);
        if (bgrEncoder.encodeFrame(bgrFrame) != firstOutput) {
            std::cerr << fixture.name << ": RGB24 and BGR24 output differs\n";
            return failure;
        }

        const auto decoded = decoder.parse(firstOutput);
        if (!decoded.complete || decoded.width != fixture.width ||
            decoded.height != fixture.height || decoded.pixels.size() !=
                static_cast<std::size_t>(fixture.width) * fixture.height) {
            return failure;
        }
        double channelError = 0.0;
        double redBias = 0.0;
        double greenBias = 0.0;
        double blueBias = 0.0;
        for (std::size_t pixel = 0; pixel < decoded.pixels.size(); ++pixel) {
            const int redDifference = static_cast<int>(decoded.pixels[pixel].red) -
                fixture.pixels[pixel * 3];
            const int greenDifference = static_cast<int>(decoded.pixels[pixel].green) -
                fixture.pixels[pixel * 3 + 1];
            const int blueDifference = static_cast<int>(decoded.pixels[pixel].blue) -
                fixture.pixels[pixel * 3 + 2];
            channelError += std::abs(redDifference) + std::abs(greenDifference) +
                std::abs(blueDifference);
            redBias += redDifference;
            greenBias += greenDifference;
            blueBias += blueDifference;
        }
        channelError /= static_cast<double>(decoded.pixels.size() * 3);
        redBias /= static_cast<double>(decoded.pixels.size());
        greenBias /= static_cast<double>(decoded.pixels.size());
        blueBias /= static_cast<double>(decoded.pixels.size());
        if (channelError > fixture.maximumMeanChannelError) {
            std::cerr << fixture.name << ": mean channel error " << channelError
                      << " exceeds " << fixture.maximumMeanChannelError << '\n';
            return failure;
        }
        if (std::abs(redBias) > fixture.maximumMeanChannelBias ||
            std::abs(greenBias) > fixture.maximumMeanChannelBias ||
            std::abs(blueBias) > fixture.maximumMeanChannelBias) {
            std::cerr << fixture.name << ": mean channel bias " << redBias << ", "
                      << greenBias << ", " << blueBias << " exceeds "
                      << fixture.maximumMeanChannelBias << '\n';
            return failure;
        }
        ++failure;
    }
    return 0;
}
