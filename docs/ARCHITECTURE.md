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
|-- src/backend/           Private graphics backend interface and SIXEL adapter
|-- src/diagnostics/       File/callback diagnostics isolated from graphics output
|-- src/damage/            Pixel/palette change detection and region merging
|-- src/encoder/           Palette mapping, dithering, and SIXEL generation
|-- src/render/            Frame to terminal rendering orchestration
|-- src/output/            OutputSink implementations and terminal presentation
|-- src/platform/windows/  Windows console integration
|-- apps/rPlayer/          Optional media player using the public API
|-- apps/examples/         Small public API consumers
|-- validation/tests/      Runtime, ABI, fuzz, and installed-consumer verification
|-- validation/benchmarks/ Reproducible performance corpus and baselines
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
- `Extent` and `fitWithin`: dependency free geometry.

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

`apps/rPlayer` owns decoding, scaling, audio, clocks, adaptive resolution, and CSV metrics. It
calls only public rasterm types. Protocol encoding, damage detection, terminal output, geometry,
and presentation belong to the library. Media specific policies do not.

## Dependency Policy

The `rasterm` target depends only on the C++ standard library and Windows system APIs. Public
headers expose neither Windows nor third party types. Only `include/rasterm/` is installed.

rasterm checks the console handle, dimensions, VT mode, stdout mode, and cancellation
handler before it starts rendering. If startup fails halfway through, or rendering later
stops because of an error or Ctrl+C, it restores the terminal settings it changed.

`GraphicsBackend` is private and decides how frames are encoded; `SixelBackend` is the only 1.0
implementation. `OutputSink` transports completed bytes and is not a protocol selection API.
Future protocols can add private backends without changing how applications submit frames.
Backend selection will become public only after multiple implementations show what that API
actually needs.
