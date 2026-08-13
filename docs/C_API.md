# Stable C API

rasterm provides a C11 compatible API through `<rasterm/capi.h>`. The header does not
expose C++ standard library, Windows, OpenCV, FFmpeg, or miniaudio types. You work with
opaque `rasterm_engine` and `rasterm_presenter` handles; rasterm keeps their internal
state private.

```c
#include <rasterm/capi.h>

rasterm_engine_options options;
rasterm_engine_options_init(&options);
options.use_alternate_screen = 1;

rasterm_engine* engine = NULL;
rasterm_result result = rasterm_engine_create(&options, &engine);
if (result == RASTERM_SUCCESS) {
    rasterm_frame frame;
    rasterm_frame_init(&frame);
    frame.data = pixels;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.format = RASTERM_PIXEL_RGBA32;
    rasterm_engine_render(engine, &frame, NULL);
}
rasterm_engine_destroy(engine);
```

Always call the matching `_init` function before using an options, frame, statistics,
or capabilities structure. Initializers zero reserved storage, set the v1 structure
size, and install documented defaults. Applications must not write reserved fields.

Create functions may return a non null handle alongside an initialization error so the
caller can retrieve the detailed message. Destroy every non null handle, including
failed initialization handles.

The synchronous Engine borrows buffers only until the render call returns. The C
Presenter copies pixels, palettes, metadata, and damage rectangles into a one frame
mailbox. When it falls behind, a new submission replaces the waiting frame instead of
adding more latency.

`rasterm_presenter_wait_until_idle` provides a bounded drain to sink acceptance, and
`rasterm_presenter_invalidate` forces the next rendered frame to be full. Immediate
destruction cancels a waiting frame, drain first when final frame delivery is required.

Custom output requires both `rasterm_write_callback` and `rasterm_flush_callback`.
Callbacks and their context must remain valid until the handle is destroyed. Presenter
callbacks execute on its worker thread; Engine callbacks execute synchronously on the
calling thread. Byte spans are borrowed only for the callback. Returning zero reports
an output failure and means no supplied bytes were accepted. Presenter statistics and
error queries are reentrant. Callbacks must not initialize, destroy, or shut down their
active handle.

All owned handles are released with their matching destroy function. Null destroy calls
are harmless. No pointer stored in an input structure transfers ownership to rasterm.
Packed and indexed strides are positive byte counts, bottom up/negative stride surfaces
are rejected. RGB565 and 0RGB1555 use little endian Windows pixel words. RGBA4444 uses
high to low R, G, B, A nibbles. Alpha is always ignored including zero alpha, the RGB
components are encoded as stored with no straight/premultiplied alpha inference.

Capability support and geometry override fields in `rasterm_engine_options` are optional.
Zero/`RASTERM_CAPABILITY_UNKNOWN` retains automatic detection. Statistics distinguish
SIXEL payload from accepted wire bytes and expose reusable scratch/output capacity plus
validation and conversion time.

The trailing encoder tuning fields control persistent palette registers, periodic refresh,
completed transaction chunk size, mapping thread count, and independent regional
quantization. `rasterm_engine_options_init` supplies the 1.2 defaults. Older zero initialized
v1 structures retain self contained palettes, single call output, automatic threading, and
global adaptive palettes.

## ABI Policy

- `RASTERM_C_API_VERSION` is `1` for the rasterm 1.x series.
- Existing function signatures, enum values, v1 structure prefixes, and documented
  behavior will not change during 1.x.
- Additive functionality uses new functions or reserved trailing storage.
- Unsupported callers receive `RASTERM_ERROR_API_VERSION_MISMATCH`.
- Breaking ABI changes require rasterm 2.0 and C API version 2.
- Engine is single threaded per handle. Presenter submission and statistics are
  thread safe.
- Structures smaller than their v1 prefix return `RASTERM_ERROR_BUFFER_TOO_SMALL`;
  larger structures are accepted and unknown trailing bytes are ignored.
- rasterm 1.2 keeps the static library as the default and can build an optional shared
  C ABI with `RASTERM_BUILD_SHARED_C_API=ON`. Define `RASTERM_SHARED_LIBRARY` when
  compiling a C/C++ consumer against its import library.

`rasterm_last_error` returns the calling thread's most recent C API validation or handle
creation diagnostic. Handle specific last error functions additionally expose engine or
Presenter failures. They support a two call size query: pass a null buffer to obtain the
required byte count, then provide a buffer with that capacity. The returned code and
UTF-8 diagnostic remain valid independently, diagnostics are copied into caller storage.
