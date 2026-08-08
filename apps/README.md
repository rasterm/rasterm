# Rasterm Applications and Integrations

Everything under `apps/` is optional and excluded from rasterm's core package and CI
release artifact.

- [`examples`](examples/README.md) — dependency free compiled API examples
- [`rPlayer`](rPlayer/README.md) — optional OpenCV/FFmpeg/miniaudio media application
- [`SimpleNES`](SimpleNES/README.md) — software framebuffer emulator integration

You build these separately when needed. The installed rasterm package contains
only public headers, the static engine library, package metadata, and license notices.
