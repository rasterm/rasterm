# Rasterm Applications and Integrations

Everything under `apps/` is optional and excluded from rasterm's core package and CI
release artifact.

- [`examples`](examples/README.md) — dependency free compiled API examples
- [`rPlayer`](rPlayer/README.md) — optional OpenCV/FFmpeg/miniaudio media application
- [`Termirror`](Termirror/README.md) — live desktop/window-region mirror using native DXGI damage
- [`SimpleNES`](SimpleNES/README.md) — software framebuffer emulator integration
- [`RetroArch`](RetroArch/README.md) — software framebuffer emulator integration (not finished)

You build these separately when needed. The installed Rasterm package contains
only public headers, the static engine library, package metadata, and license notices.
Each standalone consumer owns its generated tree: `apps/<name>/build`. The repository
root `build/` tree is reserved for Rasterm engine builds, installs, and validation.
