# Persistent SIXEL Palette Registers

Windows Terminal keeps its SIXEL color table between images, so rasterm could save a
small amount of output by defining an unchanged palette only once.

rasterm includes palette definitions in every encoded image by default because:

- applications outside rasterm can modify terminal palette registers
- output may be redirected to another parser or captured and replayed independently
- dirty regions are self contained SIXEL images
- omitting definitions would make a frame depend on hidden terminal history

The saving is small compared with the pixel data, and self contained frames are safer to
capture, replay, and recover after failures. Set `EncoderTuning::persistPaletteRegisters` (or
the matching C, Rust, or Python option) to opt into register reuse. `paletteRefreshFrames`
bounds how long an unchanged definition may be omitted. Regional adaptive palettes remain
independent when `independentRegionQuantization` is enabled.

Palette ownership is exclusive while persistence is active. After any other producer emits a
SIXEL palette definition to the same terminal, call `Engine::reset()` or
`Presenter::invalidate()` before Rasterm's next frame. Clear, sink failure, output limit failure,
and engine reset also invalidate the private register signature. The next successful update is
self contained and defines its palette again. The golden encoder and protocol recovery tests
exercise this transition.
