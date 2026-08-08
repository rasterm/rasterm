# rasterm 1.0 API Reference

This page explains how to use every public C++ type under `include/rasterm/`, including
who owns each buffer, which calls are thread safe, where callbacks run, and how errors
are returned. The C ABI is summarized near the end and defined in
[`capi.h`](../include/rasterm/capi.h). Code under `src/` and `apps/` is not public API.

## Core Rules

* rasterm 1.0 renders frames inside Windows Terminal. You supply the pixel data, and the library handles protocol encoding internally.
* `Engine` is synchronous and single threaded. `Presenter` is thread safe and handles asynchronous rendering with a queue depth of one.
* Views are immutable and borrowed. Byte strides must be positive (top down).
* Only one active `Engine` or `Presenter` can write to standard output (`stdout`) at a time per process. Trying to open a second default output instance will fail. However, you can create multiple independent instances if you supply custom output sinks.
* Public functions do not throw C++ exceptions across the C ABI. C++ initialization and state failures are reported using `Status`, frame rendering failures use `RenderStats::error`, and frame submission returns `false` (with details available via `Presenter::status()`).
* A custom `OutputSink` handles byte transport, not terminal graphics encoding. It receives finalized byte streams and must remain allocated for the lifetime of its parent engine.

## Lifecycle Classes

### `Engine`

A move only renderer for one thread. Call `initialize()` before rendering. Call
`shutdown()` when finished, or let the destructor clean up automatically.

You must call `initialize`, rendering functions, `clear`, `reset`, queries, and `shutdown` from the same thread. Any borrowed frame data and metadata passed to `renderFrame()` must stay valid until that call returns. Call `status()` to inspect the last failure, or `stats()` to check render metrics.

### `Presenter`

A thread safe wrapper around `Engine` with its own worker thread. Multiple threads may
call `submit*`, `stats`, `status`, and `shutdown`.

It holds at most one waiting frame. If the worker falls behind, the newest submission
replaces that waiting frame and increments `replacedFrames`. The producer does not wait
for the terminal to finish drawing.

* `submit(FrameView)` and `submit(IndexedFrameView)` copy active pixel rows, palettes, damage rects, and metadata references before returning.
* `submit(OwnedFrame&&)` and `submit(OwnedIndexedFrame&&)` move ownership into the presenter.
* `submitShared(...)` avoids copying pixel data, but you must pass a lifetime token that keeps the underlying memory valid until presentation finishes.
* Worker callbacks can call read only query functions. They must never initialize, move, shut down, destroy, or mutate the `Presenter` instance.

## Frames and Memory Ownership

| Type | Contract |
| --- | --- |
| `FrameView` | Borrowed packed pixels. Checks data pointers, dimensions, formats, row arithmetic, positive strides, and damage boundaries. |
| `IndexedFrameView` | Borrowed 8-bit index buffer paired with a 1–256 entry palette. Every active index must fall within the palette range. RGB values are written directly. |
| `PaletteView` | Borrowed span of `RgbColor` entries (must be non null, 1–256 items). |
| `RgbColor` | Simple struct holding 8-bit red, green, and blue values. |
| `FrameMetadata` | Metadata values holding a borrowed `DamageView`. Frame IDs and timestamps are preserved as is without interpretation. |
| `DamageRect` | Pixel bounds for updated regions inside a frame. Checks prevent integer overflow. |
| `DamageView` | Borrowed list of damage rectangles. Set `supplied=false` to let rasterm compute diffs automatically, set `supplied=true` with a count of `0` to indicate nothing changed. |
| `OwnedFrame` | Owns its packed pixel buffer and damage rect list. `view()` returns a borrowed view. |
| `OwnedIndexedFrame` | Owns its index buffer, palette, and damage rects. `view()` returns a borrowed view. |
| `SharedFrameView` | Borrowed packed view tied to a shared lifetime token (`std::shared_ptr`). |
| `SharedIndexedFrameView` | Borrowed indexed view tied to a shared lifetime token (`std::shared_ptr`). |

