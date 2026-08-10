# rasterm

Safe Rust bindings for rasterm terminal graphics. The crate provides validated packed
and indexed frames, color metadata, damage rectangles, synchronous `Engine`, and the
thread safe latest frame `Presenter`.

```powershell
cargo add rasterm
```

```rust
use rasterm::{Engine, EngineOptions, Frame, PixelFormat};

let frame = Frame::new(&pixels, width, height, stride, PixelFormat::Rgba32)?;
let mut engine = Engine::new(EngineOptions::default())?;
engine.render(&frame)?;
```

The matching native library is distributed in rasterm's release ZIP. Point
`RASTERM_LIB_DIR` at its `lib` directory before building.
