/* SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

struct Options {
    std::string_view position = "left";
    int regions = 1;
    int updates = 10000;
    int warmupUpdates = 100;
    bool prewarm = true;
};

bool parseInteger(const std::string_view text, int& value)
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() && value >= 0;
}

bool parseOptions(const int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const auto takeValue = [&](std::string_view& value) {
            if (++index >= argc) return false;
            value = argv[index];
            return true;
        };

        std::string_view value;
        if (argument == "--position") {
            if (!takeValue(value) ||
                (value != "left" && value != "middle" && value != "right")) {
                return false;
            }
            options.position = value;
        }
        else if (argument == "--regions") {
            if (!takeValue(value) || !parseInteger(value, options.regions) ||
                (options.regions != 1 && options.regions != 16)) {
                return false;
            }
        }
        else if (argument == "--updates") {
            if (!takeValue(value) || !parseInteger(value, options.updates) ||
                options.updates == 0) {
                return false;
            }
        }
        else if (argument == "--warmup-updates") {
            if (!takeValue(value) || !parseInteger(value, options.warmupUpdates)) return false;
        }
        else if (argument == "--no-prewarm") {
            options.prewarm = false;
        }
        else {
            return false;
        }
    }
    return true;
}

void appendNumber(std::string& output, const int value)
{
    char buffer[16];
    const auto [end, error] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    if (error == std::errc{}) output.append(buffer, end);
}

std::string makePatch(const int column, const int row)
{
    constexpr int patchHeight = 48;
    std::string patch = "\x1b[";
    appendNumber(patch, row + 1);
    patch += ';';
    appendNumber(patch, column + 1);
    patch += "H\x1bP7;1;0q\"1;1;120;48#1;2;100;0;0";
    for (int band = 0; band < patchHeight / 6; ++band) {
        patch += "!120~";
        patch += band + 1 < patchHeight / 6 ? '-' : '$';
    }
    patch += "\x1b\\";
    return patch;
}

std::string makePrewarm(const int pixelHeight)
{
    std::string prewarm = "\x1b[?80h\x1bP7;1;0q\"1;1;1;";
    appendNumber(prewarm, pixelHeight);
    const int bands = (pixelHeight + 5) / 6;
    for (int band = 0; band < bands; ++band) {
        prewarm += '@';
        if (band + 1 < bands) prewarm += '-';
    }
    prewarm += "\x1b\\\x1b[?80l";
    return prewarm;
}

bool writeAll(const HANDLE output, const std::string_view bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        DWORD written = 0;
        const auto remaining = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, MAXDWORD));
        if (!WriteFile(output, bytes.data() + offset, remaining, &written, nullptr) || written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

}

int main(const int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        std::cerr << "usage: rasterm-windows-terminal-sixel-repro "
                     "[--position left|middle|right] [--regions 1|16] "
                     "[--updates N] [--warmup-updates N] [--no-prewarm]\n";
        return 2;
    }

    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO console{};
    if (output == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(output, &console)) {
        std::cerr << "stdout is not an attached Windows console\n";
        return 3;
    }

    const int columns = console.srWindow.Right - console.srWindow.Left + 1;
    const int rows = console.srWindow.Bottom - console.srWindow.Top + 1;
    constexpr int virtualCellWidth = 10;
    constexpr int virtualCellHeight = 20;
    constexpr int patchCellWidth = 12;
    const int column = options.position == "left" ? 0 :
        options.position == "middle" ? columns / 2 : columns - patchCellWidth;
    const int requiredRows = options.regions == 1 ? 3 : 63;
    if (columns < patchCellWidth || column < 0 || rows < requiredRows) {
        std::cerr << "terminal is too small: need at least " << patchCellWidth << 'x'
                  << requiredRows << " cells, got " << columns << 'x' << rows << '\n';
        return 4;
    }

    std::string update = "\x1b[?2026h";
    for (int region = 0; region < options.regions; ++region) {
        update += makePatch(column, region * 4);
    }
    update += "\x1b[?2026l";

    if (!writeAll(output, "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H\x1b[?80l")) return 5;
    if (options.prewarm && !writeAll(output, makePrewarm(rows * virtualCellHeight))) return 5;
    for (int warmup = 0; warmup < options.warmupUpdates; ++warmup) {
        if (!writeAll(output, update)) return 5;
    }

    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < options.updates; ++iteration) {
        if (!writeAll(output, update)) return 5;
    }
    const double elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    writeAll(output, "\x1b[?25h\x1b[?1049l");

    const std::size_t stridePixels = static_cast<std::size_t>(columns - column) *
        virtualCellWidth;
    const std::size_t logicalInitializationBytes = stridePixels * 48 * 2 * options.regions;
    std::cerr << "position=" << options.position
              << ",regions=" << options.regions
              << ",updates=" << options.updates
              << ",terminal_cells=" << columns << 'x' << rows
              << ",logical_init_bytes_per_update=" << logicalInitializationBytes
              << ",producer_write_ms_per_update=" << elapsedMilliseconds / options.updates
              << '\n';
    return 0;
}
