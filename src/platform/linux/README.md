# Linux Platform

This directory reserves the private Linux integration boundary. rasterm does not currently
support Linux, and no file here is compiled into the library.

A Linux implementation must provide the same lifecycle behavior as the Windows console
session without exposing platform types through `include/rasterm/`:

- acquire and restore terminal/TTY state, including signal safe cancellation cleanup
- enable the required virtual terminal output mode
- detect terminal columns, rows, pixel dimensions, and cell pixel dimensions
- report SIXEL and synchronized output support conservatively
- preserve custom `OutputSink` operation without requiring a TTY
- pass lifecycle, resize, capability override, fault injection, and parser round trip tests.

Platform sources should only be added to `RASTERM_PLATFORM_SOURCES` after that contract has a
working implementation. A public platform abstraction is intentionally deferred until Windows
and Linux expose a proven common contract.
