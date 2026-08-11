/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/OutputSink.hpp>
#include <rasterm/Error.hpp>

#include <string_view>
#include <cstddef>

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
    bool drawAtHome(std::string_view sixel, bool restoreCursor = false,
                    std::size_t chunkBytes = 0) noexcept;
    bool drawAtCell(int row, int column, std::string_view sixel,
                    bool restoreCursor = false, std::size_t chunkBytes = 0) noexcept;
    [[nodiscard]] bool good() const noexcept { return lastError == ErrorCode::None; }
    [[nodiscard]] ErrorCode error() const noexcept { return lastError; }
    [[nodiscard]] std::size_t acceptedBytes() const noexcept { return totalAcceptedBytes; }

private:
    bool write(std::string_view bytes) noexcept;
    bool writeChunks(std::string_view bytes, std::size_t chunkBytes) noexcept;
    bool flush() noexcept;
    bool recoveryWrite(std::string_view bytes) noexcept;
    bool recoveryFlush() noexcept;

    OutputSink& sink;
    bool usesAlternateScreen = false;
    bool updateActive = false;
    bool restoreCursorAfterUpdate = false;
    bool synchronizedUpdateActive = false;
    ErrorCode lastError = ErrorCode::None;
    std::size_t totalAcceptedBytes = 0;
};

}