Supported `PixelFormat` types: `RGB24`, `BGR24`, `RGBA32`, `BGRA32`, `RGB565`, `XRGB1555`, and `RGBA4444`. Alpha channels are ignored. Packed 16-bit formats assume little endian byte ordering. Unrecognized formats fail validation via `isValidPixelFormat()` and `bytesPerPixel()`.

## Configuration and Color Settings

| Type | Contract |
| --- | --- |
| `EngineOptions` | Copied on initialization. Callback contexts, file path strings, event contexts, and `output` pointers must remain valid for the lifetime of the engine. |
| `PresenterOptions` | Copied on initialization. Set `maximumFramesPerSecond=0` for uncapped frame rates (negative or non finite numbers fail validation). |
| `QualityProfile` | Select from `Realtime`, `AdaptiveVideo`, or `HighQuality`. |
| `ColorMetadata` | Defines color space specs (primaries, transfer functions, matrices, reference whites, and mastering peaks). Input pixels are processed through these settings before conversion to sRGB output. |
| `ColorOptions` | Configures tone mapping, dithering, and color management policies inside the engine. |
| `ColorPrimaries` | `Unspecified`, `Bt709`, `Bt2020`, `DisplayP3`. |
| `TransferFunction` | `Unspecified`, `Srgb`, `Linear`, `Bt709`, `Gamma22`, `Pq`, `Hlg`. |
| `MatrixCoefficients` | `Unspecified`, `Identity`, `Bt601`, `Bt709`, `Bt2020NonConstant`. |
| `ColorRange` | `Unspecified`, `Limited`, `Full`. |
| `ToneMapOperator` | `None`, `Reinhard`, `Hable`, `Aces`. |
| `DitherMode` | `None`, `OrderedBayer4x4`, or `FloydSteinberg`. |

For specific color transformation details and math assumptions, see [`COLOR.md`](COLOR.md).

## Output, Logging, and Errors

### `OutputSink`

A custom byte transport class. `write()` and `flush()` execute on whatever thread calls the Engine, or on the Presenter worker thread, so they must be thread safe.

`write()` calls are all or nothing: returning `false` means no bytes were written. If an implementation hits a partial OS write, it must finish writing the remaining bytes before returning `true`. These methods are marked `noexcept`, returning `false` triggers internal engine error states.

### Diagnostics and Events

`DiagnosticOptions` holds a borrowed file path, callback pointer, and context pointer. `DiagnosticEvent` provides a severity level, error code, and a string message that is only valid for the duration of the callback. Diagnostics are kept separate from the terminal output stream.

`EventOptions` accepts a callback pointer and context. `Event` structures report terminal resizes, unsupported capabilities, dropped frames, and write failures.

Callbacks run synchronously on whatever thread detects the issue (including the background Presenter thread), are marked `noexcept`, and must not store references to any borrowed views passed into them.

`ErrorCode` values match across the C++ and C APIs. `Status` objects evaluate to `true` only when the code is `ErrorCode::None`. `RenderStats::error` holds the result of the last render operation; valid frames that haven't changed since the last render will set `rendered=false` without setting an error.

## Capabilities, Geometry, and Helpers

| Type | Contract |
| --- | --- |
| `CapabilitySupport` | Three state enum: `Unknown`, `Unsupported`, `Supported`. |
| `TerminalCapabilities` | A snapshot of output features, VT status, and geometry. Custom sinks report capability support as `Unknown`. |
| `TerminalGeometry` | Character cell and pixel counts. Call Engine queries after a resize event to refresh these values. |
| `RenderStats` | Render timing, payload metrics, and frame drop counts. Note: terminal write time measures output transmission, not the terminal's actual display rendering time. |
| `PresenterStats` | Thread safe snapshot of submitted, rendered, and dropped frame counts. |
| `Version` | Semantic versioning info. `version` and `abiVersion` are compile time constants. |
| `Extent`, `Rect` | Geometry value types. |
| `ScalePolicy`, `ScaleFilter` | Layout enums. rasterm calculates layouts using these types, but does not scale image data itself. |
| `ScalingOptions`, `ImageOptions` | Policy options for application side scaling. |
| `ScaleLayout` | Calculated output bounds for source, target, and canvas areas. |

