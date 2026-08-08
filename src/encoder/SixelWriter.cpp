/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/SixelWriter.hpp>

#include <algorithm>
#include <cmath>

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
        output += std::to_string(i);
        output += ";2;";
        output += std::to_string(to100((rgb >> 16) & 0xFF));
        output += ';';
        output += std::to_string(to100((rgb >> 8) & 0xFF));
        output += ';';
        output += std::to_string(to100(rgb & 0xFF));
    }
}

void emitRepeat(std::string& output, const int count, const int value)
{
    const char character = static_cast<char>('?' + std::clamp(value, 0, 63));
    if (count == 1) {
        output += character;
    }
    else if (count == 2) {
        output += character;
        output += character;
    }
    else {
        output += '!';
        output += std::to_string(count);
        output += character;
    }
}

}

void beginSixel(std::string& output, const SixelOptions& options)
{
    output += "\x1bP";
    output += std::to_string(macroParameterForAspectRatio(options.pixelAspectRatio));
    output += options.transparentBackground ? ";1;0q" : ";2;0q";
}

void emitRasterAttributes(std::string& output, const int width, const int height)
{
    output += "\"1;1;";
    output += std::to_string(width);
    output += ';';
    output += std::to_string(height);
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
        output += std::to_string(index + firstRegister);
        output += ";2;";
        output += std::to_string(to100(color.red));
        output += ';';
        output += std::to_string(to100(color.green));
        output += ';';
        output += std::to_string(to100(color.blue));
    }
}

void emitColorBand(std::string& output, const std::vector<uint8_t>& paletteIndices,
                     const int width, const int height, const int bandRow, const int colorIndex)
{
    output += '#';
    output += std::to_string(colorIndex);
    const int bandHeight = std::min(6, height - bandRow);
    const uint8_t* rows[6];
    for (int i = 0; i < bandHeight; ++i) {
        rows[i] = paletteIndices.data() + (bandRow + i) * width;
    }

    int column = 0;
    while (column < width) {
        int bits = 0;
        for (int i = 0; i < bandHeight; ++i) {
            bits |= (rows[i][column] == colorIndex) << i;
        }

        int run = 1;
        while (column + run < width) {
            int nextBits = 0;
            for (int i = 0; i < bandHeight; ++i) {
                nextBits |= (rows[i][column + run] == colorIndex) << i;
            }
            if (nextBits != bits) {
                break;
            }
            ++run;
        }
        emitRepeat(output, run, bits);
        column += run;
    }
    output += '$';
}

void emitIndexedFrame(std::string& output, const std::vector<uint8_t>& paletteIndices,
                        const int width, const int height, const int colorCount)
{
    static thread_local std::vector<int> colorsInBand;
    static thread_local std::vector<bool> colorSeen;
    if (colorSeen.size() < static_cast<size_t>(colorCount)) {
        colorSeen.resize(colorCount);
    }

    for (int band = 0; band < height; band += 6) {
        colorsInBand.clear();
        std::fill(colorSeen.begin(), colorSeen.begin() + colorCount, false);
        const int bandEnd = std::min(band + 6, height);
        for (int y = band; y < bandEnd; ++y) {
            const int rowStart = y * width;
            for (int x = 0; x < width; ++x) {
                const int color = paletteIndices[rowStart + x];
                if (!colorSeen[color]) {
                    colorSeen[color] = true;
                    colorsInBand.push_back(color);
                }
            }
        }
        for (const int color : colorsInBand) {
            emitColorBand(output, paletteIndices, width, height, band, color);
        }
        if (band + 6 < height) {
            output += '-';
        }
    }
}

}