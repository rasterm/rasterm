/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/rasterm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace rasterm::benchmark {

inline constexpr int corpusVersion = 3;

struct CorpusCase {
    CorpusCase(const std::string_view caseName, const int caseWidth, const int caseHeight,
               const PixelFormat caseFormat) noexcept :
        name(caseName), width(caseWidth), height(caseHeight), format(caseFormat)
    {
    }

    std::string_view name;
    int width;
    int height;
    PixelFormat format;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> indices;
    std::vector<RgbColor> palette;
    bool indexed = false;
    bool uiDamage = false;
    bool lowEntropyMotion = false;
};

inline void fillRgb(CorpusCase& item, const int seed, const bool highMotion)
{
    const int channels = bytesPerPixel(item.format);
    item.pixels.resize(static_cast<std::size_t>(item.width) * item.height * channels);
    std::uint32_t noise = static_cast<std::uint32_t>(seed + 1);
    for (int y = 0; y < item.height; ++y) {
        for (int x = 0; x < item.width; ++x) {
            noise = noise * 1664525u + 1013904223u;
            const auto red = static_cast<std::uint8_t>(highMotion ? noise >> 24 : (x + seed) & 0xff);
            const auto green = static_cast<std::uint8_t>(highMotion ? noise >> 16 : (y * 2 + seed) & 0xff);
            const auto blue = static_cast<std::uint8_t>(highMotion ? noise >> 8 : (x + y + seed) & 0xff);
            const std::size_t offset = static_cast<std::size_t>(y * item.width + x) * channels;
            if (item.format == PixelFormat::RGB24) {
                item.pixels[offset] = red;
                item.pixels[offset + 1] = green;
                item.pixels[offset + 2] = blue;
            }
            else {
                item.pixels[offset] = blue;
                item.pixels[offset + 1] = green;
                item.pixels[offset + 2] = red;
                item.pixels[offset + 3] = 255;
            }
        }
    }
}

inline CorpusCase packed16(std::string_view name, const PixelFormat format)
{
    CorpusCase item{ name, 256, 240, format };
    item.pixels.resize(static_cast<std::size_t>(item.width) * item.height * 2);
    for (int y = 0; y < item.height; ++y) {
        for (int x = 0; x < item.width; ++x) {
            std::uint16_t value = 0;
            if (format == PixelFormat::RGB565) {
                value = static_cast<std::uint16_t>(((x & 31) << 11) | ((y & 63) << 5) |
                                                   ((x + y) & 31));
            }
            else {
                value = static_cast<std::uint16_t>(((x & 15) << 12) | ((y & 15) << 8) |
                                                   (((x + y) & 15) << 4) | 0xf);
            }
            const std::size_t offset = static_cast<std::size_t>(y * item.width + x) * 2;
            item.pixels[offset] = static_cast<std::uint8_t>(value);
            item.pixels[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        }
    }
    return item;
}

inline CorpusCase uiBgra(std::string_view name, const bool damage)
{
    CorpusCase item{ name, 1280, 720, PixelFormat::BGRA32 };
    item.uiDamage = damage;
    item.pixels.resize(static_cast<std::size_t>(item.width) * item.height * 4);
    for (int y = 0; y < item.height; ++y) {
        for (int x = 0; x < item.width; ++x) {
            const bool panel = x >= 320 && x < 960 && y >= 180 && y < 540;
            const bool control = x >= 480 && x < 800 && y >= 420 && y < 480;
            const std::uint8_t value = control ? 0x6e : panel ? 0x1b : 0x11;
            const std::size_t offset = static_cast<std::size_t>(y * item.width + x) * 4;
            item.pixels[offset] = control ? 0xfe : value;
            item.pixels[offset + 1] = control ? 0xa8 : value;
            item.pixels[offset + 2] = control ? 0x6e : value;
            item.pixels[offset + 3] = 0xff;
        }
    }
    return item;
}

inline CorpusCase lowEntropyMotion()
{
    CorpusCase item{ "low-entropy-motion", 1280, 720, PixelFormat::BGRA32 };
    item.lowEntropyMotion = true;
    item.pixels.resize(static_cast<std::size_t>(item.width) * item.height * 4);
    for (std::size_t offset = 0; offset < item.pixels.size(); offset += 4) {
        item.pixels[offset] = 0x11;
        item.pixels[offset + 1] = 0x11;
        item.pixels[offset + 2] = 0x11;
        item.pixels[offset + 3] = 0xff;
    }
    return item;
}

inline std::vector<CorpusCase> makeCorpus()
{
    std::vector<CorpusCase> result;
    result.reserve(11);

    CorpusCase still{ "static-image", 640, 360, PixelFormat::RGB24 };
    fillRgb(still, 1, false);
    result.push_back(std::move(still));

    CorpusCase animation{ "animation", 320, 180, PixelFormat::RGB24 };
    fillRgb(animation, 19, false);
    result.push_back(std::move(animation));

    CorpusCase motion{ "high-motion-video", 640, 360, PixelFormat::RGB24 };
    fillRgb(motion, 31, true);
    result.push_back(std::move(motion));

    CorpusCase ui{ "ui-damage", 800, 450, PixelFormat::RGB24 };
    fillRgb(ui, 7, false);
    ui.uiDamage = true;
    result.push_back(std::move(ui));

    CorpusCase indexed{ "indexed-nes-256x240", 256, 240, PixelFormat::RGB24 };
    indexed.indexed = true;
    indexed.palette.resize(64);
    indexed.indices.resize(static_cast<std::size_t>(indexed.width) * indexed.height);
    for (std::size_t index = 0; index < indexed.palette.size(); ++index) {
        indexed.palette[index] = {
            static_cast<std::uint8_t>((index * 37) & 0xff),
            static_cast<std::uint8_t>((index * 73) & 0xff),
            static_cast<std::uint8_t>((index * 109) & 0xff),
        };
    }
    for (std::size_t index = 0; index < indexed.indices.size(); ++index) {
        indexed.indices[index] = static_cast<std::uint8_t>((index + index / indexed.width) & 63);
    }
    result.push_back(std::move(indexed));

    CorpusCase expanded{ "emulator-rgb24", 256, 240, PixelFormat::RGB24 };
    fillRgb(expanded, 11, false);
    result.push_back(std::move(expanded));
    result.push_back(packed16("emulator-rgb565", PixelFormat::RGB565));
    result.push_back(packed16("emulator-rgba4444", PixelFormat::RGBA4444));
    result.push_back(uiBgra("ui-bgra-full", false));
    result.push_back(uiBgra("ui-bgra-damage", true));
    result.push_back(lowEntropyMotion());
    return result;
}

}
