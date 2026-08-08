/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/OutputSink.hpp>
#include <rasterm/Error.hpp>

#include <string_view>

namespace rasterm {

class TerminalRenderer {
public:
    explicit TerminalRenderer(OutputSink& output, bool useAlternateScreen = false);
    ~TerminalRenderer();

    TerminalRenderer(const TerminalRenderer&) = delete;
    TerminalRenderer& operator=(const TerminalRenderer&) = delete;

    bool clearAndHome() noexcept;
    bool beginSynchronizedUpdate(bool preserveCursor = true,
                                 bool synchronizedOutput = true) noexcept;
    bool endSynchronizedUpdate() noexcept;
    bool drawAtHome(std::string_view sixel, bool restoreCursor = false) noexcept;
    bool drawAtCell(int row, int column, std::string_view sixel,
                    bool restoreCursor = false) noexcept;
    [[nodiscard]] bool good() const noexcept { return lastError == ErrorCode::None; }
    [[nodiscard]] ErrorCode error() const noexcept { return lastError; }

private:
    bool write(std::string_view bytes) noexcept;
    bool flush() noexcept;
    bool recoveryWrite(std::string_view bytes) noexcept;
    bool recoveryFlush() noexcept;

    OutputSink& sink;
    bool usesAlternateScreen = false;
    bool updateActive = false;
    bool restoreCursorAfterUpdate = false;
    bool synchronizedUpdateActive = false;
    ErrorCode lastError = ErrorCode::None;
};

}