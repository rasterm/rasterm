# Changelog

All notable changes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and Semantic Versioning.

## [1.2.0] - 2026-08-11

### Added

- Bounded, frame-scoped SIXEL transactions with output limit and sink failure recovery.
- Presenter drain/invalidate APIs, complete accepted frame outcomes, and stable rejection status.
- Terminal capability and geometry overrides for custom and automatically detected output.
- Validation/conversion timing plus wire, scratch, and output capacity statistics.
- Corpus v5, randomized state machine coverage, and ASan/libFuzzer CI hardening.
- Periodically refreshed persistent SIXEL palettes, completed transaction chunking,
  independent regional quantization, a reusable parallel mapper, and AVX-512 dispatch.

### Changed

- Convert only supplied damaged regions when a compatible non-sRGB conversion cache exists.
- Validate copied and owned indexed Presenter frames once while retaining worker side validation
  for shared mutable input.
- Use a portable scalar damage comparison fallback outside x86 SSE2 builds.
- Document alpha bearing packed formats as ignoring alpha in the 1.2 rendering contract.

### Fixed

- Link Windows fuzz targets through the Clang driver with the required libFuzzer/ASan
  runtimes, and instrument the core library for coverage guided fuzzing.

## [1.1.0] - 2026-08-10

### Added

- Optional `rasterm.dll` exposing only the stable C API v1, with an installed
  `rasterm::shared` target and exact export baseline validation.
- Checked in `rasterm-sys` declarations and a safe Rust `rasterm` crate with RAII
  handles, packed/indexed frames, damage, color metadata, validation, and statistics.
- Typed Python bindings with context managed Engine/Presenter handles, type stubs,
  buffer protocol input, indexed palettes, damage, color metadata, and a self-contained
  Windows wheel build path.
- Experimental rastermUI framework using LVGL 9.5 software rendering
  and rasterm BGRA damage presentation, with input, resize, scrolling, themed controls,
  text input, examples, and a Kitchen Sink regression application.
- Private graphics backend boundary for future protocol implementations.
- Complete compiled C/C++ examples and authoritative API, lifetime, build, color,
  compatibility, binding, terminal, and realtime pipeline documentation.
- Corpus v3 benchmarks for full frame, damaged BGRA UI, indexed NES, packed 16-bit,
  static image, high motion, and low entropy motion surfaces.
- PowerShell entry points for reproducible rPlayer, SimpleNES, etc.
- Dedicated Rust/Python binding CI and binding sources in the versioned release ZIP.

### Changed

- Map packed 24-bit and 32-bit frames directly in the SIXEL encoder.
- Map default sRGB RGB565, XRGB1555, and RGBA4444 frames directly while retaining
  conversion for non sRGB color metadata.
- Normalize cell aligned caller damage and avoid full frame comparison copies for trusted damage.
- Coalesce nearby damage when cursor/frame overhead outweighs bounded area inflation,
  and promote sufficiently large damage to a full frame.
- Trim unused SIXEL color plane tails and redundant band carriage returns.
- Precompute active per color sixel masks once per band instead of rescanning source
  pixels for every palette color.
- Emit protocol numbers directly into the output buffer and split DECGRI repeats at
  Windows Terminal's 65,535 parameter limit.
- Clamp full height SIXEL frames with DECSDM and reject unsafe bottom edge patches to
  prevent terminal scrolling when a surface height is not divisible by six.
- Keep the synchronous Engine path allocation free after warmup and retain the
  Presenter's bounded latest frame mailbox instead of introducing queued latency.
- Preserve exact indexed palettes while extending direct packed mappings to every
  supported default-sRGB 16/24/32-bit format.
- Expand release validation across MSVC Debug/Release, clang-cl, MinGW64, installed C
  and C++ consumers, bindings, documentation, ABI layouts, and exported symbols.

### Fixed

- Prevent stale or duplicated terminal pixels when full height surfaces end on an
  incomplete six pixel band, including maximized rastermUI surfaces.
- Recover with a safe full frame when a bottom edge damage patch cannot be represented
  without scrolling.
- Split oversized DECGRI repeats so Windows Terminal never receives a parameter beyond
  its audited 65,535 limit.
- Preserve complete pixel, palette, metadata, and damage ownership across asynchronous
  C Presenter submissions.
- Make allocation failure validation deterministic so MSVC Debug CI cannot stall while
  exhaustively walking injected failures.
- Correct Rust and Python default frame color metadata to match the valid native sRGB
  and source defaults.
- Keep shared library declarations optin so existing static C and C++ consumers remain
  source and ABI compatible with 1.0.

### Performance

- Reduced the measured damaged BGRA UI memorysink path from 0.558 ms to 0.017 ms
  median by trusting normalized caller damage and avoiding comparison frame copies.
- Reduced representative high motion encode median from about 27.1 ms to 8.2 ms by
  building active per color masks once per SIXEL band.
- Reduced indexed NES encode median from about 2.4 ms to 0.6 ms while keeping payloads
  byte for byte equivalent and steady state allocation count at zero.
- Rejected tile hashing, encoded region caching, and slower packed-16 AVX2 mapping after
  measurement showed no safe improvement over trusted damage and scalar direct mapping.

## [1.0.0] - 2026-08-02

### Added

- Dependency free Windows x64 raster graphics engine with a synchronous C++ `Engine`
  and opaque handle C API v1.
- Capacity one asynchronous `Presenter` that copies submissions, replaces stale waiting
  frames, bounds memory, and exposes thread safe statistics.
- RGB24, BGR24, RGBA32, BGRA32, RGB565, XRGB1555, RGBA4444, and exact indexed palette
  input with explicit positive byte strides.
- Realtime, adaptive video, and high quality profiles with deterministic palette policy.
- Color primaries, transfer functions, matrix/range conversion, sRGB output conversion,
  HDR to SDR tone mapping, ordered/error diffusion dithering, and palette locking.
- Automatic or caller supplied damage rectangles with first frame and reset recovery.
- Transactional SIXEL generation, synchronized output, alternate screen handling,
  cursor preservation, payload limits, and all or nothing output sinks.
- Terminal capability and geometry reporting, structured status/error handling,
  diagnostics, events, render metrics, and two call UTF-8 C diagnostics.
- Cancellation safe Windows console restoration, process control handling, and exclusive
  terminal ownership across Engine/Presenter instances.
- Extensible versioned C structures, initializer functions, ABI version query, enum and
  layout fixtures, and stable static library symbol validation.
- Installable `rasterm::rasterm` CMake package, pkg-config metadata, component packaging,
  and clean C11/C++20 consumer projects.
- MSVC Debug/Release, clang-cl, and MinGW64 CI with warnings as errors, install/package
  checks, sanitizers, allocation failure injection, concurrency/stress tests, and fuzzers.
- Microsoft derived SIXEL parser harness, golden round trip fixtures, color conformance
  references, protocol recovery tests, and deterministic benchmark corpus.
- rPlayer image/video/audio application, SimpleNES indexed framebuffer integration, and
  RetroArch rasterm video driver reference integration.
