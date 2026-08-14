# Versioning and Compatibility

rasterm uses semantic versioning for its public headers and installed CMake package.

- The current version is `1.3.0`, C++ ABI generation is `1`, and C API version is `1`.
- From 1.0 onward, breaking source or ABI changes require a major version increment.
- Additive APIs may ship in minor releases. Fixes that preserve documented behavior ship in patch releases.
- Deprecated APIs remain available for at least one subsequent minor release before removal.
- Only declarations under `include/rasterm/` are public. Nothing under `src/`, `apps/`, `validation/`, or Microsoft's terminal snapshot carries compatibility guarantees.
- Consumers should compare `RASTERM_C_API_VERSION` with `rasterm_c_api_version()` when
  loading binaries built independently.

rasterm 1.2 uses a static library for its C++ API and offers an optional DLL containing
only the C ABI. Official MSVC binaries support Windows x64, Visual Studio 2022 v143,
C++20, and the dynamic MSVC runtime: `/MD` for Release and
`/MDd` for Debug. Consumers must match architecture, configuration, runtime library mode,
iterator debug level, and a binary compatible v143 toolset. Debug and Release objects or
CRT modes must not be mixed.

The C++ API uses standard library types, so binary compatibility is guaranteed only
inside the MSVC boundary above. Source compatibility is broader: rebuild rasterm with
your own compatible compiler when needed. The C API uses opaque handles and fixed-size
structures from `<rasterm/capi.h>`, but a static library must still match the caller's
object format and runtime. MinGW programs therefore need a MinGW64 build and cannot link
the official MSVC `.lib`.

The reviewed 1.0 public surface is exactly the declarations installed under
`include/rasterm/`. Encoder, parser, terminal session, color conversion, media, and
integration declarations under `src/`, `validation/`, and `apps/` are private. The
optional shared library exports C API v1 only; the C++ ABI remains static and private.
