# Rust and Python Bindings

rasterm's stable C API v1 is the only foreign language boundary. Rust and Python do not
depend on each other and neither binding reaches into the C++ ABI.

Cross language default values are defined once in
[`defaults.h`](../include/rasterm/defaults.h). C++ initializers derive from that table, while
C, Rust, and Python conformance tests verify every mapping they expose.

```text
                 rasterm C++ core
                        |
                 stable C ABI v1
                  /             \
         rasterm-sys         Python ctypes
              |              buffer wrapper
        safe rasterm crate
```

The public package is [`rasterm`](https://pypi.org/project/rasterm/) on PyPI. The Rust
crates are [`rasterm`](https://crates.io/crates/rasterm) and
[`rasterm-sys`](https://crates.io/crates/rasterm-sys).

## Native Libraries

Rust links the normal static library by default. Python loads the optional shared C ABI,
which exports only declarations from `<rasterm/capi.h>`:

```powershell
cmake -S . -B build/bindings `
  -DRASTERM_BUILD_SHARED_C_API=ON `
  -DRASTERM_WARNINGS_AS_ERRORS=ON
cmake --build build/bindings --config Release --target rasterm rasterm-shared
```

This produces `rasterm.lib`, `rasterm.dll`, and the shared library import archive
`rasterm-import.lib`. Static C/C++ consumers are unchanged.

Generator and platform choices are cached per build directory. When reusing an existing
`build/bindings`, repeat its original generator arguments so the repository commands omit
`-A` so the same directory remains reusable on an x64 developer shell.

## Rust

[`bindings/rust/rasterm-sys`](../bindings/rust/rasterm-sys/) contains literal,
checked in C ABI declarations. It intentionally performs no convenience conversion and
does not require libclang or consumer time bindgen. [`bindings/rust/rasterm`](../bindings/rust/rasterm/)
provides `Engine`, `Presenter`, packed/indexed frames, damage, validation, statistics,
errors, and RAII ownership.

```rust
use rasterm::{Engine, EngineOptions, Frame, PixelFormat};

let frame = Frame::new(&pixels, width, height, stride, PixelFormat::Rgba32)?;
let mut engine = Engine::new(EngineOptions::default())?;
engine.render(&frame)?;
```

For repository builds, `rasterm-sys` discovers common local Release/Debug output
directories. Packaged or custom builds should be explicit:

```powershell
$env:RASTERM_LIB_DIR = Resolve-Path build/bindings/Release
cargo test --manifest-path bindings/rust/Cargo.toml --workspace
cargo run --manifest-path bindings/rust/Cargo.toml -p rasterm --example gradient
```

Set `RASTERM_LINK_DYNAMIC=1` to link `rasterm-import.lib` instead. `RASTERM_LIB_NAME`
overrides the library basename for nonstandard installations.

`Engine` is deliberately neither `Send` nor `Sync`. `Presenter` is `Send + Sync`
because C API submission and statistics are thread safe. Rust validates dimensions,
positive strides, buffer lengths, and indexed palette limits before an unsafe call.

## Python

The Python package is dependency free and accepts any C contiguous object implementing
the buffer protocol. This includes `bytes`, `bytearray`, `memoryview`, and C contiguous
NumPy/OpenCV arrays without making NumPy a required dependency.

```powershell
$env:RASTERM_LIBRARY = Resolve-Path build/bindings/Release/rasterm.dll
python -m pip install -e bindings/python
python bindings/python/examples/gradient.py
```

```python
import rasterm

frame = rasterm.Frame(pixels, width, height, width * 4,
                      rasterm.PixelFormat.RGBA32)
with rasterm.Engine() as engine:
    stats = engine.render(frame)
```

The package includes type stubs and `py.typed`. `RASTERM_LIBRARY` may point to an
external DLL. Release wheels place `rasterm.dll` in the package's `_native` directory,
which the loader checks automatically.

## Ownership and ABI Checks

- `Engine.render` borrows pixels, palettes, and damage only until the call returns.
- `Presenter.submit` copies the entire packed or indexed submission before returning,
  so temporary Rust slices and Python buffers are safe.
- Output callbacks are intentionally deferred from the safe V1 bindings exposing them
  requires binding owned callback state that survives the handle and Presenter worker.
- Both bindings compare the loaded C API version with version 1.
- Rust and Python assert the published x64 structure sizes. The existing C/C++ ABI
  fixtures remain authoritative for symbols, offsets, enum values, and calling convention.
- Debug and Release libraries, architectures, and CRT modes must not be mixed.

See [`C_API.md`](C_API.md) and [`LIFETIMES.md`](LIFETIMES.md) for the complete native
contract.
