/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string_view>

namespace rasterm::test::fixtures {

inline constexpr std::string_view solidRed1x1 =
    "\x1bP7;2;0q\"1;1;1;1#1;2;100;0;0#1@$\x1b\\";

inline constexpr std::string_view solidRed2x1Band = "#1@@$";
inline constexpr std::string_view solidRed3x1Band = "#1!3@$";

}