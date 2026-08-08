/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Statistics.hpp>

namespace rasterm {

enum class CapabilitySupport : std::int32_t {
    Unknown = 0,
    Unsupported = 1,
    Supported = 2,
};

struct TerminalCapabilities {
    bool customOutput = false;
    bool validOutputHandle = false;
    bool consoleOutput = false;
    bool virtualTerminalOutput = false;
    CapabilitySupport sixel = CapabilitySupport::Unknown;
    CapabilitySupport synchronizedOutput = CapabilitySupport::Unknown;
    TerminalGeometry geometry{};
};

}