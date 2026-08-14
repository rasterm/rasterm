# Changelog

All notable changes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and Semantic Versioning.

## [Unreleased]

## [1.3.0] - 2026-08-14

### Added

- Dependency free packed pixel resampling with fit, fill, stretch, integer, crop, and no scale
  layouts. Nearest, linear, cubic, and area filtering. Background fill, contrast, and sharpening.
- Safe tightly packed frame helpers and span based palette construction while preserving the
  aggregate zero overhead frame views.
- A canonical installed C defaults table with C, C++, Rust, and Python conformance coverage.
- A public API ownership, threading, error, and compatibility inventory plus a runnable callback
  reentrancy and custom output capability override example.
- Independent first and only include compilation for every installed C and C++ header.
- Private protocol independent `EncodeRequest` and `EncodedUpdate` contracts between renderer
  policy and graphics backends, without widening the public API or C ABI.
- Private nanosecond stage measurements for palette analysis, palette mapping, SIXEL writing,
  and output budget commit work.
- Deterministic Microsoft compatible decode fixtures for gradients, skin tones, dark scenes,
  UI text, and high motion content.
- Seeded randomized damage properties covering 20,000 cell alignment, clipping, merging,
  bottom row, arbitrary cell size, and integer boundary cases.
- Focused benchmark tools for scalar/AVX2/AVX-512 dispatch, palette lookup compression, and
  full frame versus regional encoding from small through high DPI surfaces.

### Changed

- Organize private protocol code under `src/encoder/sixel` and `src/backend/sixel`, generic
  resampling under `src/scaling`, and platform selected sources under `src/platform`.
- Consolidate correctness tests, ABI checks, fuzzers, installed header checks, and benchmarks
  behind one `validation/CMakeLists.txt` kit while retaining isolated test executables.
- Keep renderer policy protocol neutral by translating quality and encoder tuning into private
  `SixelOptions` only inside `SixelBackend`.
- Replace area threshold presentation heuristics with a measured cost model for estimated SIXEL
  bytes, cursor positioning, palette definitions, synchronized output, and shared versus
  independent regional palettes.
- Formalize persistent palette ownership: another terminal graphics producer requires
  `Engine::reset()` or `Presenter::invalidate()` before rasterm presents again.
- Retain the existing 32 KiB fixed palette lookup after the measured 4 KiB candidate changed
  20-100% of palette selections for only a modest cache improvement.

### Fixed

- Restore high quality still-image fidelity with 255-color perceptual palettes, palette aware
  Floyd Steinberg diffusion, and fine nearest palette mapping instead of quantizing through an
  unrelated coarse RGB grid. Add photograph sized channel bias and RGB/BGR equivalence regressions.
- Make output limit failures fully transactional by discarding private partial output, restoring
  palette register state, and emitting no cursor, synchronization, palette, or SIXEL bytes.
- Prevent cell alignment and region cost arithmetic from overflowing near integer boundaries.
- Clip partially out of bounds internal damage safely and sanitize invalid renderer cell sizes.
- Preserve separate distant regions and merge nearby regions according to actual backend palette
  reuse instead of assuming every region pays for an independent palette.

### Performance

- Keep scalar fixed palette mapping as automatic dispatch after isolated Release measurements
  found no stable AVX2 crossover through width 1920 on the tested system, forced SIMD modes remain
  available to the internal benchmark for hardware specific research.
- Measure cell aligned 10% regional updates at roughly 0.005, 0.025, 0.090, and 0.156 ms median on
  320x180, 800x450, 1920x1080, and 2800x1400 low entropy memory sink surfaces respectively.
- Preserve the allocation free warmed public path and keep all new stage metrics private.

## [1.2.0] - 2026-08-12

### Added

- Frame scoped SIXEL transactions that select damage and palette policy once, encode each
  selected region once, stage the complete update, and only then write it to the sink.
- Checked aggregate output accounting that enforces `maximumOutputBytes` while constructing
  a transaction and rejects oversized frames before writing any frame bytes.
- Bounded transaction chunking through `EncoderTuning::outputChunkBytes`, including balanced
  DCS termination after a partially accepted chunk and safe full frame recovery afterward.
- Persistent SIXEL palette registers with configurable periodic refresh and conservative
  invalidation after reset, clear, output rejection, write failure, or flush failure.
- Independent adaptive quantization for damaged regions, avoiding redundant full frame palette
  analysis and reducing palette traffic for localized updates.
- A reusable bounded row mapping worker pool with configurable thread count and a deterministic
  `maximumThreads = 1` baseline.
- Runtime fixed palette dispatch across byte identical scalar, AVX2, and AVX-512 implementations.
- `Presenter::waitUntilIdle` and `rasterm_presenter_wait_until_idle` for bounded draining through
  sink acceptance without claiming terminal parser or compositor completion.
- `Presenter::invalidate` and `rasterm_presenter_invalidate` for worker ordered invalidation that
  forces the next accepted presentation to be a full frame.
- Complete Presenter accounting for presented, replaced, unchanged, failed, rejected, and
  shutdown cancelled frames, with exactly one non rejected outcome for every accepted frame.
- Stable invalid, stopped, and allocation failure rejection status plus metadata for the frame
  actually replaced in the capacity one mailbox.
