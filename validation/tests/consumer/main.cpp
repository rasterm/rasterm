/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <string>

class MemorySink final : public rasterm::OutputSink {
public:
    bool write(const std::string_view bytes) noexcept override
    {
        output.append(bytes);
        return true;
    }

    bool flush() noexcept override { return true; }

    std::string output;
};

int main()
{
    MemorySink output;
    rasterm::Engine engine;
    if (!engine.initialize({ .output = &output })) {
        return 1;
    }
    const unsigned char pixel[] = { 255, 0, 0 };
    const auto result = engine.renderFrame(pixel, 1, 1, 3, rasterm::PixelFormat::RGB24);
    return result.rendered && !output.output.empty() ? 0 : 2;
}