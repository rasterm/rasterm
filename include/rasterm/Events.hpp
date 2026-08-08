/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Error.hpp>
#include <rasterm/Metadata.hpp>
#include <rasterm/Statistics.hpp>

namespace rasterm {

enum class EventType : std::int32_t {
    TerminalResized = 0,
    UnsupportedCapability = 1,
    FrameDropped = 2,
    OutputFailure = 3,
};

struct Event {
    EventType type = EventType::OutputFailure;
    ErrorCode error = ErrorCode::None;
    TerminalGeometry geometry{};
    std::uint64_t frameId = 0;
    std::int64_t timestampNanoseconds = unknownTimestamp;
};

using EventCallback = void (*)(const Event& event, void* context) noexcept;

struct EventOptions {
    EventCallback callback = nullptr;
    void* context = nullptr;
};

}