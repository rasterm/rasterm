/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Color.hpp>
#include <rasterm/Damage.hpp>

#include <cstdint>
#include <limits>

namespace rasterm {

inline constexpr std::int64_t unknownTimestamp = std::numeric_limits<std::int64_t>::min();

struct FrameMetadata {
    std::uint64_t frameId = 0;
    std::int64_t timestampNanoseconds = unknownTimestamp;
    ColorMetadata color{};
    ColorMetadata sourceColor{
        .primaries = ColorPrimaries::Unspecified,
        .transfer = TransferFunction::Unspecified,
        .matrix = MatrixCoefficients::Unspecified,
        .range = ColorRange::Unspecified,
    };
    DamageView damage{};
};

}