- `TerminalOverrides` and matching additive C options for explicit SIXEL support,
  synchronized output support, and terminal cell/pixel geometry overrides.
- `EncoderTuning` controls for palette persistence, refresh cadence, output chunk size, worker
  count, and independent regional quantization.
- Render statistics for validation and conversion time, total wire bytes, reusable scratch
  bytes, output buffer capacity, and the expanded Presenter outcome counters.
- Corpus v4 coverage for adaptive multi region transactions, bounded writer rejection,
  output ceilings, non-sRGB regional conversion, and copied/owned/shared indexed submissions.
- Corpus v5 coverage for palette reuse, single thread and automatic mapping, chunked and
  unchunked sinks, and independent versus global regional palette selection.
- Randomized rendering state machine coverage across recovery, invalidation, representation
  changes, damage, palettes, output limits, and sink failures.
- Bounded seeded libFuzzer smoke execution for frame validation, damage rectangles, C structures,
  and SIXEL writing, with failure artifacts retained by CI.

### Changed

- Reset both damage and palette knowledge after any output limit, write, or flush failure so the
  next accepted render cannot depend on uncertain terminal state.
- Separate encoded SIXEL payload bytes from all bytes accepted by the output sink, including
  cursor movement, synchronization, protocol framing, and recovery traffic.
- Clear stale Presenter rejection status after a later successful operation while preserving
  cumulative outcome and failure counters.
- Keep immediate shutdown bounded by cancelling waiting work and finishing only an in flight sink
  call; drain then shutdown is now an explicit caller decision.
- Validate synchronous indexed input once before private rendering and trust validated copied or
  owned Presenter storage while continuing to revalidate shared mutable input on the worker.
- Cache compatible non sRGB converted pixels and update only trusted caller supplied damage, while
  forcing full conversion after reset, shape, format, color policy, or damage mode changes.
- Treat packed/indexed representation and indexed palette changes conservatively so supplied
  damage cannot preserve stale terminal pixels.
- Use a portable scalar automatic damage comparison path when x86 SSE2 is unavailable.
- Preserve the allocation free warmed synchronous realtime path while adding persistent palettes,
  transactional staging, regional conversion, and parallel mapping.
- Extend the C API v1 through trailing structure fields and additive functions while preserving
  its existing prefix layouts, enum values, calling convention, and exported symbols.
- Update the Python `rasterm` package and Rust `rasterm`/`rasterm-sys` crates to 1.2.0 metadata,
  native version checks, and the expanded additive C ABI structure layouts.
- Make the compiled C and C++ examples a standalone CMake consumer with artifacts under
  `apps/examples/build`, leaving the root build tree for the engine, installation, bindings,
  benchmarks, and validation.
- Keep the root `vcpkg.json` dependency free and scoped to the rasterm core rather than optional
  multimedia application dependencies.
- Update build scripts, CI commands, documentation, and Rust local library discovery for the
  application-local build layout.
- Document RGBA32, BGRA32, and RGBA4444 as encoding stored RGB while ignoring alpha so rasterm 1.2
  does not infer straight or premultiplied alpha or perform alpha compositing.

### Fixed

- Prevent output limit rejection from leaking cursor movement, synchronization, palette, or SIXEL
  bytes from an incomplete logical frame.
- Recover damage, conversion cache, and palette state after partial sink acceptance so a later
  frame cannot reuse terminal state that may not have arrived.
- Keep chunk failure recovery protocol balanced by terminating an open DCS whenever the sink can
  still accept the recovery terminator.
- Ensure adaptive palette decisions advance once per submitted logical frame instead of once per
  damaged region.
- Ensure every accepted Presenter submission is accounted for under replacement, render failure,
  invalidation, drain, and concurrent shutdown.
- Preserve accumulated damage when a compatible pending regional frame is replaced in the
  Presenter's capacity one mailbox.
- Avoid redundant indexed validation for immutable copied/owned submissions without weakening the
  validation contract for caller owned shared storage.
- Link Windows fuzz targets through the Clang driver with the required libFuzzer/ASan
  runtimes, deploy the dynamic ASan runtime beside each executable, and instrument the
  core library for coverage guided fuzzing.
- Remove core test and benchmark targets that referenced external rPlayer or historical Windows
  Terminal research sources, restoring clean ASan, MSVC, clang-cl, and MinGW configuration from
  a core only checkout.
- Keep optional tests and benchmarks disabled by default in the PowerShell library build and avoid
  attempting to execute targets that were not requested.

### Performance

- Reuse unchanged palette register definitions across frames instead of resending the complete
  palette for every compatible update, with bounded refresh to prevent unbounded state reliance.
- Limit non sRGB conversion work to compatible supplied damage rectangles instead of repeatedly
  converting the complete surface.
- Quantize independent damaged regions locally when enabled instead of scanning the full frame for
  every regional update.
- Reuse worker threads and mapping scratch storage across frames instead of introducing per frame
  thread construction or steady state allocations.
- Add AVX-512 fixed palette mapping while retaining runtime AVX2 and scalar fallbacks and
  byte identical encoded output.
- Retain small work synchronous execution to avoid parallel scheduling overhead where measurement
  does not justify it.

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
  supported default sRGB 16/24/32-bit format.
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
