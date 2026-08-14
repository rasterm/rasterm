# Rasterm Public API Inventory

This inventory covers installed declarations under `include/rasterm/`. It is a compatibility
map, not a replacement for [`API.md`](API.md). “C ABI” means the concept has a stable equivalent
in `capi.h`, C++ ABI is not promised across toolchains or runtime library modes.

## Lifecycle and Output

| API | Ownership | Threading | Failure reporting | Compatibility |
| --- | --- | --- | --- | --- |
| `Engine` | Owns terminal session and private renderer, borrows callback contexts and `OutputSink` | One caller thread for lifecycle, rendering, and queries | `Status`, `RenderStats::error` | Public C++ 1.x, stable C ABI handle |
| `Presenter` | Owns an `Engine` and worker, copies, moves, or shares submitted frames according to overload | Submission/query operations are thread safe, lifecycle cannot run from worker callbacks | `Status`, `bool`, `PresenterStats` | Public C++ 1.x, stable C ABI handle |
| `OutputSink` | Caller owned for the engine lifetime, encoded byte views expire when `write()` returns | Called on the engine caller or presenter worker | `false` creates a stable output failure | Public C++ 1.x, C ABI callback equivalent |
| `Status`, `ErrorCode` | Value types | Safe to copy between threads | Explicit code and message | Public C++ 1.x, error codes fixed in C ABI v1 |

## Frame and Buffer Types

| API | Ownership | Threading | Validation/errors | Compatibility |
| --- | --- | --- | --- | --- |
| `FrameView` | Borrows packed pixels and metadata damage | Immutable view, caller coordinates access | `isValid()`, render returns `RenderStats` | Public C++ 1.x, stable C ABI struct |
| `IndexedFrameView`, `PaletteView` | Borrow indices, palette, and damage | Immutable view, caller coordinates access | `isValid()`, `hasValidIndices()` | Public C++ 1.x, stable C ABI structs |
| `OwnedFrame`, `OwnedIndexedFrame` | Own pixels/palette/damage | Movable, not internally synchronized | `isValid()` | Public C++ 1.x |
| `SharedFrameView`, `SharedIndexedFrameView` | Borrow data through a shared lifetime token | Token controls lifetime, not producer mutation | `isValid()` and presenter rejection | Public C++ 1.x |
| `FrameMetadata`, `DamageView` | Value metadata plus borrowed damage span | Immutable during a render/submission | Frame validation | Public C++ 1.x, stable C ABI equivalents |
| `FrameView::tightlyPacked()` | Borrows pixels and calculates a safe packed stride | Same as `FrameView` | Empty view on invalid dimensions/overflow | Additive C++ 1.x helper |
| `IndexedFrameView::tightlyPacked()`, `PaletteView::from()` | Borrow indices/palette | Same as indexed views | Existing view validation | Additive C++ 1.x helpers |
| `Engine::renderFrame(data, width, height, stride, format)` | Borrows packed pixels for the call | Same as `Engine` | `RenderStats::error`, delegates to frame validation | Public C++ 1.x overload, prefer `FrameView` |

## Configuration and Capabilities

| API | Ownership | Threading | Validation/errors | Compatibility |
| --- | --- | --- | --- | --- |
| `EngineOptions`, `PresenterOptions` | Copied, contained pointers/contexts remain caller owned | Configure before initialization | `initialize()` validates values | Public C++ 1.x, stable C ABI prefixes |
| `ColorOptions`, `ColorMetadata` | Values | Safe to copy | Invalid numeric values reject initialization/frame | Public C++ 1.x, stable C ABI fields |
| `EncoderTuning` | Value policy, no protocol objects exposed | Configure before initialization | `initialize()` | Public C++ 1.x, stable C ABI fields |
| `TerminalOverrides` | Caller declared capabilities/geometry | Configure before initialization | Contradictory geometry is rejected | Public C++ 1.x, stable C ABI fields |
| `TerminalCapabilities`, `TerminalGeometry` | Returned snapshots | Query on owning engine thread, presenter snapshots are synchronized | Capability states may remain `Unknown` | Public C++ 1.x, stable C ABI output struct |
| `defaults.h` | Compile time constants | N/A | Cross language conformance tests | Additive C/C++ 1.x contract |

## Scaling and Geometry

| API | Ownership | Threading | Validation/errors | Compatibility |
| --- | --- | --- | --- | --- |
| `Extent`, `Rect`, `ScaleLayout` | Values | Pure/thread safe | Empty result for invalid inputs | Public C++ 1.x |
| `calculateScaleLayout()`, `fitWithin()` | No retained state | Pure/thread safe | Empty result for invalid/overflowing geometry | Public C++ 1.x |
| `ScalingOptions`, `ImageOptions` | Values | Safe to copy | Validated by `scaleFrame()` | Public C++ 1.x |
| `scaleFrame()` | Borrows source for the call, returns owned RGB24 pixels | Reentrant, no shared state | Empty `OwnedFrame` for invalid frame, bounds, or options | Additive C++ 1.x helper |

## Diagnostics, Events, and Statistics

| API | Ownership | Threading | Failure reporting | Compatibility |
| --- | --- | --- | --- | --- |
| `DiagnosticOptions`, `EventOptions` | Borrow callback and context for lifecycle | Callback runs synchronously on detecting thread | Callbacks are `noexcept`, diagnostic setup can fail initialization | Public C++ 1.x |
| `DiagnosticEvent`, `Event` | Borrowed during callback or copied value | Do not mutate/destroy parent from its callback | Contains stable error/event information | Public C++ 1.x |
| `RenderStats`, `PresenterStats` | Returned values | Engine thread or synchronized presenter query | Include failures, drops, timing, and capacity | Public C++ 1.x, stable C ABI output prefixes |

## Callback Reentrancy

Callbacks may read immutable event data and update independent synchronized state. They must not
initialize, move, shut down, destroy, or otherwise mutate the `Engine`/`Presenter` that invoked
them. Custom `OutputSink` functions follow the same restriction. The runnable
[`callbacks_and_overrides.cpp`](../apps/examples/callbacks_and_overrides.cpp) example shows a
custom byte sink, diagnostic/event callbacks, and explicit capability overrides.

## Private Implementation

Protocol encoders, `GraphicsBackend`, damage heuristics, console ownership, palette mapping, and
terminal transaction writers live under `src/`. They are intentionally absent from installed
headers and may change without a public API migration.
