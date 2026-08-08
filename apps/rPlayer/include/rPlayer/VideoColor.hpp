/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Color.hpp>

#include <string>

namespace rasterm::rPlayer {

[[nodiscard]] ColorMetadata probeVideoColor(const std::string& path) noexcept;
[[nodiscard]] ColorMetadata decodedRgbColor(ColorMetadata source) noexcept;

}