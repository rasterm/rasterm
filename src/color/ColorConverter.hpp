/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Color.hpp>
#include <rasterm/Frame.hpp>

#include <vector>

namespace rasterm {

class ColorConverter {
public:
    FrameView toSrgb(const FrameView& frame, const ColorOptions& options);

private:
    std::vector<std::uint8_t> pixels;
};

}