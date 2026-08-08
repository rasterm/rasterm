/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

namespace rasterm {

class TerminalOwner {
public:
    TerminalOwner() = default;
    ~TerminalOwner();

    TerminalOwner(const TerminalOwner&) = delete;
    TerminalOwner& operator=(const TerminalOwner&) = delete;

    [[nodiscard]] bool acquire() noexcept;
    void release() noexcept;
    [[nodiscard]] bool ownsTerminal() const noexcept { return owned; }

private:
    bool owned = false;
};

}