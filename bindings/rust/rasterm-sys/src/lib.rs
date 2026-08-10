/* SPDX-License-Identifier: Apache-2.0 */

#![allow(non_camel_case_types, non_upper_case_globals)]

use core::ffi::{c_char, c_void};

pub const RASTERM_C_API_VERSION: u32 = 1;
pub const RASTERM_C_UNKNOWN_TIMESTAMP: i64 = i64::MIN;

pub type rasterm_result = i32;
pub const RASTERM_SUCCESS: rasterm_result = 0;
pub const RASTERM_ERROR_INVALID_ARGUMENT: rasterm_result = 1;
pub const RASTERM_ERROR_INVALID_OUTPUT_HANDLE: rasterm_result = 2;
pub const RASTERM_ERROR_OUTPUT_IS_NOT_CONSOLE: rasterm_result = 3;
pub const RASTERM_ERROR_CONSOLE_MODE_QUERY_FAILED: rasterm_result = 4;
pub const RASTERM_ERROR_CONSOLE_MODE_ENABLE_FAILED: rasterm_result = 5;
pub const RASTERM_ERROR_TERMINAL_DIMENSIONS_UNAVAILABLE: rasterm_result = 6;
pub const RASTERM_ERROR_UNSUPPORTED_TERMINAL: rasterm_result = 7;
pub const RASTERM_ERROR_STDOUT_MODE_FAILED: rasterm_result = 8;
pub const RASTERM_ERROR_CONTROL_HANDLER_REGISTRATION_FAILED: rasterm_result = 9;
pub const RASTERM_ERROR_DIAGNOSTIC_OPEN_FAILED: rasterm_result = 10;
pub const RASTERM_ERROR_OUTPUT_WRITE_FAILED: rasterm_result = 11;
pub const RASTERM_ERROR_OUTPUT_FLUSH_FAILED: rasterm_result = 12;
pub const RASTERM_ERROR_OUTPUT_BUFFER_LIMIT_EXCEEDED: rasterm_result = 13;
pub const RASTERM_ERROR_PRESENTER_THREAD_START_FAILED: rasterm_result = 14;
pub const RASTERM_ERROR_INITIALIZATION_EXCEPTION: rasterm_result = 15;
pub const RASTERM_ERROR_RENDERING_EXCEPTION: rasterm_result = 16;
pub const RASTERM_ERROR_BUFFER_TOO_SMALL: rasterm_result = 17;
pub const RASTERM_ERROR_API_VERSION_MISMATCH: rasterm_result = 18;
pub const RASTERM_ERROR_OUT_OF_MEMORY: rasterm_result = 19;

pub type rasterm_pixel_format = i32;
pub const RASTERM_PIXEL_RGB24: rasterm_pixel_format = 0;
pub const RASTERM_PIXEL_BGR24: rasterm_pixel_format = 1;
pub const RASTERM_PIXEL_RGBA32: rasterm_pixel_format = 2;
pub const RASTERM_PIXEL_BGRA32: rasterm_pixel_format = 3;
pub const RASTERM_PIXEL_RGB565: rasterm_pixel_format = 4;
pub const RASTERM_PIXEL_0RGB1555: rasterm_pixel_format = 5;
pub const RASTERM_PIXEL_RGBA4444: rasterm_pixel_format = 6;

pub type rasterm_quality_profile = i32;
pub const RASTERM_QUALITY_REALTIME: rasterm_quality_profile = 0;
pub const RASTERM_QUALITY_ADAPTIVE_VIDEO: rasterm_quality_profile = 1;
pub const RASTERM_QUALITY_HIGH: rasterm_quality_profile = 2;

pub type rasterm_capability_support = i32;
pub const RASTERM_CAPABILITY_UNKNOWN: rasterm_capability_support = 0;
pub const RASTERM_CAPABILITY_UNSUPPORTED: rasterm_capability_support = 1;
pub const RASTERM_CAPABILITY_SUPPORTED: rasterm_capability_support = 2;

pub type rasterm_color_primaries = i32;
pub const RASTERM_PRIMARIES_UNSPECIFIED: rasterm_color_primaries = 0;
pub const RASTERM_PRIMARIES_BT709: rasterm_color_primaries = 1;
pub const RASTERM_PRIMARIES_BT2020: rasterm_color_primaries = 2;
pub const RASTERM_PRIMARIES_DISPLAY_P3: rasterm_color_primaries = 3;

pub type rasterm_transfer_function = i32;
pub const RASTERM_TRANSFER_UNSPECIFIED: rasterm_transfer_function = 0;
pub const RASTERM_TRANSFER_SRGB: rasterm_transfer_function = 1;
pub const RASTERM_TRANSFER_LINEAR: rasterm_transfer_function = 2;
pub const RASTERM_TRANSFER_BT709: rasterm_transfer_function = 3;
pub const RASTERM_TRANSFER_GAMMA22: rasterm_transfer_function = 4;
pub const RASTERM_TRANSFER_PQ: rasterm_transfer_function = 5;
pub const RASTERM_TRANSFER_HLG: rasterm_transfer_function = 6;

