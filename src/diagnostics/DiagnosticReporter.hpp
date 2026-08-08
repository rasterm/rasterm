/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Diagnostics.hpp>
#include <rasterm/Error.hpp>

#include <cstdio>
#include <mutex>

namespace rasterm {

class DiagnosticReporter {
public:
    DiagnosticReporter() = default;
    ~DiagnosticReporter();

    DiagnosticReporter(const DiagnosticReporter&) = delete;
    DiagnosticReporter& operator=(const DiagnosticReporter&) = delete;

    Status configure(const DiagnosticOptions& options);
    void reset() noexcept;
    void report(DiagnosticSeverity severity, ErrorCode code, std::string_view message) noexcept;

private:
    std::mutex mutex;
    std::FILE* file = nullptr;
    DiagnosticCallback callback = nullptr;
    void* callbackContext = nullptr;
};

}