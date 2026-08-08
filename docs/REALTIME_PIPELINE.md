# Realtime Pipeline

`Engine` renders one frame at a time on the calling thread. It accepts either a packed
`FrameView` or an `IndexedFrameView` with an exact palette. `Presenter` adds a worker
thread and keeps at most one frame waiting.

For video, rasterm uses this path:

```text
OpenCV frame -> resize -> public FrameView -> Engine -> SIXEL -> Windows Terminal
                         |                |
                         |                +-> (optional cell aligned dirtyregions)
                         +-> (player owned adaptive resolution controller)
```

The player uses audio as its clock. If a video frame is already late, it drops that frame
instead of slowing or breaking the audio stream.

Frame metrics are written to `rasterm-metrics.csv` in the process working
directory. Logging does not write into the terminal while video is displayed.

The realtime packed pixel preset uses a stable palette without Floyd Steinberg dithering.
Palette slots keep the same meaning between frames, which prevents brightness and shadow
pumping. Indexed sources skip this mapping and keep the caller's palette. Still images
can use the slower high quality adaptive profile.
