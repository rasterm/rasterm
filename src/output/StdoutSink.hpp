/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/OutputSink.hpp>

namespace rasterm {

class StdoutSink final : public OutputSink {
public:
    bool write(std::string_view bytes) noexcept override;
    bool flush() noexcept override;
};

}