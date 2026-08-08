/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Error.hpp>

#include <string_view>

namespace rasterm {

enum class DiagnosticSeverity : std::int32_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct DiagnosticEvent {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    ErrorCode code = ErrorCode::None;
    std::string_view message;
};

using DiagnosticCallback = void (*)(const DiagnosticEvent& event, void* context) noexcept;

struct DiagnosticOptions {
    const char* filePath = nullptr;
    DiagnosticCallback callback = nullptr;
    void* callbackContext = nullptr;
};

}