/* SPDX-License-Identifier: Apache-2.0 */

#include <output/StdoutSink.hpp>

#include <cstdio>

namespace rasterm {

bool StdoutSink::write(const std::string_view bytes) noexcept
{
    return bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size();
}

bool StdoutSink::flush() noexcept
{
    return std::fflush(stdout) == 0;
}

}