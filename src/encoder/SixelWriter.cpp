/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelWriter.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>

namespace rasterm {
namespace {

int macroParameterForAspectRatio(const int pixelAspectRatio)
{
    switch (pixelAspectRatio) {
    case 2: return 0;
    case 3: return 3;
    case 5: return 2;
    default: return 7;
    }
}

void appendNumber(std::string& output, const std::uint32_t value)
{
    char buffer[16];
    const auto [end, error] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    if (error == std::errc{}) output.append(buffer, end);
}

void appendPalette(std::string& output, const std::vector<uint32_t>& palette, const int colorCount,
                   const std::vector<bool>* colorsUsed = nullptr)
{
    const auto to100 = [](const int value) {
        return static_cast<int>(std::round(value * 100.0 / 255.0));
    };
    for (int i = 1; i < colorCount; ++i) {
        if (colorsUsed && !(*colorsUsed)[i]) {
            continue;
        }
        const uint32_t rgb = palette[i];
        output += '#';
        appendNumber(output, i);
        output += ";2;";
        appendNumber(output, to100((rgb >> 16) & 0xFF));
        output += ';';
        appendNumber(output, to100((rgb >> 8) & 0xFF));
        output += ';';
        appendNumber(output, to100(rgb & 0xFF));
    }
}

void emitRepeat(std::string& output, const int count, const int value)
{
    const char character = static_cast<char>('?' + std::clamp(value, 0, 63));
    constexpr int maximumRepeat = 65535;
    int remaining = count;
    while (remaining > 0) {
        const int run = std::min(remaining, maximumRepeat);
        if (run == 1) {
            output += character;
        }
        else if (run == 2) {
            output += character;
            output += character;
        }
        else {
            output += '!';
            appendNumber(output, run);
            output += character;
        }
        remaining -= run;
    }
}

void emitColorBand(std::string& output, const std::uint8_t* masks,
                   const int firstColumn, const int lastColumn, const int colorIndex)
{
    output += '#';
    appendNumber(output, colorIndex);
    if (firstColumn > 0) {
        emitRepeat(output, firstColumn, 0);
    }

    int column = firstColumn;
    while (column <= lastColumn) {
        const int bits = masks[column];
        int run = 1;
        while (column + run <= lastColumn && masks[column + run] == bits) {
            ++run;
        }
        emitRepeat(output, run, bits);
        column += run;
    }
}

}

void beginSixel(std::string& output, const SixelOptions& options)
{
    output += "\x1bP";
    appendNumber(output, macroParameterForAspectRatio(options.pixelAspectRatio));
    output += options.transparentBackground ? ";1;0q" : ";2;0q";
}

void emitRasterAttributes(std::string& output, const int width, const int height)
{
    output += "\"1;1;";
    appendNumber(output, width);
    output += ';';
    appendNumber(output, height);
}

void emitPalette(std::string& output, const FastColorMapper& mapper)
{
    appendPalette(output, mapper.colorNumToRgb, mapper.nextColorNum, &mapper.colorUsed);
}

void emitPalette(std::string& output, const FastAdaptiveColorMapper& mapper)
{
    appendPalette(output, mapper.palette, mapper.nextColorNum);
}

void emitPalette(std::string& output, const PaletteView palette, const int firstRegister)
{
    const auto to100 = [](const int value) {
        return (value * 100 + 127) / 255;
    };
    for (std::size_t index = 0; index < palette.size; ++index) {
        const RgbColor color = palette.colors[index];
        output += '#';
        appendNumber(output, static_cast<std::uint32_t>(index + firstRegister));
        output += ";2;";
        appendNumber(output, to100(color.red));
        output += ';';
        appendNumber(output, to100(color.green));
        output += ';';
        appendNumber(output, to100(color.blue));
    }
}

void emitIndexedFrame(std::string& output, const std::vector<uint8_t>& paletteIndices,
                      const int width, const int height, const int colorCount)
{
    static thread_local std::vector<std::uint8_t> bandMasks;
    static thread_local std::vector<int> colorsInBand;
    static thread_local std::vector<int> colorSlot;
    static thread_local std::vector<int> firstColumn;
    static thread_local std::vector<int> lastColumn;
    if (colorSlot.size() < static_cast<size_t>(colorCount)) {
        colorSlot.resize(colorCount);
        firstColumn.resize(colorCount);
        lastColumn.resize(colorCount);
    }

    const std::size_t maskCount = static_cast<std::size_t>(colorCount) * width;
    if (bandMasks.size() < maskCount) {
        bandMasks.resize(maskCount);
    }

    for (int band = 0; band < height; band += 6) {
        colorsInBand.clear();
        std::fill(colorSlot.begin(), colorSlot.begin() + colorCount, -1);
        const int bandEnd = std::min(band + 6, height);
        for (int y = band; y < bandEnd; ++y) {
            const int rowStart = y * width;
            for (int x = 0; x < width; ++x) {
                const int color = paletteIndices[rowStart + x];
                int slot = colorSlot[color];
                if (slot < 0) {
                    slot = static_cast<int>(colorsInBand.size());
                    colorSlot[color] = slot;
                    colorsInBand.push_back(color);
                    firstColumn[color] = x;
                    lastColumn[color] = x;
                    std::fill_n(bandMasks.begin() + static_cast<std::size_t>(slot) * width,
                                width, std::uint8_t{});
                }
                else {
                    firstColumn[color] = std::min(firstColumn[color], x);
                    lastColumn[color] = std::max(lastColumn[color], x);
                }
                bandMasks[static_cast<std::size_t>(slot) * width + x] |=
                    static_cast<std::uint8_t>(1U << (y - band));
            }
        }
        for (std::size_t index = 0; index < colorsInBand.size(); ++index) {
            const int color = colorsInBand[index];
            emitColorBand(output, bandMasks.data() + index * width,
                          firstColumn[color], lastColumn[color], color);
            if (index + 1 < colorsInBand.size() || band + 6 >= height) {
                output += '$';
            }
        }
        if (band + 6 < height) {
            output += '-';
        }
    }
}

}
