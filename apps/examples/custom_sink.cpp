/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <cstdint>
#include <string>

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        try {
            output.append(bytes);
            return true;
        }
        catch (...) {
            return false;
        }
    }
    bool flush() noexcept override { return true; }
    std::string output;
};

int main()
{
    MemorySink sink;
    rasterm::Engine engine;
    if (!engine.initialize({ .output = &sink })) return 1;

    constexpr std::uint8_t red[] = { 255, 0, 0 };
    const rasterm::RenderStats stats = engine.renderFrame(
        red, 1, 1, 3, rasterm::PixelFormat::RGB24);
    return stats.error == rasterm::ErrorCode::None && !sink.output.empty() ? 0 : 2;
}