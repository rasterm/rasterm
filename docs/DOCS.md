# rasterm Documentation

rasterm is a frame presentation library: your program supplies pixels, and rasterm gets
them onto the terminal. The public C and C++ APIs do not expose SIXEL, even though SIXEL
is the Windows Terminal backend used in rasterm 1.x.

## Start Here

- [`README.md`](../README.md) — overview, quick build, and current limitations
- [`BUILD.md`](BUILD.md) — source builds, installation, consumers, and applications
- [`API.md`](API.md) — ownership, threading, callbacks, and error handling
- [`/apps`](../apps/README.md) — compiled minimal C and C++ programs

## Public Contracts

- [`BINDINGS.md`](BINDINGS.md) - Rust and Python APIs, builds, ownership, and packaging

- [`COMPATIBILITY.md`](COMPATIBILITY.md) — semantic versioning, static library policy,
  compiler/runtime boundary, and C ABI v1
- [`C_API.md`](C_API.md) — C specific usage and structure/version rules
- [`LIFETIMES.md`](LIFETIMES.md) — focused threading and buffer lifetime guide
- [`TERMINAL_COMPATIBILITY.md`](TERMINAL_COMPATIBILITY.md) — supported Windows Terminal
  lines and conservative capability detection
- [`COLOR.md`](COLOR.md) — color conversion, perceptual limits, dithering, palettes, HDR,
  and 256-color constraints

## Engineering and Validation

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — dependency boundaries and private backend seam
- [`REALTIME_PIPELINE.md`](REALTIME_PIPELINE.md) — clock correct asynchronous presentation
- [`PERSISTENT_PALETTES.md`](PERSISTENT_PALETTES.md) — palette register behavior study

## Integrations

- [`bindings`](../bindings/) - safe Rust and typed Python bindings over C API v1
- [`apps/rPlayer`](../apps/rPlayer/README.md) — image/video/audio application
- [`apps/Termirror`](../apps/Termirror/README.md) — read only DXGI desktop/window region mirror
- [`apps/SimpleNES`](../apps/SimpleNES/README.md) — native indexed/software framebuffer

## Project Policy

- [`CONTRIBUTING.md`](../CONTRIBUTING.md)
- [`SECURITY.md`](../SECURITY.md)
- [`LICENSE`](../LICENSE), [`NOTICE`](../NOTICE)

Only declarations under `include/rasterm/` are public. The media player, emulator
integrations, protocol code, tests, benchmarks, and Microsoft parser snapshot are useful
project material, but they are not part of the library API.
