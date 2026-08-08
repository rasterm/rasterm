# Color Management

Unless you provide other metadata, rasterm treats packed RGB pixels as full-range sRGB
with BT.709 primaries. When metadata says otherwise, rasterm converts the pixels to
linear light, maps BT.2020 or Display P3 colors into BT.709, tone maps HDR when needed,
and converts the result back to sRGB for the terminal.

`FrameMetadata::sourceColor` preserves media metadata before a decoder converts YUV
to RGB. `FrameMetadata::color` describes submitted pixels. Matrix and range conversion
belong to the decoder when it produces RGB; the player marks decoded RGB as identity
matrix and full range while retaining the original matrix and range separately.

Tone mapping provides None, Reinhard, Hable, and ACES curves. SDR sRGB frames use a
zero copy path. Adaptive palettes are locked for a configurable interval and rebuilt
at scene cuts. Ordered Bayer dithering is deterministic across frames. Floyd Steinberg
is restricted to high quality still output because its propagated error is not
temporally stable.

Still images use Windows Imaging Component to convert embedded ICC profiles to sRGB;
formats unsupported by WIC fall back to OpenCV. Palette tests use Oklab distance.

## 1.0 Conformance Limits

The color tests compare normalized Oklab values. These limits tell us whether a build is
good enough to release; they do not mean a 256-color terminal image is visually lossless:

| Path | Maximum error | Mean error |
|---|---:|---:|
| Indexed caller palette, including the 64 color NES fixture | 0.015 | 0.005 |
| Realtime fixed palette sRGB ColorChecker | 0.160 | 0.075 |

Indexed colors are preserved exactly as SIXEL RGB register percentages. SIXEL registers
store integer percentages from 0 through 100, so decoding a register back to 8-bit sRGB
can differ by at most the protocol's percentage quantization. The test validates every
one of 256 emitted register definitions before measuring decoded Oklab error.

`validation/tests/fixtures/ColorReferenceFixtures.hpp` covers sRGB full and limited range, BT.601,
BT.709, BT.2020, linear SDR, PQ, and HLG inputs. Matrix coefficients describe the source
before YUV to RGB decode, rasterm accepts RGB, so the decoder must apply that matrix and
place the original description in `sourceColor`.

The adaptive palette tests keep the palette stable through small changes and rebuild it
after a clear scene cut. Ordered Bayer dithering stays anchored to pixel positions, so
the same frame produces the same bytes even after another frame appears between repeats.
This avoids random flicker, although shallow gradients can still show normal color bands.

## Expected Limitations

- SIXEL exposes at most 256 registers. Images with more simultaneous colors are
  quantized (smooth gradients, shadows, and skin tones can show banding).
- Integer percent SIXEL registers cannot represent every 8-bit channel exactly.
- Realtime mode favors stable fixed colors and bounded encode time over minimum still 
  image error. High quality mode is slower and may rebuild adaptive palettes.
- HDR is tone mapped into SDR sRGB. Highlight detail outside the selected output peak is
  compressed, and a terminal cannot reproduce the source display's peak luminance or
  wide gamut.
- PQ/HLG conversion depends on accurate source metadata. Missing or incorrect mastering
  information cannot be reconstructed by rasterm.
- Ordered dithering is deterministic but visible at close range. Floyd Steinberg remains
  a quality first still image option because error diffusion is not temporally stable.
