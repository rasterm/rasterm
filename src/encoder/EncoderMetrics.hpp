/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <chrono>

namespace rasterm {

struct EncodeStageTimings {
    std::chrono::nanoseconds analysis{};
    std::chrono::nanoseconds mapping{};
    std::chrono::nanoseconds writing{};
    std::chrono::nanoseconds budgeting{};
};

}