pub type rasterm_matrix_coefficients = i32;
pub const RASTERM_MATRIX_UNSPECIFIED: rasterm_matrix_coefficients = 0;
pub const RASTERM_MATRIX_IDENTITY: rasterm_matrix_coefficients = 1;
pub const RASTERM_MATRIX_BT601: rasterm_matrix_coefficients = 2;
pub const RASTERM_MATRIX_BT709: rasterm_matrix_coefficients = 3;
pub const RASTERM_MATRIX_BT2020_NCL: rasterm_matrix_coefficients = 4;

pub type rasterm_color_range = i32;
pub const RASTERM_RANGE_UNSPECIFIED: rasterm_color_range = 0;
pub const RASTERM_RANGE_LIMITED: rasterm_color_range = 1;
pub const RASTERM_RANGE_FULL: rasterm_color_range = 2;

pub type rasterm_tone_map_operator = i32;
pub const RASTERM_TONE_MAP_NONE: rasterm_tone_map_operator = 0;
pub const RASTERM_TONE_MAP_REINHARD: rasterm_tone_map_operator = 1;
pub const RASTERM_TONE_MAP_HABLE: rasterm_tone_map_operator = 2;
pub const RASTERM_TONE_MAP_ACES: rasterm_tone_map_operator = 3;

pub type rasterm_dither_mode = i32;
pub const RASTERM_DITHER_NONE: rasterm_dither_mode = 0;
pub const RASTERM_DITHER_ORDERED_BAYER_4X4: rasterm_dither_mode = 1;
pub const RASTERM_DITHER_FLOYD_STEINBERG: rasterm_dither_mode = 2;

pub type rasterm_write_callback =
    Option<unsafe extern "C" fn(*mut c_void, *const c_char, usize) -> i32>;
pub type rasterm_flush_callback = Option<unsafe extern "C" fn(*mut c_void) -> i32>;

