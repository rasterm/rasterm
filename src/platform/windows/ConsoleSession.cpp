/* SPDX-License-Identifier: Apache-2.0 */

#include <platform/windows/ConsoleSession.hpp>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <string>

namespace rasterm {
namespace {

std::atomic<HANDLE> activeHandle{ INVALID_HANDLE_VALUE };
std::atomic<DWORD> activeOriginalMode{ 0 };
std::atomic<bool> activeAlternateScreen{ false };
std::mutex controlStateMutex;

void writeAll(const HANDLE handle, const char* bytes, const std::size_t size) noexcept
{
    std::size_t offset = 0;
    while (offset < size) {
        DWORD written = 0;
        const DWORD remaining = static_cast<DWORD>(size - offset);
        if (!WriteFile(handle, bytes + offset, remaining, &written, nullptr) || written == 0) {
            return;
        }
        offset += written;
    }
}

bool environmentExists(const wchar_t* name) noexcept
{
    wchar_t value[128]{};
    return GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value))) > 0;
}

bool environmentContains(const wchar_t* name, const wchar_t* needle) noexcept
{
    wchar_t value[128]{};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    return length > 0 && length < std::size(value) && wcsstr(value, needle) != nullptr;
}

TerminalGeometry queryGeometry(const HANDLE handle) noexcept
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(handle, &info)) {
        return {};
    }
    const int columns = info.srWindow.Right - info.srWindow.Left + 1;
    const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (columns <= 0 || rows <= 0) {
        return {};
    }

    CONSOLE_FONT_INFOEX font{};
    font.cbSize = sizeof(font);
    int cellWidth = 10;
    int cellHeight = 20;
    if (GetCurrentConsoleFontEx(handle, FALSE, &font) &&
        font.dwFontSize.X > 0 && font.dwFontSize.Y > 0) {
        cellWidth = font.dwFontSize.X;
        cellHeight = font.dwFontSize.Y;
    }
    return {
        .columns = columns,
        .rows = rows,
        .pixelWidth = columns * cellWidth,
        .pixelHeight = rows * cellHeight,
        .cellPixelWidth = cellWidth,
        .cellPixelHeight = cellHeight,
    };
}

BOOL WINAPI restoreOnControlEvent(const DWORD event) noexcept
{
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT &&
        event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT) {
        return FALSE;
    }

    std::lock_guard lock(controlStateMutex);
    const HANDLE handle = activeHandle.load(std::memory_order_acquire);
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
        const char* sequence = activeAlternateScreen.load(std::memory_order_relaxed)
            ? "\x1b[?2026l\x1b[?25h\x1b[?1049l"
            : "\x1b[?2026l\x1b[?25h";
        writeAll(handle, sequence, std::strlen(sequence));
        SetConsoleMode(handle, activeOriginalMode.load(std::memory_order_relaxed));
    }
    return FALSE;
}

}

