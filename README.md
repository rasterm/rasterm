# rasterm 1.1.0

rasterm lets C and C++ programs draw real pixel frames in Windows Terminal. Give it a
packed or indexed framebuffer and it handles SIXEL encoding, terminal setup, frame
scheduling, and cleanup for you.

https://github.com/user-attachments/assets/1e39653c-9c40-4073-88be-a63bd653373f


```text
Application -> FrameView -> Engine/Presenter -> private backend -> OutputSink
```

## Features

- Dependency free static C++ library, optional shared C ABI, and stable C API v1
- Safe Rust and typed, buffer protocol first Python bindings
- Synchronous rendering or an asynchronous presenter that keeps only the newest frame
- RGB/BGR 24-bit, RGBA/BGRA 32-bit, RGB565, XRGB1555, and RGBA4444
- Exact caller provided indexed palettes with 1–256 colors
- Realtime, adaptive video, and high quality profiles
- Color metadata, SDR conversion, HDR to SDR tone mapping, and stable dithering
- Caller or internally determined damage rectangles
- Custom output sinks, structured errors, diagnostics, events, and metrics
- Safe console restoration after shutdown, cancellation, or partial startup
- Installable `rasterm::rasterm` CMake target and MinGW pkg config metadata

## Build and Install

Requirements: Windows x64, Visual Studio 2022 v143, CMake 3.24+, and Windows Terminal
1.22+ (1.23+ recommended).

```powershell
cmake -S . -B build/core -A x64 `
  -DRASTERM_BUILD_TESTS=ON `
  -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/core --config Release --parallel
ctest --test-dir build/core -C Release --output-on-failure
cmake --install build/core --config Release --prefix build/install
```

See [`docs/BUILD.md`](docs/BUILD.md) for clean package consumption and verified commands
for rasterm, rPlayer, SimpleNES, RetroArch, MinGW, benchmarks, and sanitizers.

## Minimal C++

```cpp
#include <rasterm/rasterm.hpp>

rasterm::Engine engine;
const rasterm::Status initialized = engine.initialize({ .useAlternateScreen = true });
if (!initialized) return 1;

rasterm::FrameView frame{
    pixels, width, height, strideBytes, rasterm::PixelFormat::RGBA32
};
const rasterm::RenderStats result = engine.renderFrame(frame);
```

Realtime producers should use `Presenter`; known palette producers should use
`IndexedFrameView`. Complete compiled C and C++ examples are in
[`apps/examples`](apps/examples/README.md).

## Repository Layout

```text
include/rasterm/        public C++ and C API
src/                    private engine implementation
apps/examples/          compiled minimal consumers
apps/rPlayer/           media application
bindings/               Rust and Python bindings over the C ABI
validation/tests/       correctness, ABI, consumer, stress, and fuzz validation
validation/benchmarks/  deterministic performance corpus and baselines
docs/                   public documentation
docs/internal/          engineering/integration material
```

## Documentation

- [API Contracts](docs/API.md)
- [Build and Installation](docs/BUILD.md)
- [Rust and Python Bindings](docs/BINDINGS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Compatibility and Versioning](docs/COMPATIBILITY.md)
- [Terminal Compatibility](docs/TERMINAL_COMPATIBILITY.md)
- [Color Behavior and Limitations](docs/COLOR.md)
- [Documentation Index](docs/DOCS.md)

## Current Limitations

- Windows x64 and Windows Terminal SIXEL are the only supported platform/backend.
- rasterm outputs at most 256 colors per image; HDR input is tone mapped to SDR.
- Capability detection is conservative and does not actively negotiate with a terminal.
- C++ consumers use the static library the optional DLL exposes only the stable C ABI.
  MSVC and MinGW artifacts are not interchangeable.
- Resolution and FPS depend on the scene, payload size, terminal dimensions, Windows
  Terminal version, and hardware. No terminal renderer can promise the same native
  resolution or frame rate for every workload.
- Kitty, iTerm2, native window, network, and recording backends are post 1.0 work. They
  will stay internal until rasterm has enough real implementations to design a useful
  shared API.

## Contributing and Security

See [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`SECURITY.md`](SECURITY.md). Contributions
are Apache-2.0 unless explicitly and validly marked otherwise.

Created by [MicREsoft](https://github.com/micREsoft).

## License

Apache License 2.0. See [`LICENSE`](LICENSE), [`NOTICE`](NOTICE), and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
