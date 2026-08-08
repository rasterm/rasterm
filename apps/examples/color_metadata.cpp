/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <string_view>

class DiscardSink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override { return true; }
    bool flush() noexcept override { return true; }
};

int main()
{
    constexpr int width = 2;
    constexpr int height = 2;
    std::array<std::uint8_t, width * height * 4> pixels{};
    DiscardSink sink;
    rasterm::Engine engine;
    if (!engine.initialize({
            .color = { .convertToSrgb = true,
                       .toneMap = rasterm::ToneMapOperator::Aces,
                       .outputPeakNits = 203.0f },
            .output = &sink,
        })) return 1;

    rasterm::FrameMetadata metadata;
    metadata.color = {
        .primaries = rasterm::ColorPrimaries::Bt2020,
        .transfer = rasterm::TransferFunction::Pq,
        .matrix = rasterm::MatrixCoefficients::Bt2020NonConstant,
        .range = rasterm::ColorRange::Limited,
        .referenceWhiteNits = 203.0f,
        .masteringPeakNits = 1000.0f,
    };
    const rasterm::FrameView frame{
        pixels.data(), width, height, width * 4, rasterm::PixelFormat::RGBA32, metadata,
    };
    return engine.renderFrame(frame).error == rasterm::ErrorCode::None ? 0 : 2;
}