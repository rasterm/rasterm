# rasterm Architecture

rasterm keeps the rendering library separate from the programs built with it. Code flows
toward the small public API:

```text
apps / external consumers
            |
            v
     include/rasterm       public, dependency free API
            |
            v
       src/engine          API implementation and ownership
            |
            v
damage | backend | encoder | render | output | platform
```

Only `include/rasterm/` is installed. Everything under `src/` is an implementation detail,
even though the folders have descriptive names instead of living under one large
`internal` directory.

## Workspace

```text
rasterm/
|-- include/rasterm/       Supported C++ API
|-- src/engine/            Engine and asynchronous Presenter implementations
|-- src/backend/           Private graphics backend interface
|   `-- sixel/             SIXEL backend adapter
|-- src/diagnostics/       File/callback diagnostics isolated from graphics output
|-- src/damage/            Pixel/palette change detection and region merging
|-- src/encoder/sixel/     Private SIXEL analysis, mapping, SIMD, and writing
|-- src/render/            Frame to terminal rendering orchestration
|-- src/output/            OutputSink implementations and terminal presentation
|-- src/platform/windows/  Windows console integration
|-- src/platform/linux/    Reserved Linux platform boundary and design notes
|-- apps/examples/         Small public API consumers
|-- validation/            Unified CMake validation kit
|   |-- tests/             Runtime, ABI, fuzz, and installed consumer verification
|   `-- benchmarks/        Reproducible performance corpus and baselines
`-- docs/                  User and implementation documentation
```

## Public Surface

- `Engine`: synchronous terminal ownership and rendering.
- `Presenter`: asynchronous latest frame submission.
- `FrameView`: borrowed RGB, BGR, RGBA, or BGRA pixels.
- `IndexedFrameView`: borrowed 8-bit indices and an exact caller palette.
- `EngineOptions` and `PresenterOptions`: rendering policy.
- `RenderStats` and `PresenterStats`: timing and frame results.
- `Status`, `ErrorCode`, and `TerminalCapabilities`: startup results and terminal support.
- `OutputSink`: sends finished output to a terminal, test buffer, or custom destination.
- `Extent`, `fitWithin`, and `calculateScaleLayout`: dependency free geometry.
- `scaleFrame`: dependency free packed pixel resampling into an owned RGB24 canvas.

Consumers may include the narrow headers they use or the `rasterm.hpp` umbrella.

## Runtime Pipeline

```text
packed pixels ----> normalize 32-bit layouts ---+
                                                   |
indexed pixels + exact palette --------------------+--> damage tracking
                                                        |
                                                        v
                                             private GraphicsBackend
                                                        |
                                             EncodeRequest / EncodedUpdate
                                                        |
                                                        v
                                                   SIXEL encoding
                                                        |
                                                        v
                                              synchronized presentation
                                                        |
                                                        v
                                                    OutputSink
```

Packed frames are quantized according to the selected quality profile. Indexed frames bypass
quantization: caller index `n` maps to SIXEL register `n + 1`, preserving the supplied RGB value.
Register zero is reserved below the limit, 256 entry palettes use all registers.

Damage tracking is byte width aware. Indexed palette changes are treated as full frame damage,
even if the index buffer is unchanged.

The synchronous engine borrows a frame until `renderFrame` returns. `Presenter` copies the
active rows and indexed palette into a one frame mailbox. If the terminal falls behind, a
new frame replaces the waiting frame instead of building a laggy queue.

## Application Boundary

`apps/rPlayer` owns decoding, audio, clocks, adaptive resolution decisions, and CSV metrics.
`apps/Termirror` owns DXGI capture, cursor composition, and capture damage translation. Apps
decide when and why to resize a source, the public `scaleFrame` helper provides the generic
dependency free pixel operation. Both call only public rasterm types. Protocol encoding,
terminal output, geometry, and presentation belong to the library. Application specific
policies do not.

## Dependency Policy

The `rasterm` target depends only on the C++ standard library and Windows system APIs. Public
headers expose neither Windows nor third party types. Only `include/rasterm/` is installed.

rasterm checks the console handle, dimensions, VT mode, stdout mode, and cancellation
handler before it starts rendering. If startup fails halfway through, or rendering later
stops because of an error or Ctrl+C, it restores the terminal settings it changed.

`GraphicsBackend` is private and decides how frames are encoded. The renderer submits generic
`EncodeRequest` intent (full or regional update and its byte ceiling) and receives an
`EncodedUpdate` as it never owns `SixelOptions`. `SixelBackend` translates the private backend
configuration into SIXEL policy and is the only 1.x implementation. Stage timings remain private
so palette analysis, mapping, writing, and budgeting can evolve without widening the public ABI.
`OutputSink` transports completed bytes and is not a protocol selection API.
Future protocols can add private backends without changing how applications submit frames.
Backend selection will become public only after multiple implementations show what that API
actually needs.
