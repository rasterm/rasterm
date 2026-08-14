/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string_view>

namespace rasterm {

class OutputSink {
public:
    virtual ~OutputSink() = default;

    /* implementations must accept all bytes or return false without accepting any.
       rasterm may issue a best effort retry for terminal restoration sequences. */

    virtual bool write(std::string_view bytes) noexcept = 0;
    virtual bool flush() noexcept = 0;
};

}
