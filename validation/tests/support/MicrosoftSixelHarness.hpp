/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/IndexedFrame.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace rasterm::test {

/* small round trip harness derived from the protocol behavior exercised by the
   Microsoft Terminal snapshot in SixelParser.cpp: RGB100 palette
   definitions, DECGCR, DECGNL, sixel data, and DECGRI repeat introducers. */

struct DecodedSixel {
    bool complete = false;
    int width = 0;
    int height = 0;
    std::vector<RgbColor> pixels;
};

class MicrosoftSixelHarness {
public:
    [[nodiscard]] DecodedSixel parse(const std::string_view input) const
    {
        DecodedSixel result;
        const std::size_t start = input.find("\x1bP");
        if (start == std::string_view::npos) {
            return result;
        }
        std::size_t position = input.find('q', start + 2);
        if (position == std::string_view::npos) {
            return result;
        }
        ++position;

        std::array<RgbColor, 256> palette{};
        int color = 0;
        int x = 0;
        int y = 0;
        while (position < input.size()) {
            if (input[position] == '\x1b' && position + 1 < input.size() &&
                input[position + 1] == '\\') {
                result.complete = true;
                break;
            }
            const char command = input[position++];
            if (command == '"') {
                const auto values = parameters(input, position);
                if (values.size() >= 4) {
                    result.width = values[2];
                    result.height = values[3];
                    result.pixels.assign(static_cast<std::size_t>(result.width) * result.height, {});
                }
            }
            else if (command == '#') {
                color = number(input, position) % static_cast<int>(palette.size());
                if (position < input.size() && input[position] == ';') {
                    ++position;
                    const auto values = parameters(input, position);
                    if (values.size() >= 4 && values[0] == 2) {
                        palette[color] = {
                            from100(values[1]),
                            from100(values[2]),
                            from100(values[3]),
                        };
                    }
                }
            }
            else if (command == '!') {
                const int repeat = std::max(1, number(input, position));
                if (position < input.size() && input[position] >= '?' && input[position] <= '~') {
                    draw(result, x, y, input[position++] - '?', repeat, palette[color]);
                    x += repeat;
                }
            }
            else if (command >= '?' && command <= '~') {
                draw(result, x, y, command - '?', 1, palette[color]);
                ++x;
            }
            else if (command == '$') {
                x = 0;
            }
            else if (command == '-') {
                x = 0;
                y += 6;
            }
        }
        return result;
    }

private:
    static int number(const std::string_view input, std::size_t& position)
    {
        int value = 0;
        while (position < input.size() && std::isdigit(static_cast<unsigned char>(input[position]))) {
            value = value * 10 + input[position++] - '0';
        }
        return value;
    }

    static std::vector<int> parameters(const std::string_view input, std::size_t& position)
    {
        std::vector<int> values;
        while (position < input.size()) {
            if (!std::isdigit(static_cast<unsigned char>(input[position]))) {
                break;
            }
            values.push_back(number(input, position));
            if (position >= input.size() || input[position] != ';') {
                break;
            }
            ++position;
        }
        return values;
    }

    static std::uint8_t from100(const int value) noexcept
    {
        return static_cast<std::uint8_t>(std::clamp((value * 255 + 50) / 100, 0, 255));
    }

    static void draw(DecodedSixel& result, const int x, const int y, const int bits,
                     const int repeat, const RgbColor color)
    {
        if (result.width <= 0 || result.height <= 0) {
            return;
        }
        for (int column = 0; column < repeat; ++column) {
            for (int bit = 0; bit < 6; ++bit) {
                const int pixelX = x + column;
                const int pixelY = y + bit;
                if ((bits & (1 << bit)) != 0 && pixelX >= 0 && pixelX < result.width &&
                    pixelY >= 0 && pixelY < result.height) {
                    result.pixels[static_cast<std::size_t>(pixelY) * result.width + pixelX] = color;
                }
            }
        }
    }
};

}