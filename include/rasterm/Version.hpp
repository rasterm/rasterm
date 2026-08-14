/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>

#define RASTERM_VERSION_MAJOR 1
#define RASTERM_VERSION_MINOR 3
#define RASTERM_VERSION_PATCH 0
#define RASTERM_ABI_VERSION 1
#define RASTERM_VERSION_PRERELEASE ""

namespace rasterm {

struct Version {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

inline constexpr Version version{
    RASTERM_VERSION_MAJOR,
    RASTERM_VERSION_MINOR,
    RASTERM_VERSION_PATCH,
};
inline constexpr std::uint32_t abiVersion = RASTERM_ABI_VERSION;

}