`fitWithin()` and `calculateScaleLayout()` are thread safe calculation functions. Passing invalid values returns an empty result.

## C ABI (v1)

Include `<rasterm/capi.h>`. Handle types (`rasterm_engine` and `rasterm_presenter`) are opaque pointers that must be freed with their corresponding destroy functions.

Every struct containing a `struct_size` field must be initialized with its `*_init` function before use. Passing incorrect struct sizes or unrecognized version flags will fail. Reserved fields must be initialized to zero.

Memory ownership rules match the C++ API: `rasterm_engine_render*` borrows memory synchronously, while `rasterm_presenter_submit*` copies data before returning. Memory passed to write callbacks is only valid during the callback execution. Do not destroy handles from inside their own callbacks.

| C Type | Description |
| --- | --- |
| `rasterm_engine`, `rasterm_presenter` | Opaque handle pointers. Destroy with their matching function (passing NULL is safe). |
| `rasterm_result` | Error codes mapping directly to C++ `ErrorCode` values. |
| `rasterm_pixel_format`, `rasterm_quality_profile`, `rasterm_capability_support` | Enum types. Unrecognized values cause initialization failures. |
| `rasterm_color_primaries`, `rasterm_transfer_function`, `rasterm_matrix_coefficients`, `rasterm_color_range`, `rasterm_tone_map_operator`, `rasterm_dither_mode` | Enums matching the C++ color options. |
| `rasterm_write_callback`, `rasterm_flush_callback` | Context pointers that run on the caller or Presenter thread. Write callbacks are all or nothing, and byte pointers expire when the function returns. |
| `rasterm_color_metadata`, `rasterm_damage_rect`, `rasterm_rgb_color` | Plain structs containing no pointer ownership. |
| `rasterm_frame_metadata` | Value fields and a borrowed array of `damage_rects`. `Engine` borrows these; `Presenter` copies them on submission. |
| `rasterm_engine_options`, `rasterm_presenter_options` | Configuration structs. Callback contexts must stay allocated until the parent handle is destroyed. |
| `rasterm_frame` | Borrowed pixel memory with positive stride. |
| `rasterm_indexed_frame` | Borrowed index buffer and palette memory. |
| `rasterm_render_stats`, `rasterm_terminal_capabilities`, `rasterm_presenter_stats` | Output structs populated up to the known ABI struct size. |

Functions return `rasterm_result` codes. To fetch detailed error strings, call `rasterm_engine_last_error`, `rasterm_presenter_last_error`, or `rasterm_last_error`. Pass a NULL buffer first to get the required string length.

API exports, structs, and function signatures are fixed in [`capi.h`](../include/rasterm/capi.h) and validated via tests in `validation/tests/abi/`.

## Examples

Example code is available in [`apps/examples`](../apps/examples/):

* C API usage
* Synchronous C++ usage
* Async `Presenter` setups
* Palette based (indexed) rendering
* Custom `OutputSink` implementations
* Damage rect usage
* Color space configuration

Enable examples in your build configuration with `-DRASTERM_BUILD_EXAMPLES=ON`.

## Limitations

* **Platforms:** Windows x64 using Windows Terminal with SIXEL enabled is the only supported target for 1.0.
* **Console Handle:** Default output requires a direct console handle. Use a custom output sink if you need ConPTY capture or standard output redirection.
* **Capabilities:** Detection relies on environment variables and handle inspection rather than active terminal query negotiation.
* **Palette Limits:** Output is capped at 256 colors per SIXEL image. HDR inputs are tone mapped down to SDR.
* **Scaling:** Layout calculations are supported, but resampling/scaling pixels must be done by the caller.
* **Distribution:** 1.0 ships as static libraries only. See [`COMPATIBILITY.md`](COMPATIBILITY.md).
