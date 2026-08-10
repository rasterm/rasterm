# rasterm-sys

Literal, checked in Rust declarations for rasterm's stable C API v1. This crate does
not require bindgen or libclang.

Set `RASTERM_LIB_DIR` to a directory containing the matching `rasterm.lib`, or use the
library from the rasterm release ZIP. Set `RASTERM_LINK_DYNAMIC=1` when linking the
optional `rasterm-import.lib` and place `rasterm.dll` on `PATH` at runtime.

Most applications should depend on the safe `rasterm` crate instead.
