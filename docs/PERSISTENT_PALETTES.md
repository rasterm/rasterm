# Persistent SIXEL Palette Registers

Windows Terminal keeps its SIXEL color table between images, so rasterm could save a
small amount of output by defining an unchanged palette only once.

raasterm still includes palette definitions in every encoded image because:

- applications outside rasterm can modify terminal palette registers
- output may be redirected to another parser or captured and replayed independently
- dirty regions are self contained SIXEL images
- omitting definitions would make a frame depend on hidden terminal history

The saving is small compared with the pixel data, and self contained frames are safer to
capture, replay, and recover after failures. rasterm may offer persistent definitions as
an opt in backend setting after they have been tested across more terminals.
