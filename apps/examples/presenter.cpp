/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

int main()
{
    constexpr int width = 256;
    constexpr int height = 240;
    std::vector<std::uint8_t> pixels(width * height * 4, 255);

    rasterm::Presenter presenter;
    const rasterm::Status status = presenter.initialize({
        .engine = { .useAlternateScreen = true },
        .maximumFramesPerSecond = 60.0,
    });
    if (!status) return 1;

    for (int frameId = 0; frameId < 120; ++frameId) {
        pixels[0] = static_cast<std::uint8_t>(frameId * 2);
        rasterm::FrameView frame{
            pixels.data(), width, height, width * 4, rasterm::PixelFormat::RGBA32,
            { .frameId = static_cast<std::uint64_t>(frameId) },
        };
        if (!presenter.submit(frame)) return 2;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return 0;
}