#[repr(C)]
pub struct rasterm_engine {
    _private: [u8; 0],
}
#[repr(C)]
pub struct rasterm_presenter {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct rasterm_color_metadata {
    pub primaries: i32,
    pub transfer: i32,
    pub matrix: i32,
    pub range: i32,
    pub reference_white_nits: f32,
    pub mastering_peak_nits: f32,
    pub reserved: [u64; 2],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct rasterm_damage_rect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct rasterm_frame_metadata {
    pub frame_id: u64,
    pub timestamp_nanoseconds: i64,
    pub color: rasterm_color_metadata,
    pub source_color: rasterm_color_metadata,
    pub damage_rects: *const rasterm_damage_rect,
    pub damage_count: usize,
    pub damage_supplied: i32,
    pub reserved: [u64; 4],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct rasterm_engine_options {
    pub struct_size: u32,
    pub api_version: u32,
    pub quality: i32,
    pub use_alternate_screen: i32,
    pub preserve_cursor: i32,
    pub enable_dirty_regions: i32,
    pub require_sixel_support: i32,
    pub use_synchronized_output: i32,
    pub maximum_output_bytes: u64,
    pub backpressure_threshold_milliseconds: f64,
    pub convert_to_srgb: i32,
    pub tone_map: i32,
    pub output_peak_nits: f32,
    pub realtime_dither: i32,
    pub adaptive_palette_lock_frames: i32,
    pub scene_cut_threshold: f32,
    pub output_context: *mut c_void,
    pub write: rasterm_write_callback,
    pub flush: rasterm_flush_callback,
    pub reserved: [u64; 8],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct rasterm_frame {
    pub struct_size: u32,
    pub data: *const u8,
    pub width: i32,
    pub height: i32,
    pub stride: i64,
    pub format: i32,
    pub metadata: rasterm_frame_metadata,
    pub reserved: [u64; 4],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct rasterm_presenter_options {
    pub struct_size: u32,
    pub engine: rasterm_engine_options,
    pub maximum_frames_per_second: f64,
    pub reserved: [u64; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct rasterm_rgb_color {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct rasterm_indexed_frame {
    pub struct_size: u32,
    pub indices: *const u8,
    pub width: i32,
    pub height: i32,
    pub stride: i64,
    pub palette: *const rasterm_rgb_color,
    pub palette_size: usize,
    pub metadata: rasterm_frame_metadata,
    pub reserved: [u64; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct rasterm_render_stats {
    pub struct_size: u32,
    pub error: i32,
    pub rendered: i32,
    pub full_frame: i32,
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
    pub reserved: [u64; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct rasterm_terminal_capabilities {
    pub struct_size: u32,
    pub custom_output: i32,
    pub valid_output_handle: i32,
    pub console_output: i32,
    pub virtual_terminal_output: i32,
    pub sixel: i32,
    pub synchronized_output: i32,
    pub columns: i32,
    pub rows: i32,
    pub pixel_width: i32,
    pub pixel_height: i32,
    pub cell_pixel_width: i32,
    pub cell_pixel_height: i32,
    pub reserved: [u64; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct rasterm_presenter_stats {
    pub struct_size: u32,
    pub submitted_frames: u64,
    pub presented_frames: u64,
    pub replaced_frames: u64,
    pub latest_render: rasterm_render_stats,
    pub reserved: [u64; 8],
}

unsafe extern "C" {
    pub fn rasterm_engine_options_init(options: *mut rasterm_engine_options);
    pub fn rasterm_frame_init(frame: *mut rasterm_frame);
    pub fn rasterm_indexed_frame_init(frame: *mut rasterm_indexed_frame);
    pub fn rasterm_render_stats_init(stats: *mut rasterm_render_stats);
    pub fn rasterm_terminal_capabilities_init(capabilities: *mut rasterm_terminal_capabilities);
    pub fn rasterm_presenter_options_init(options: *mut rasterm_presenter_options);
    pub fn rasterm_presenter_stats_init(stats: *mut rasterm_presenter_stats);
    pub fn rasterm_c_api_version() -> u32;
    pub fn rasterm_version(major: *mut u32, minor: *mut u32, patch: *mut u32);
    pub fn rasterm_last_error(
        buffer: *mut c_char,
        capacity: usize,
        required_size: *mut usize,
    ) -> rasterm_result;
    pub fn rasterm_engine_create(
        options: *const rasterm_engine_options,
        output: *mut *mut rasterm_engine,
    ) -> rasterm_result;
    pub fn rasterm_engine_destroy(engine: *mut rasterm_engine);
    pub fn rasterm_engine_render(
        engine: *mut rasterm_engine,
        frame: *const rasterm_frame,
        stats: *mut rasterm_render_stats,
    ) -> rasterm_result;
    pub fn rasterm_engine_render_indexed(
        engine: *mut rasterm_engine,
        frame: *const rasterm_indexed_frame,
        stats: *mut rasterm_render_stats,
    ) -> rasterm_result;
    pub fn rasterm_engine_clear(engine: *mut rasterm_engine) -> rasterm_result;
    pub fn rasterm_engine_reset(engine: *mut rasterm_engine) -> rasterm_result;
    pub fn rasterm_engine_get_stats(
        engine: *const rasterm_engine,
        stats: *mut rasterm_render_stats,
    ) -> rasterm_result;
    pub fn rasterm_engine_get_capabilities(
        engine: *const rasterm_engine,
        capabilities: *mut rasterm_terminal_capabilities,
    ) -> rasterm_result;
    pub fn rasterm_engine_last_error(
        engine: *const rasterm_engine,
        buffer: *mut c_char,
        capacity: usize,
        required_size: *mut usize,
    ) -> rasterm_result;
    pub fn rasterm_presenter_create(
        options: *const rasterm_presenter_options,
        output: *mut *mut rasterm_presenter,
    ) -> rasterm_result;
    pub fn rasterm_presenter_destroy(presenter: *mut rasterm_presenter);
    pub fn rasterm_presenter_submit(
        presenter: *mut rasterm_presenter,
        frame: *const rasterm_frame,
    ) -> rasterm_result;
    pub fn rasterm_presenter_submit_indexed(
        presenter: *mut rasterm_presenter,
        frame: *const rasterm_indexed_frame,
    ) -> rasterm_result;
    pub fn rasterm_presenter_get_stats(
        presenter: *const rasterm_presenter,
        stats: *mut rasterm_presenter_stats,
    ) -> rasterm_result;
    pub fn rasterm_presenter_last_error(
        presenter: *const rasterm_presenter,
        buffer: *mut c_char,
        capacity: usize,
        required_size: *mut usize,
    ) -> rasterm_result;
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::size_of;

    #[test]
    fn x64_v1_layout_matches_c_api() {
        if size_of::<usize>() != 8 {
            return;
        }
        assert_eq!(size_of::<rasterm_color_metadata>(), 40);
        assert_eq!(size_of::<rasterm_frame_metadata>(), 152);
        assert_eq!(size_of::<rasterm_engine_options>(), 160);
        assert_eq!(size_of::<rasterm_frame>(), 224);
        assert_eq!(size_of::<rasterm_presenter_options>(), 240);
        assert_eq!(size_of::<rasterm_indexed_frame>(), 232);
        assert_eq!(size_of::<rasterm_render_stats>(), 176);
        assert_eq!(size_of::<rasterm_terminal_capabilities>(), 120);
        assert_eq!(size_of::<rasterm_presenter_stats>(), 272);
    }
}