TerminalCapabilities probeConsoleCapabilities() noexcept
{
    TerminalCapabilities result{};
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    result.validOutputHandle = handle != INVALID_HANDLE_VALUE && handle != nullptr;
    if (!result.validOutputHandle) {
        result.sixel = CapabilitySupport::Unsupported;
        result.synchronizedOutput = CapabilitySupport::Unsupported;
        return result;
    }

    DWORD mode = 0;
    result.consoleOutput = GetFileType(handle) == FILE_TYPE_CHAR && GetConsoleMode(handle, &mode);
    result.virtualTerminalOutput = result.consoleOutput &&
        (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    result.geometry = queryGeometry(handle);

    const bool windowsTerminal = environmentExists(L"WT_SESSION") ||
        environmentContains(L"TERM_PROGRAM", L"Windows_Terminal");
    const bool declaredSixel = environmentContains(L"TERM", L"sixel");
    if (windowsTerminal) {
        result.sixel = CapabilitySupport::Supported;
        result.synchronizedOutput = CapabilitySupport::Supported;
    }
    else if (declaredSixel) {
        result.sixel = CapabilitySupport::Supported;
        result.synchronizedOutput = CapabilitySupport::Unknown;
    }
    else if (!result.consoleOutput) {
        result.sixel = CapabilitySupport::Unsupported;
        result.synchronizedOutput = CapabilitySupport::Unsupported;
    }
    return result;
}

ConsoleSession::~ConsoleSession()
{
    restore();
}

Status ConsoleSession::activate(const bool useAlternateScreen)
{
    restore();
    if (!owner.acquire()) {
        return Status::failure(ErrorCode::InvalidArgument,
                               "Another rasterm Engine owns the process terminal.");
    }
    detected = probeConsoleCapabilities();
    if (!detected.validOutputHandle) {
        owner.release();
        return Status::failure(ErrorCode::InvalidOutputHandle,
                               "STDOUT is not attached to a valid Windows handle.");
    }
    if (!detected.consoleOutput) {
        owner.release();
        return Status::failure(ErrorCode::OutputIsNotConsole,
                               "STDOUT is not a Windows console; provide an OutputSink for redirected output.");
    }
    if (detected.geometry.columns <= 0 || detected.geometry.rows <= 0) {
        owner.release();
        return Status::failure(ErrorCode::TerminalDimensionsUnavailable,
                               "Windows did not report positive terminal dimensions.");
    }

    outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(outputHandle, &originalConsoleMode)) {
        outputHandle = INVALID_HANDLE_VALUE;
        owner.release();
        return Status::failure(ErrorCode::ConsoleModeQueryFailed,
                               "Unable to query the Windows console output mode.");
    }
    const DWORD requiredMode = originalConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(outputHandle, requiredMode)) {
        outputHandle = INVALID_HANDLE_VALUE;
        owner.release();
        return Status::failure(ErrorCode::ConsoleModeEnableFailed,
                               "Unable to enable virtual-terminal output processing.");
    }
    DWORD verifiedMode = 0;
    if (!GetConsoleMode(outputHandle, &verifiedMode) ||
        (verifiedMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        SetConsoleMode(outputHandle, originalConsoleMode);
        outputHandle = INVALID_HANDLE_VALUE;
        owner.release();
        return Status::failure(ErrorCode::ConsoleModeEnableFailed,
                               "Virtual-terminal output mode could not be verified.");
    }
    detected.virtualTerminalOutput = true;

    originalStdoutMode = _setmode(_fileno(stdout), _O_BINARY);
    if (originalStdoutMode == -1) {
        SetConsoleMode(outputHandle, originalConsoleMode);
        outputHandle = INVALID_HANDLE_VALUE;
        owner.release();
        return Status::failure(ErrorCode::StdoutModeFailed,
                               "Unable to switch stdout to binary mode.");
    }

    {
        std::lock_guard lock(controlStateMutex);
        controlHandlerRegistered = SetConsoleCtrlHandler(restoreOnControlEvent, TRUE) != FALSE;
        if (controlHandlerRegistered) {
            activeOriginalMode.store(originalConsoleMode, std::memory_order_relaxed);
            activeAlternateScreen.store(useAlternateScreen, std::memory_order_relaxed);
            activeHandle.store(outputHandle, std::memory_order_release);
        }
    }
    if (!controlHandlerRegistered) {
        _setmode(_fileno(stdout), originalStdoutMode);
        originalStdoutMode = -1;
        SetConsoleMode(outputHandle, originalConsoleMode);
        outputHandle = INVALID_HANDLE_VALUE;
        owner.release();
        return Status::failure(ErrorCode::ControlHandlerRegistrationFailed,
                               "Unable to register terminal restoration for process cancellation.");
    }
    return Status::success();
}

void ConsoleSession::restore() noexcept
{
    if (controlHandlerRegistered) {
        {
            std::lock_guard lock(controlStateMutex);
            activeHandle.store(INVALID_HANDLE_VALUE, std::memory_order_release);
        }
        SetConsoleCtrlHandler(restoreOnControlEvent, FALSE);
        controlHandlerRegistered = false;
    }
    if (originalStdoutMode != -1) {
        _setmode(_fileno(stdout), originalStdoutMode);
        originalStdoutMode = -1;
    }
    if (outputHandle != INVALID_HANDLE_VALUE && outputHandle != nullptr) {
        SetConsoleMode(outputHandle, originalConsoleMode);
        outputHandle = INVALID_HANDLE_VALUE;
    }
    owner.release();
}

TerminalGeometry ConsoleSession::refreshGeometry() noexcept
{
    if (outputHandle != INVALID_HANDLE_VALUE && outputHandle != nullptr) {
        detected.geometry = queryGeometry(outputHandle);
    }
    return detected.geometry;
}

}