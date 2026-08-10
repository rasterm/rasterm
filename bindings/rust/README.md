# rasterm for Rust

This workspace contains two dependency free crates:

- `rasterm-sys`: literal checked in declarations for stable C API v1.
- `rasterm`: safe ownership, packed/indexed frames, damage, Engine, and Presenter.

```powershell
$env:RASTERM_LIB_DIR = Resolve-Path ../../build/bindings/Release
cargo test --workspace
cargo run -p rasterm --example gradient
```

See [`docs/BINDINGS.md`](../../docs/BINDINGS.md) for linking and lifetime details.
 