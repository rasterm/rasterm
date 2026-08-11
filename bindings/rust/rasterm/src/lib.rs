/* SPDX-License-Identifier: Apache-2.0 */

/*! safe, dependency free Rust bindings for rasterm's stable C ABI. */

use rasterm_sys as sys;
use std::{
    ffi::CStr,
    fmt,
    marker::PhantomData,
    mem::MaybeUninit,
    ptr::{self, NonNull},
    rc::Rc,
};

pub const C_API_VERSION: u32 = sys::RASTERM_C_API_VERSION;
pub const UNKNOWN_TIMESTAMP: i64 = sys::RASTERM_C_UNKNOWN_TIMESTAMP;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum PixelFormat {
    Rgb24 = sys::RASTERM_PIXEL_RGB24,
    Bgr24 = sys::RASTERM_PIXEL_BGR24,
    Rgba32 = sys::RASTERM_PIXEL_RGBA32,
    Bgra32 = sys::RASTERM_PIXEL_BGRA32,
    Rgb565 = sys::RASTERM_PIXEL_RGB565,
    Xrgb1555 = sys::RASTERM_PIXEL_0RGB1555,
    Rgba4444 = sys::RASTERM_PIXEL_RGBA4444,
}

impl PixelFormat {
    const fn bytes_per_pixel(self) -> usize {
        match self {
            Self::Rgb24 | Self::Bgr24 => 3,
            Self::Rgba32 | Self::Bgra32 => 4,
            Self::Rgb565 | Self::Xrgb1555 | Self::Rgba4444 => 2,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum QualityProfile {
    #[default]
    Realtime = sys::RASTERM_QUALITY_REALTIME,
    AdaptiveVideo = sys::RASTERM_QUALITY_ADAPTIVE_VIDEO,
    High = sys::RASTERM_QUALITY_HIGH,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum ToneMapOperator {
    None = 0,
    Reinhard = 1,
    Hable = 2,
    #[default]
    Aces = 3,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum DitherMode {
    #[default]
    None = 0,
    OrderedBayer4x4 = 1,
    FloydSteinberg = 2,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum ColorPrimaries {
    #[default]
    Unspecified = 0,
    Bt709 = 1,
    Bt2020 = 2,
    DisplayP3 = 3,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum TransferFunction {
    #[default]
    Unspecified = 0,
    Srgb = 1,
    Linear = 2,
    Bt709 = 3,
    Gamma22 = 4,
    Pq = 5,
    Hlg = 6,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum MatrixCoefficients {
    #[default]
    Unspecified = 0,
    Identity = 1,
    Bt601 = 2,
    Bt709 = 3,
    Bt2020Ncl = 4,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum ColorRange {
    #[default]
    Unspecified = 0,
    Limited = 1,
    Full = 2,
}

#[derive(Clone, Copy, Debug)]
pub struct ColorMetadata {
    pub primaries: ColorPrimaries,
    pub transfer: TransferFunction,
    pub matrix: MatrixCoefficients,
    pub range: ColorRange,
    pub reference_white_nits: f32,
    pub mastering_peak_nits: f32,
}

impl ColorMetadata {
    pub const fn srgb() -> Self {
        Self {
            primaries: ColorPrimaries::Bt709,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Identity,
            range: ColorRange::Full,
            reference_white_nits: 203.0,
            mastering_peak_nits: 1000.0,
        }
    }

    fn raw(self) -> sys::rasterm_color_metadata {
        sys::rasterm_color_metadata {
            primaries: self.primaries as i32,
            transfer: self.transfer as i32,
            matrix: self.matrix as i32,
            range: self.range as i32,
            reference_white_nits: self.reference_white_nits,
            mastering_peak_nits: self.mastering_peak_nits,
            reserved: [0; 2],
        }
    }
}

impl Default for ColorMetadata {
    fn default() -> Self {
        Self {
            primaries: ColorPrimaries::Unspecified,
            transfer: TransferFunction::Unspecified,
            matrix: MatrixCoefficients::Unspecified,
            range: ColorRange::Unspecified,
            reference_white_nits: 203.0,
            mastering_peak_nits: 1000.0,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct EngineOptions {
    pub quality: QualityProfile,
    pub use_alternate_screen: bool,
    pub preserve_cursor: bool,
    pub enable_dirty_regions: bool,
    pub require_sixel_support: bool,
    pub use_synchronized_output: bool,
    pub maximum_output_bytes: u64,
    pub backpressure_threshold_milliseconds: f64,
    pub convert_to_srgb: bool,
    pub tone_map: ToneMapOperator,
    pub output_peak_nits: f32,
    pub realtime_dither: DitherMode,
    pub adaptive_palette_lock_frames: i32,
    pub scene_cut_threshold: f32,
}

impl Default for EngineOptions {
    fn default() -> Self {
        Self {
            quality: QualityProfile::Realtime,
            use_alternate_screen: false,
            preserve_cursor: true,
            enable_dirty_regions: false,
            require_sixel_support: false,
            use_synchronized_output: true,
            maximum_output_bytes: 0,
            backpressure_threshold_milliseconds: 12.0,
            convert_to_srgb: true,
            tone_map: ToneMapOperator::Aces,
            output_peak_nits: 203.0,
            realtime_dither: DitherMode::None,
            adaptive_palette_lock_frames: 12,
            scene_cut_threshold: 0.30,
        }
    }
}

impl EngineOptions {
    fn raw(self) -> sys::rasterm_engine_options {
        let mut raw = MaybeUninit::uninit();
        unsafe { sys::rasterm_engine_options_init(raw.as_mut_ptr()) };
        let mut raw = unsafe { raw.assume_init() };
        raw.quality = self.quality as i32;
        raw.use_alternate_screen = i32::from(self.use_alternate_screen);
        raw.preserve_cursor = i32::from(self.preserve_cursor);
        raw.enable_dirty_regions = i32::from(self.enable_dirty_regions);
        raw.require_sixel_support = i32::from(self.require_sixel_support);
        raw.use_synchronized_output = i32::from(self.use_synchronized_output);
        raw.maximum_output_bytes = self.maximum_output_bytes;
        raw.backpressure_threshold_milliseconds = self.backpressure_threshold_milliseconds;
        raw.convert_to_srgb = i32::from(self.convert_to_srgb);
        raw.tone_map = self.tone_map as i32;
        raw.output_peak_nits = self.output_peak_nits;
        raw.realtime_dither = self.realtime_dither as i32;
        raw.adaptive_palette_lock_frames = self.adaptive_palette_lock_frames;
        raw.scene_cut_threshold = self.scene_cut_threshold;
        raw
    }
}

#[derive(Clone, Copy, Debug)]
pub struct PresenterOptions {
    pub engine: EngineOptions,
    pub maximum_frames_per_second: f64,
}

impl Default for PresenterOptions {
    fn default() -> Self {
        Self {
            engine: EngineOptions::default(),
            maximum_frames_per_second: 0.0,
        }
    }
}

pub type DamageRect = sys::rasterm_damage_rect;
pub type RgbColor = sys::rasterm_rgb_color;

#[derive(Clone, Copy, Debug)]
pub struct FrameMetadata<'a> {
    pub frame_id: u64,
    pub timestamp_nanoseconds: i64,
    pub color: ColorMetadata,
    pub source_color: ColorMetadata,
    pub damage: Option<&'a [DamageRect]>,
}

impl Default for FrameMetadata<'_> {
    fn default() -> Self {
        Self {
            frame_id: 0,
            timestamp_nanoseconds: UNKNOWN_TIMESTAMP,
            color: ColorMetadata::srgb(),
            source_color: ColorMetadata::default(),
            damage: None,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Frame<'a> {
    pixels: &'a [u8],
    width: i32,
    height: i32,
    stride: i64,
    format: PixelFormat,
    metadata: FrameMetadata<'a>,
}

impl<'a> Frame<'a> {
    pub fn new(
        pixels: &'a [u8],
        width: u32,
        height: u32,
        stride: usize,
        format: PixelFormat,
    ) -> Result<Self, Error> {
        validate_surface(
            pixels.len(),
            width,
            height,
            stride,
            format.bytes_per_pixel(),
        )?;
        Ok(Self {
            pixels,
            width: width as i32,
            height: height as i32,
            stride: stride as i64,
            format,
            metadata: FrameMetadata::default(),
        })
    }

    pub fn metadata(mut self, metadata: FrameMetadata<'a>) -> Self {
        self.metadata = metadata;
        self
    }

    fn raw(&self) -> sys::rasterm_frame {
        let mut raw = MaybeUninit::uninit();
        unsafe { sys::rasterm_frame_init(raw.as_mut_ptr()) };
        let mut raw = unsafe { raw.assume_init() };
        raw.data = self.pixels.as_ptr();
        raw.width = self.width;
        raw.height = self.height;
        raw.stride = self.stride;
        raw.format = self.format as i32;
        apply_metadata(&mut raw.metadata, self.metadata);
        raw
    }
}

#[derive(Clone, Copy, Debug)]
pub struct IndexedFrame<'a> {
    indices: &'a [u8],
    width: i32,
    height: i32,
    stride: i64,
    palette: &'a [RgbColor],
    metadata: FrameMetadata<'a>,
}

impl<'a> IndexedFrame<'a> {
    pub fn new(
        indices: &'a [u8],
        width: u32,
        height: u32,
        stride: usize,
        palette: &'a [RgbColor],
    ) -> Result<Self, Error> {
        validate_surface(indices.len(), width, height, stride, 1)?;
        if palette.len() > 256 {
            return Err(Error::binding(
                "an indexed palette cannot contain more than 256 colors",
            ));
        }
        Ok(Self {
            indices,
            width: width as i32,
            height: height as i32,
            stride: stride as i64,
            palette,
            metadata: FrameMetadata::default(),
        })
    }

    pub fn metadata(mut self, metadata: FrameMetadata<'a>) -> Self {
        self.metadata = metadata;
        self
    }

    fn raw(&self) -> sys::rasterm_indexed_frame {
        let mut raw = MaybeUninit::uninit();
        unsafe { sys::rasterm_indexed_frame_init(raw.as_mut_ptr()) };
        let mut raw = unsafe { raw.assume_init() };
        raw.indices = self.indices.as_ptr();
        raw.width = self.width;
        raw.height = self.height;
        raw.stride = self.stride;
        raw.palette = self.palette.as_ptr();
        raw.palette_size = self.palette.len();
        apply_metadata(&mut raw.metadata, self.metadata);
        raw
    }
}

fn validate_surface(
    length: usize,
    width: u32,
    height: u32,
    stride: usize,
    bytes_per_pixel: usize,
) -> Result<(), Error> {
    if width == 0 || height == 0 || width > i32::MAX as u32 || height > i32::MAX as u32 {
        return Err(Error::binding(
            "frame dimensions must be positive signed 32-bit values",
        ));
    }
    let row_bytes = (width as usize)
        .checked_mul(bytes_per_pixel)
        .ok_or_else(|| Error::binding("frame row size overflowed"))?;
    if stride < row_bytes || stride > i64::MAX as usize {
        return Err(Error::binding("frame stride is smaller than one pixel row"));
    }
    let required = stride
        .checked_mul(height as usize - 1)
        .and_then(|prefix| prefix.checked_add(row_bytes))
        .ok_or_else(|| Error::binding("frame byte size overflowed"))?;
    if length < required {
        return Err(Error::binding(
            "pixel buffer is smaller than the described frame",
        ));
    }
    Ok(())
}

fn apply_metadata(destination: &mut sys::rasterm_frame_metadata, metadata: FrameMetadata<'_>) {
    destination.frame_id = metadata.frame_id;
    destination.timestamp_nanoseconds = metadata.timestamp_nanoseconds;
    destination.color = metadata.color.raw();
    destination.source_color = metadata.source_color.raw();
    if let Some(damage) = metadata.damage {
        destination.damage_rects = damage.as_ptr();
        destination.damage_count = damage.len();
        destination.damage_supplied = 1;
    }
}

#[derive(Clone, Debug)]
pub struct Error {
    pub code: i32,
    pub message: String,
}

impl Error {
    fn binding(message: &str) -> Self {
        Self {
            code: sys::RASTERM_ERROR_INVALID_ARGUMENT,
            message: message.to_owned(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "rasterm error {}: {}", self.code, self.message)
    }
}

impl std::error::Error for Error {}

#[derive(Clone, Copy, Debug, Default)]
pub struct RenderStats {
    pub rendered: bool,
    pub full_frame: bool,
    pub payload_bytes: u64,
    pub dirty_regions: u32,
    pub width: i32,
    pub height: i32,
    pub colors_used: i32,
    pub encode_milliseconds: f64,
    pub present_milliseconds: f64,
    pub frames_per_second: f64,
    pub payload_bytes_per_second: f64,
    pub terminal_write_p95_milliseconds: f64,
    pub terminal_write_p99_milliseconds: f64,
    pub output_failures: u64,
    pub backpressure_events: u64,
    pub payload_limit_drops: u64,
}

impl From<sys::rasterm_render_stats> for RenderStats {
    fn from(raw: sys::rasterm_render_stats) -> Self {
        Self {
            rendered: raw.rendered != 0,
            full_frame: raw.full_frame != 0,
            payload_bytes: raw.payload_bytes,
            dirty_regions: raw.dirty_regions,
            width: raw.width,
            height: raw.height,
            colors_used: raw.colors_used,
            encode_milliseconds: raw.encode_milliseconds,
            present_milliseconds: raw.present_milliseconds,
            frames_per_second: raw.frames_per_second,
            payload_bytes_per_second: raw.payload_bytes_per_second,
            terminal_write_p95_milliseconds: raw.terminal_write_p95_milliseconds,
            terminal_write_p99_milliseconds: raw.terminal_write_p99_milliseconds,
            output_failures: raw.output_failures,
            backpressure_events: raw.backpressure_events,
            payload_limit_drops: raw.payload_limit_drops,
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct PresenterStats {
    pub submitted_frames: u64,
    pub presented_frames: u64,
    pub replaced_frames: u64,
    pub latest_render: RenderStats,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct TerminalCapabilities {
    pub custom_output: bool,
    pub valid_output_handle: bool,
    pub console_output: bool,
    pub virtual_terminal_output: bool,
    pub sixel: i32,
    pub synchronized_output: i32,
    pub columns: i32,
    pub rows: i32,
    pub pixel_width: i32,
    pub pixel_height: i32,
    pub cell_pixel_width: i32,
    pub cell_pixel_height: i32,
}

pub struct Engine {
    raw: NonNull<sys::rasterm_engine>,
    _single_threaded: PhantomData<Rc<()>>,
}

impl Engine {
    pub fn new(options: EngineOptions) -> Result<Self, Error> {
        check_abi()?;
        let options = options.raw();
        let mut raw = ptr::null_mut();
        let code = unsafe { sys::rasterm_engine_create(&options, &mut raw) };
        let handle = NonNull::new(raw);
        if code != sys::RASTERM_SUCCESS {
            let message = handle.map_or_else(global_error, |value| engine_error(value.as_ptr()));
            if let Some(handle) = handle {
                unsafe { sys::rasterm_engine_destroy(handle.as_ptr()) }
            }
            return Err(Error { code, message });
        }
        Ok(Self {
            raw: handle.ok_or_else(|| Error::binding("engine creation returned a null handle"))?,
            _single_threaded: PhantomData,
        })
    }

    pub fn render(&mut self, frame: &Frame<'_>) -> Result<RenderStats, Error> {
        let frame = frame.raw();
        let mut stats = initialized_stats();
        let code = unsafe { sys::rasterm_engine_render(self.raw.as_ptr(), &frame, &mut stats) };
        self.checked(code)?;
        Ok(stats.into())
    }

    pub fn render_indexed(&mut self, frame: &IndexedFrame<'_>) -> Result<RenderStats, Error> {
        let frame = frame.raw();
        let mut stats = initialized_stats();
        let code =
            unsafe { sys::rasterm_engine_render_indexed(self.raw.as_ptr(), &frame, &mut stats) };
        self.checked(code)?;
        Ok(stats.into())
    }

    pub fn clear(&mut self) -> Result<(), Error> {
        let code = unsafe { sys::rasterm_engine_clear(self.raw.as_ptr()) };
        self.checked(code)
    }

    pub fn reset(&mut self) -> Result<(), Error> {
        let code = unsafe { sys::rasterm_engine_reset(self.raw.as_ptr()) };
        self.checked(code)
    }

    pub fn stats(&self) -> Result<RenderStats, Error> {
        let mut stats = initialized_stats();
        let code = unsafe { sys::rasterm_engine_get_stats(self.raw.as_ptr(), &mut stats) };
        self.checked(code)?;
        Ok(stats.into())
    }

    pub fn capabilities(&self) -> Result<TerminalCapabilities, Error> {
        let mut raw = MaybeUninit::uninit();
        unsafe { sys::rasterm_terminal_capabilities_init(raw.as_mut_ptr()) };
        let mut raw = unsafe { raw.assume_init() };
        let code = unsafe { sys::rasterm_engine_get_capabilities(self.raw.as_ptr(), &mut raw) };
        self.checked(code)?;
        Ok(TerminalCapabilities {
            custom_output: raw.custom_output != 0,
            valid_output_handle: raw.valid_output_handle != 0,
            console_output: raw.console_output != 0,
            virtual_terminal_output: raw.virtual_terminal_output != 0,
            sixel: raw.sixel,
            synchronized_output: raw.synchronized_output,
            columns: raw.columns,
            rows: raw.rows,
            pixel_width: raw.pixel_width,
            pixel_height: raw.pixel_height,
            cell_pixel_width: raw.cell_pixel_width,
            cell_pixel_height: raw.cell_pixel_height,
        })
    }

    fn checked(&self, code: i32) -> Result<(), Error> {
        if code == sys::RASTERM_SUCCESS {
            Ok(())
        } else {
            Err(Error {
                code,
                message: engine_error(self.raw.as_ptr()),
            })
        }
    }
}

impl Drop for Engine {
    fn drop(&mut self) {
        unsafe { sys::rasterm_engine_destroy(self.raw.as_ptr()) }
    }
}

pub struct Presenter {
    raw: NonNull<sys::rasterm_presenter>,
}

unsafe impl Send for Presenter {}
unsafe impl Sync for Presenter {}

impl Presenter {
    pub fn new(options: PresenterOptions) -> Result<Self, Error> {
        check_abi()?;
        let mut raw_options = MaybeUninit::uninit();
        unsafe { sys::rasterm_presenter_options_init(raw_options.as_mut_ptr()) };
        let mut raw_options = unsafe { raw_options.assume_init() };
        raw_options.engine = options.engine.raw();
        raw_options.maximum_frames_per_second = options.maximum_frames_per_second;
        let mut raw = ptr::null_mut();
        let code = unsafe { sys::rasterm_presenter_create(&raw_options, &mut raw) };
        let handle = NonNull::new(raw);
        if code != sys::RASTERM_SUCCESS {
            let message = handle.map_or_else(global_error, |value| presenter_error(value.as_ptr()));
            if let Some(handle) = handle {
                unsafe { sys::rasterm_presenter_destroy(handle.as_ptr()) }
            }
            return Err(Error { code, message });
        }
        Ok(Self {
            raw: handle
                .ok_or_else(|| Error::binding("presenter creation returned a null handle"))?,
        })
    }

    pub fn submit(&self, frame: &Frame<'_>) -> Result<(), Error> {
        let frame = frame.raw();
        let code = unsafe { sys::rasterm_presenter_submit(self.raw.as_ptr(), &frame) };
        self.checked(code)
    }

    pub fn submit_indexed(&self, frame: &IndexedFrame<'_>) -> Result<(), Error> {
        let frame = frame.raw();
        let code = unsafe { sys::rasterm_presenter_submit_indexed(self.raw.as_ptr(), &frame) };
        self.checked(code)
    }

    pub fn stats(&self) -> Result<PresenterStats, Error> {
        let mut raw = MaybeUninit::uninit();
        unsafe { sys::rasterm_presenter_stats_init(raw.as_mut_ptr()) };
        let mut raw = unsafe { raw.assume_init() };
        let code = unsafe { sys::rasterm_presenter_get_stats(self.raw.as_ptr(), &mut raw) };
        self.checked(code)?;
        Ok(PresenterStats {
            submitted_frames: raw.submitted_frames,
            presented_frames: raw.presented_frames,
            replaced_frames: raw.replaced_frames,
            latest_render: raw.latest_render.into(),
        })
    }

    fn checked(&self, code: i32) -> Result<(), Error> {
        if code == sys::RASTERM_SUCCESS {
            Ok(())
        } else {
            Err(Error {
                code,
                message: presenter_error(self.raw.as_ptr()),
            })
        }
    }
}

impl Drop for Presenter {
    fn drop(&mut self) {
        unsafe { sys::rasterm_presenter_destroy(self.raw.as_ptr()) }
    }
}

pub fn version() -> (u32, u32, u32) {
    let (mut major, mut minor, mut patch) = (0, 0, 0);
    unsafe { sys::rasterm_version(&mut major, &mut minor, &mut patch) };
    (major, minor, patch)
}

fn check_abi() -> Result<(), Error> {
    let version = unsafe { sys::rasterm_c_api_version() };
    if version == C_API_VERSION {
        Ok(())
    } else {
        Err(Error {
            code: sys::RASTERM_ERROR_API_VERSION_MISMATCH,
            message: format!(
                "binding requires C API {C_API_VERSION}, loaded library reports {version}"
            ),
        })
    }
}

fn initialized_stats() -> sys::rasterm_render_stats {
    let mut stats = MaybeUninit::uninit();
    unsafe { sys::rasterm_render_stats_init(stats.as_mut_ptr()) };
    unsafe { stats.assume_init() }
}

fn read_error(mut call: impl FnMut(*mut std::ffi::c_char, usize, *mut usize)) -> String {
    let mut required = 0;
    call(ptr::null_mut(), 0, &mut required);
    if required == 0 {
        return "rasterm reported an error without a diagnostic".to_owned();
    }
    let mut bytes = vec![0_u8; required];
    call(bytes.as_mut_ptr().cast(), bytes.len(), &mut required);
    unsafe { CStr::from_ptr(bytes.as_ptr().cast()) }
        .to_string_lossy()
        .into_owned()
}

fn global_error() -> String {
    read_error(|buffer, capacity, required| unsafe {
        sys::rasterm_last_error(buffer, capacity, required);
    })
}

fn engine_error(engine: *mut sys::rasterm_engine) -> String {
    read_error(|buffer, capacity, required| unsafe {
        sys::rasterm_engine_last_error(engine, buffer, capacity, required);
    })
}

fn presenter_error(presenter: *mut sys::rasterm_presenter) -> String {
    read_error(|buffer, capacity, required| unsafe {
        sys::rasterm_presenter_last_error(presenter, buffer, capacity, required);
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frame_rejects_short_buffers() {
        assert!(Frame::new(&[0; 15], 2, 2, 8, PixelFormat::Rgba32).is_err());
        let frame = Frame::new(&[0; 16], 2, 2, 8, PixelFormat::Rgba32).unwrap();
        let raw = frame.raw();
        assert_eq!(raw.metadata.color.transfer, sys::RASTERM_TRANSFER_SRGB);
        assert_eq!(raw.metadata.color.reference_white_nits, 203.0);
        assert_eq!(raw.metadata.source_color.reference_white_nits, 203.0);
    }

    #[test]
    fn indexed_frame_rejects_large_palettes() {
        let palette = [RgbColor::default(); 257];
        assert!(IndexedFrame::new(&[0], 1, 1, 1, &palette).is_err());
    }

    #[test]
    fn linked_library_matches_binding_version() {
        assert_eq!(unsafe { sys::rasterm_c_api_version() }, C_API_VERSION);
        assert_eq!(version(), (1, 2, 0));
    }
}
