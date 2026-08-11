/* SPDX-License-Identifier: Apache-2.0 */

#include <output/TerminalRenderer.hpp>

#include <algorithm>
#include <cstdio>

namespace rasterm {

TerminalRenderer::TerminalRenderer(OutputSink& output, const bool useAlternateScreen) :
    sink(output),
    usesAlternateScreen(useAlternateScreen)
{
    if (usesAlternateScreen) {
        write("\x1b[?1049h");
    }
    write("\x1b[?25l");
    write("\x1b[?80l");
    flush();
}

TerminalRenderer::~TerminalRenderer()
{
    endSynchronizedUpdate();
    recoveryWrite("\x1b[?25h");
    if (usesAlternateScreen) {
        recoveryWrite("\x1b[?1049l");
    }
    recoveryFlush();
}

bool TerminalRenderer::write(const std::string_view bytes) noexcept
{
    if (!sink.write(bytes)) {
        lastError = ErrorCode::OutputWriteFailed;
        return false;
    }
    totalAcceptedBytes += bytes.size();
    return true;
}

bool TerminalRenderer::writeChunks(const std::string_view bytes,
                                   const std::size_t chunkBytes) noexcept
{
    if (chunkBytes == 0 || bytes.size() <= chunkBytes) return write(bytes);
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t count = std::min(chunkBytes, bytes.size() - offset);
        if (!write(bytes.substr(offset, count))) {
            if (offset > 0) recoveryWrite("\x1b\\");
            return false;
        }
        offset += count;
    }
    return true;
}

bool TerminalRenderer::flush() noexcept
{
    if (!sink.flush()) {
        lastError = ErrorCode::OutputFlushFailed;
        return false;
    }
    return true;
}

bool TerminalRenderer::recoveryWrite(const std::string_view bytes) noexcept
{
    if (sink.write(bytes)) {
        totalAcceptedBytes += bytes.size();
        return true;
    }
    if (lastError == ErrorCode::None) {
        lastError = ErrorCode::OutputWriteFailed;
    }
    (void)sink.write(bytes);
    return false;
}

bool TerminalRenderer::recoveryFlush() noexcept
{
    if (sink.flush()) {
        return true;
    }
    if (lastError == ErrorCode::None) {
        lastError = ErrorCode::OutputFlushFailed;
    }
    (void)sink.flush();
    return false;
}

bool TerminalRenderer::clearAndHome() noexcept
{
    bool success = write("\x1b[2J\x1b[H");
    if (!updateActive) {
        success = flush() && success;
    }
    return success;
}

bool TerminalRenderer::beginSynchronizedUpdate(const bool preserveCursor,
                                               const bool synchronizedOutput) noexcept
{
    if (updateActive) {
        return good();
    }
    lastError = ErrorCode::None;
    updateActive = true;
    restoreCursorAfterUpdate = false;
    synchronizedUpdateActive = false;
    bool success = true;
    if (synchronizedOutput) {
        synchronizedUpdateActive = write("\x1b[?2026h");
        success = synchronizedUpdateActive;
    }
    if (preserveCursor) {
        restoreCursorAfterUpdate = write("\x1b" "7");
        success = restoreCursorAfterUpdate && success;
    }
    return success;
}

bool TerminalRenderer::endSynchronizedUpdate() noexcept
{
    if (!updateActive) {
        return good();
    }
    bool success = true;
    if (restoreCursorAfterUpdate) {
        success = recoveryWrite("\x1b" "8") && success;
    }
    if (synchronizedUpdateActive) {
        success = recoveryWrite("\x1b[?2026l") && success;
    }
    success = recoveryFlush() && success;
    updateActive = false;
    restoreCursorAfterUpdate = false;
    synchronizedUpdateActive = false;
    return success;
}

bool TerminalRenderer::drawAtHome(const std::string_view sixel, const bool restoreCursor,
                                  const std::size_t chunkBytes) noexcept
{
    const bool saveLocally = restoreCursor && !updateActive;
    bool success = true;
    if (saveLocally) {
        success = write("\x1b" "7");
    }

    /* in scrolling mode a final, partial sixel band can move the viewport when
     * a full height image is not divisible by six. display mode clamps the full
     * image to the page; positioned patches switch back to scrolling mode. */


    success = write("\x1b[?80h") && success;
    success = write("\x1b[H") && success;
    success = writeChunks(sixel, chunkBytes) && success;
    success = recoveryWrite("\x1b[?80l") && success;
    if (saveLocally) {
        success = recoveryWrite("\x1b" "8") && success;
    }
    if (!updateActive) {
        success = flush() && success;
    }
    return success;
}

bool TerminalRenderer::drawAtCell(const int row, const int column, const std::string_view sixel,
                                  const bool restoreCursor,
                                  const std::size_t chunkBytes) noexcept
{
    const bool saveLocally = restoreCursor && !updateActive;
    bool success = true;
    if (saveLocally) {
        success = write("\x1b" "7");
    }
    char cursor[32];
    const int length = std::snprintf(cursor, sizeof(cursor), "\x1b[%d;%dH", row + 1, column + 1);
    success = write({ cursor, static_cast<std::size_t>(length) }) && success;
    success = writeChunks(sixel, chunkBytes) && success;
    if (saveLocally) {
        success = recoveryWrite("\x1b" "8") && success;
    }
    if (!updateActive) {
        success = flush() && success;
    }
    return success;
}

}
