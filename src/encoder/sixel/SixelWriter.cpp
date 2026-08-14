/* SPDX-License-Identifier: Apache-2.0 */

#include <encoder/sixel/SixelWriter.hpp>

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

void appendPalette(SixelOutput& output, const std::vector<uint32_t>& palette, const int colorCount,
                   const std::vector<bool>* colorsUsed = nullptr)
{
    const auto to100 = [](const int value) {
        return static_cast<int>(std::round(value * 100.0 / 255.0));
    };
    for (int i = 1; i < colorCount; ++i) {
        if (output.exceeded()) break;
        if (colorsUsed && !(*colorsUsed)[i]) {
            continue;
        }
        const uint32_t rgb = palette[i];
        output.append('#');
        output.appendNumber(i);
        output.append(";2;");
        output.appendNumber(to100((rgb >> 16) & 0xFF));
        output.append(';');
        output.appendNumber(to100((rgb >> 8) & 0xFF));
        output.append(';');
        output.appendNumber(to100(rgb & 0xFF));
    }
}

void emitRepeat(SixelOutput& output, const int count, const int value)
{
    const char character = static_cast<char>('?' + std::clamp(value, 0, 63));
    constexpr int maximumRepeat = 65535;
    int remaining = count;
    while (remaining > 0) {
        if (output.exceeded()) break;
        const int run = std::min(remaining, maximumRepeat);
        if (run == 1) {
            output.append(character);
        }
        else if (run == 2) {
            output.append(character);
            output.append(character);
        }
        else {
            output.append('!');
            output.appendNumber(run);
            output.append(character);
        }
        remaining -= run;
    }
}

void emitColorBand(SixelOutput& output, const std::uint8_t* masks,
                   const int firstColumn, const int lastColumn, const int colorIndex)
{
    output.append('#');
    output.appendNumber(colorIndex);
    if (firstColumn > 0) {
        emitRepeat(output, firstColumn, 0);
    }

    int column = firstColumn;
    while (column <= lastColumn) {
        if (output.exceeded()) break;
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

bool SixelOutput::append(const std::string_view bytes)
{
    if (limitExceeded) return false;
    if (maximumBytes > 0 &&
        (storage.size() > maximumBytes || bytes.size() > maximumBytes - storage.size())) {
        limitExceeded = true;
        return false;
    }
    storage.append(bytes);
    return true;
}

bool SixelOutput::append(const char byte)
{
    return append(std::string_view(&byte, 1));
}

bool SixelOutput::appendNumber(const std::uint32_t value)
{
    char buffer[16];
    const auto [end, error] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    return error == std::errc{} && append({ buffer, static_cast<std::size_t>(end - buffer) });
}

void beginSixel(SixelOutput& output, const SixelOptions& options)
{
    output.append("\x1bP");
    output.appendNumber(macroParameterForAspectRatio(options.pixelAspectRatio));
    output.append(options.transparentBackground ? ";1;0q" : ";2;0q");
}

void beginSixel(std::string& output, const SixelOptions& options)
{
    SixelOutput checked(output);
    beginSixel(checked, options);
}

void emitRasterAttributes(SixelOutput& output, const int width, const int height)
{
    output.append("\"1;1;");
    output.appendNumber(width);
    output.append(';');
    output.appendNumber(height);
}

void emitRasterAttributes(std::string& output, const int width, const int height)
{
    SixelOutput checked(output);
    emitRasterAttributes(checked, width, height);
}

void emitPalette(SixelOutput& output, const FixedPaletteMapper& mapper)
{
    appendPalette(output, mapper.colorNumToRgb, mapper.nextColorNum, &mapper.colorUsed);
}

void emitFullPalette(SixelOutput& output, const FixedPaletteMapper& mapper)
{
    appendPalette(output, mapper.colorNumToRgb, mapper.nextColorNum);
}

void emitPalette(SixelOutput& output, const AdaptivePaletteMapper& mapper)
{
    appendPalette(output, mapper.palette, mapper.nextColorNum);
}

void emitPalette(SixelOutput& output, const PaletteView palette, const int firstRegister)
{
    const auto to100 = [](const int value) {
        return (value * 100 + 127) / 255;
    };
    for (std::size_t index = 0; index < palette.size; ++index) {
        if (output.exceeded()) break;
        const RgbColor color = palette.colors[index];
        output.append('#');
        output.appendNumber(static_cast<std::uint32_t>(index + firstRegister));
        output.append(";2;");
        output.appendNumber(to100(color.red));
        output.append(';');
        output.appendNumber(to100(color.green));
        output.append(';');
        output.appendNumber(to100(color.blue));
    }
}

void emitIndexedFrame(SixelOutput& output, const std::vector<uint8_t>& paletteIndices,
                      const int width, const int height, const int colorCount)
{
    if (output.exceeded()) return;

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
        if (output.exceeded()) break;
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
            if (output.exceeded()) break;
            const int color = colorsInBand[index];
            emitColorBand(output, bandMasks.data() + index * width,
                          firstColumn[color], lastColumn[color], color);
            if (index + 1 < colorsInBand.size() || band + 6 >= height) {
                output.append('$');
            }
        }
        if (band + 6 < height) {
            output.append('-');
        }
    }
}

void emitPalette(std::string& output, const FixedPaletteMapper& mapper)
{
    SixelOutput checked(output);
    emitPalette(checked, mapper);
}

void emitPalette(std::string& output, const AdaptivePaletteMapper& mapper)
{
    SixelOutput checked(output);
    emitPalette(checked, mapper);
}

void emitPalette(std::string& output, const PaletteView palette, const int firstRegister)
{
    SixelOutput checked(output);
    emitPalette(checked, palette, firstRegister);
}

void emitIndexedFrame(std::string& output, const std::vector<uint8_t>& paletteIndices,
                      const int width, const int height, const int colorCount)
{
    SixelOutput checked(output);
    emitIndexedFrame(checked, paletteIndices, width, height, colorCount);
}

}
