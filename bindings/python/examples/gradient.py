# SPDX-License-Identifier: Apache-2.0 

import rasterm

width, height = 320, 180
pixels = bytearray(width * height * 4)
for y in range(height):
    for x in range(width):
        offset = (y * width + x) * 4
        pixels[offset:offset + 4] = bytes((x * 255 // width, y * 255 // height, 180, 255))

frame = rasterm.Frame(pixels, width, height, width * 4, rasterm.PixelFormat.RGBA32)
with rasterm.Engine() as engine:
    engine.render(frame)
