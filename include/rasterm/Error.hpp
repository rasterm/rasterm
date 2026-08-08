/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace rasterm {

enum class ErrorCode : std::int32_t {
    None = 0,
    InvalidArgument = 1,
    InvalidOutputHandle = 2,
    OutputIsNotConsole = 3,
    ConsoleModeQueryFailed = 4,
    ConsoleModeEnableFailed = 5,
    TerminalDimensionsUnavailable = 6,
    UnsupportedTerminal = 7,
    StdoutModeFailed = 8,
    ControlHandlerRegistrationFailed = 9,
    DiagnosticOpenFailed = 10,
    OutputWriteFailed = 11,
    OutputFlushFailed = 12,
    OutputBufferLimitExceeded = 13,
    PresenterThreadStartFailed = 14,
    InitializationException = 15,
    RenderingException = 16,
    BufferTooSmall = 17,
    ApiVersionMismatch = 18,
    OutOfMemory = 19,
};

struct Status {
    ErrorCode code = ErrorCode::None;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::None; }
    explicit operator bool() const noexcept { return ok(); }

    static Status success() { return {}; }
    static Status failure(const ErrorCode code, std::string message)
    {
        return { code, std::move(message) };
    }
};

}