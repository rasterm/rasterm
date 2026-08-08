/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Capabilities.hpp>
#include <rasterm/Error.hpp>

#include <platform/windows/TerminalOwner.hpp>

#include <windows.h>

namespace rasterm {

class ConsoleSession {
public:
    ConsoleSession() = default;
    ~ConsoleSession();

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

    Status activate(bool useAlternateScreen);
    void restore() noexcept;
    [[nodiscard]] TerminalGeometry refreshGeometry() noexcept;
    [[nodiscard]] const TerminalCapabilities& capabilities() const noexcept { return detected; }

private:
    TerminalOwner owner;
    TerminalCapabilities detected{};
    HANDLE outputHandle = INVALID_HANDLE_VALUE;
    DWORD originalConsoleMode = 0;
    int originalStdoutMode = -1;
    bool controlHandlerRegistered = false;
};

TerminalCapabilities probeConsoleCapabilities() noexcept;

}