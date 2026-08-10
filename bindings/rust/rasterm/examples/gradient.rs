/* SPDX-License-Identifier: Apache-2.0 */

use rasterm::{Engine, EngineOptions, Frame, PixelFormat};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let (width, height) = (320_u32, 180_u32);
    let mut pixels = vec![0_u8; (width * height * 4) as usize];
    for y in 0..height {
        for x in 0..width {
            let offset = ((y * width + x) * 4) as usize;
            pixels[offset..offset + 4].copy_from_slice(&[
                (x * 255 / width) as u8,
                (y * 255 / height) as u8,
                180,
                255,
            ]);
        }
    }

    let frame = Frame::new(
        &pixels,
        width,
        height,
        width as usize * 4,
        PixelFormat::Rgba32,
    )?;
    let mut engine = Engine::new(EngineOptions::default())?;
    engine.render(&frame)?;
    Ok(())
}
