# rasterm Examples

Each file is a complete minimal program. Build the standalone examples project with:

```powershell
.\scripts\build-examples.ps1 -Configuration Release
```

Executables are written to `apps/examples/build/Release`:

- `c_api.c` — stable C ABI with a callback sink
- `synchronous.cpp` — borrowed synchronous frame
- `presenter.cpp` — capacity one asynchronous presentation
- `indexed.cpp` — exact caller provided palette
- `custom_sink.cpp` — all or nothing memory sink
- `damage_rectangles.cpp` — caller provided damage
- `color_metadata.cpp` — source color metadata and tone mapping

See [`docs/API.md`](../../docs/API.md) for ownership and threading contracts.
