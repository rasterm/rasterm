/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace {

class Sink final : public rasterm::OutputSink {
public:
    bool write(std::string_view) noexcept override { return true; }
    bool flush() noexcept override { return true; }
};

}

int main()
{
    std::array<std::uint8_t, 16> bytes{};
    const auto invalidFormat = static_cast<rasterm::PixelFormat>(99);
    if (rasterm::bytesPerPixel(invalidFormat) != 0 ||
        rasterm::FrameView{ bytes.data(), 1, 1, 3, invalidFormat }.isValid()) return 1;
    if (rasterm::FrameView{ bytes.data(), 1, 1, -3, rasterm::PixelFormat::RGB24 }.isValid() ||
        rasterm::FrameView{ bytes.data(), 0, 1, 3, rasterm::PixelFormat::RGB24 }.isValid() ||
        rasterm::FrameView{ bytes.data(), 1, 0, 3, rasterm::PixelFormat::RGB24 }.isValid()) return 2;
    if (rasterm::FrameView{
            bytes.data(), (std::numeric_limits<int>::max)(),
            (std::numeric_limits<int>::max)(),
            (std::numeric_limits<std::ptrdiff_t>::max)(), rasterm::PixelFormat::RGBA32
        }.isValid()) return 3;
    if (rasterm::FrameView{
            bytes.data(), 1, 2, (std::numeric_limits<std::ptrdiff_t>::max)(),
            rasterm::PixelFormat::RGB24
        }.isValid()) return 4;

    const rasterm::DamageRect overflow{
        (std::numeric_limits<int>::max)() - 1, 0, 4, 1
    };
    const rasterm::DamageRect edge{ 9, 9, 1, 1 };
    if (rasterm::DamageView{ &overflow, 1, true }.isValidFor(
            (std::numeric_limits<int>::max)(), 1) ||
        !rasterm::DamageView{ &edge, 1, true }.isValidFor(10, 10)) {
        return 5;
    }
    const rasterm::DamageRect empty{ 0, 0, 0, 1 };
    if (rasterm::DamageView{ &empty, 1, true }.isValidFor(10, 10)) return 5;

    std::array<rasterm::RgbColor, 2> palette{{ {0, 0, 0}, {255, 255, 255} }};
    std::array<std::uint8_t, 2> indices{{ 0, 2 }};
    rasterm::IndexedFrameView indexed{
        indices.data(), 2, 1, 2, { palette.data(), palette.size() }
    };
    if (indexed.hasValidIndices()) return 6;
    indexed.indices = bytes.data();
    indexed.palette = { palette.data(), 257 };
    if (indexed.isValid()) return 7;
    indexed.palette = { nullptr, 1 };
    if (indexed.isValid()) return 8;
    indexed.palette = { palette.data(), palette.size() };
    indexed.stride = -2;
    if (indexed.isValid()) return 9;

    Sink sink;
    rasterm::Engine engine;
    if (!engine.initialize({ .output = &sink })) return 10;
    const auto rejected = engine.renderFrame(bytes.data(), 1, 1, 3, invalidFormat);
    if (rejected.error != rasterm::ErrorCode::InvalidArgument || engine.status().message.empty()) {
        return 11;
    }

    rasterm::FrameView malformed{ bytes.data(), 1, 1, 3, rasterm::PixelFormat::RGB24 };
    malformed.metadata.color.transfer = static_cast<rasterm::TransferFunction>(99);
    if (engine.renderFrame(malformed).error != rasterm::ErrorCode::InvalidArgument) return 12;
    const auto invalidLayout = rasterm::calculateScaleLayout(
        { 1, 1 }, { 1, 1 }, static_cast<rasterm::ScalePolicy>(99));
    const auto overflowingLayout = rasterm::calculateScaleLayout(
        { (std::numeric_limits<int>::max)(), 1 },
        { 1, (std::numeric_limits<int>::max)() }, rasterm::ScalePolicy::Fill);
    if (invalidLayout.canvas.width != 0 || overflowingLayout.canvas.width != 0) return 13;

    rasterm::FrameMetadata badDamage;
    badDamage.damage = { nullptr, 1, true };
    rasterm::OwnedFrame owned({ 0, 0, 0 }, 1, 1, 3, rasterm::PixelFormat::RGB24,
                              badDamage);
    if (owned.isValid()) return 14;
    engine.shutdown();
    return 0;
}