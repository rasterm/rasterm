# rasterm Bindings

Both supported language bindings use `<rasterm/capi.h>` as their only native contract:

- [`rust`](rust/) contains the raw `rasterm-sys` crate and safe `rasterm` crate.
- [`python`](python/) contains a typed, dependency free buffer protocol wrapper.

Build, linking, ownership, and ABI details are documented in
[`docs/BINDINGS.md`](../docs/BINDINGS.md).
