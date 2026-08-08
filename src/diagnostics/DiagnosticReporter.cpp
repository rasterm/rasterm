/* SPDX-License-Identifier: Apache-2.0 */

#include <diagnostics/DiagnosticReporter.hpp>

#include <chrono>

namespace rasterm {
namespace {

const char* severityName(const DiagnosticSeverity severity) noexcept
{
    switch (severity) {
    case DiagnosticSeverity::Info: return "info";
    case DiagnosticSeverity::Warning: return "warning";
    case DiagnosticSeverity::Error: return "error";
    }
    return "unknown";
}

}

DiagnosticReporter::~DiagnosticReporter()
{
    reset();
}

Status DiagnosticReporter::configure(const DiagnosticOptions& options)
{
    reset();
    callback = options.callback;
    callbackContext = options.callbackContext;
    if (options.filePath != nullptr && options.filePath[0] != '\0') {
        if (fopen_s(&file, options.filePath, "ab") != 0 || file == nullptr) {
            callback = nullptr;
            callbackContext = nullptr;
            return Status::failure(ErrorCode::DiagnosticOpenFailed,
                                   "Unable to open the diagnostics file.");
        }
    }
    return Status::success();
}

void DiagnosticReporter::reset() noexcept
{
    std::lock_guard lock(mutex);
    if (file != nullptr) {
        std::fclose(file);
        file = nullptr;
    }
    callback = nullptr;
    callbackContext = nullptr;
}

void DiagnosticReporter::report(const DiagnosticSeverity severity, const ErrorCode code,
                                const std::string_view message) noexcept
{
    try {
        DiagnosticCallback activeCallback = nullptr;
        void* activeContext = nullptr;
        {
            std::lock_guard lock(mutex);
            if (file != nullptr) {
                const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                std::fprintf(file, "%lld,%s,%u,%.*s\n",
                             static_cast<long long>(milliseconds), severityName(severity),
                             static_cast<unsigned int>(code), static_cast<int>(message.size()),
                             message.data());
                std::fflush(file);
            }
            activeCallback = callback;
            activeContext = callbackContext;
        }
        if (activeCallback != nullptr) {
            activeCallback({ severity, code, message }, activeContext);
        }
    }
    catch (...) {
    }
}

}