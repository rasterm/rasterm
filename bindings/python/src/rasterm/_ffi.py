# SPDX-License-Identifier: Apache-2.0 

from __future__ import annotations

import ctypes as c
import os
from pathlib import Path
from typing import Optional, Union

C_API_VERSION = 1
SUCCESS = 0


class ColorMetadata(c.Structure):
    _fields_ = [
        ("primaries", c.c_int32), ("transfer", c.c_int32),
        ("matrix", c.c_int32), ("range", c.c_int32),
        ("reference_white_nits", c.c_float), ("mastering_peak_nits", c.c_float),
        ("reserved", c.c_uint64 * 2),
    ]


class DamageRect(c.Structure):
    _fields_ = [("x", c.c_int32), ("y", c.c_int32),
                ("width", c.c_int32), ("height", c.c_int32)]


class FrameMetadata(c.Structure):
    _fields_ = [
        ("frame_id", c.c_uint64), ("timestamp_nanoseconds", c.c_int64),
        ("color", ColorMetadata), ("source_color", ColorMetadata),
        ("damage_rects", c.POINTER(DamageRect)), ("damage_count", c.c_size_t),
        ("damage_supplied", c.c_int32), ("reserved", c.c_uint64 * 4),
    ]


class EngineOptions(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("api_version", c.c_uint32),
        ("quality", c.c_int32), ("use_alternate_screen", c.c_int32),
        ("preserve_cursor", c.c_int32), ("enable_dirty_regions", c.c_int32),
        ("require_sixel_support", c.c_int32), ("use_synchronized_output", c.c_int32),
        ("maximum_output_bytes", c.c_uint64),
        ("backpressure_threshold_milliseconds", c.c_double),
        ("convert_to_srgb", c.c_int32), ("tone_map", c.c_int32),
        ("output_peak_nits", c.c_float), ("realtime_dither", c.c_int32),
        ("adaptive_palette_lock_frames", c.c_int32), ("scene_cut_threshold", c.c_float),
        ("output_context", c.c_void_p), ("write", c.c_void_p), ("flush", c.c_void_p),
        ("reserved", c.c_uint64 * 8),
    ]


class Frame(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("data", c.POINTER(c.c_uint8)),
        ("width", c.c_int32), ("height", c.c_int32), ("stride", c.c_int64),
        ("format", c.c_int32), ("metadata", FrameMetadata),
        ("reserved", c.c_uint64 * 4),
    ]


class PresenterOptions(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("engine", EngineOptions),
        ("maximum_frames_per_second", c.c_double), ("reserved", c.c_uint64 * 8),
    ]


class RgbColor(c.Structure):
    _fields_ = [("red", c.c_uint8), ("green", c.c_uint8), ("blue", c.c_uint8)]


class IndexedFrame(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("indices", c.POINTER(c.c_uint8)),
        ("width", c.c_int32), ("height", c.c_int32), ("stride", c.c_int64),
        ("palette", c.POINTER(RgbColor)), ("palette_size", c.c_size_t),
        ("metadata", FrameMetadata), ("reserved", c.c_uint64 * 4),
    ]


class RenderStats(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("error", c.c_int32),
        ("rendered", c.c_int32), ("full_frame", c.c_int32),
        ("payload_bytes", c.c_uint64), ("dirty_regions", c.c_uint32),
        ("width", c.c_int32), ("height", c.c_int32), ("colors_used", c.c_int32),
        ("encode_milliseconds", c.c_double), ("present_milliseconds", c.c_double),
        ("frames_per_second", c.c_double), ("payload_bytes_per_second", c.c_double),
        ("terminal_write_p95_milliseconds", c.c_double),
        ("terminal_write_p99_milliseconds", c.c_double),
        ("output_failures", c.c_uint64), ("backpressure_events", c.c_uint64),
        ("payload_limit_drops", c.c_uint64), ("reserved", c.c_uint64 * 8),
    ]


class TerminalCapabilities(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("custom_output", c.c_int32),
        ("valid_output_handle", c.c_int32), ("console_output", c.c_int32),
        ("virtual_terminal_output", c.c_int32), ("sixel", c.c_int32),
        ("synchronized_output", c.c_int32), ("columns", c.c_int32),
        ("rows", c.c_int32), ("pixel_width", c.c_int32),
        ("pixel_height", c.c_int32), ("cell_pixel_width", c.c_int32),
        ("cell_pixel_height", c.c_int32), ("reserved", c.c_uint64 * 8),
    ]


class PresenterStats(c.Structure):
    _fields_ = [
        ("struct_size", c.c_uint32), ("submitted_frames", c.c_uint64),
        ("presented_frames", c.c_uint64), ("replaced_frames", c.c_uint64),
        ("latest_render", RenderStats), ("reserved", c.c_uint64 * 8),
    ]


def _candidate_libraries(explicit: Optional[Union[os.PathLike[str], str]]):
    if explicit is not None:
        yield Path(explicit)
        return
    configured = os.environ.get("RASTERM_LIBRARY")
    if configured:
        yield Path(configured)
    yield Path(__file__).parent / "_native" / "rasterm.dll"


class Library:
    def __init__(self, path: Optional[Union[os.PathLike[str], str]] = None):
        failures = []
        self.raw = None
        for candidate in _candidate_libraries(path):
            try:
                self.raw = c.CDLL(str(candidate))
                break
            except OSError as error:
                failures.append(str(error))
        if self.raw is None:
            detail = "; ".join(failures) or "no library candidate was available"
            raise OSError("rasterm.dll could not be loaded; set RASTERM_LIBRARY: " + detail)
        self._configure()
        version = self.raw.rasterm_c_api_version()
        if version != C_API_VERSION:
            raise RuntimeError(
                f"Python binding requires rasterm C API {C_API_VERSION}, loaded {version}")

    def _configure(self):
        dll = self.raw
        dll.rasterm_c_api_version.restype = c.c_uint32
        dll.rasterm_version.argtypes = [c.POINTER(c.c_uint32)] * 3
        dll.rasterm_engine_options_init.argtypes = [c.POINTER(EngineOptions)]
        dll.rasterm_frame_init.argtypes = [c.POINTER(Frame)]
        dll.rasterm_indexed_frame_init.argtypes = [c.POINTER(IndexedFrame)]
        dll.rasterm_render_stats_init.argtypes = [c.POINTER(RenderStats)]
        dll.rasterm_terminal_capabilities_init.argtypes = [c.POINTER(TerminalCapabilities)]
        dll.rasterm_presenter_options_init.argtypes = [c.POINTER(PresenterOptions)]
        dll.rasterm_presenter_stats_init.argtypes = [c.POINTER(PresenterStats)]
        dll.rasterm_engine_create.argtypes = [c.POINTER(EngineOptions), c.POINTER(c.c_void_p)]
        dll.rasterm_engine_create.restype = c.c_int32
        dll.rasterm_engine_destroy.argtypes = [c.c_void_p]
        dll.rasterm_engine_render.argtypes = [c.c_void_p, c.POINTER(Frame), c.POINTER(RenderStats)]
        dll.rasterm_engine_render.restype = c.c_int32
        dll.rasterm_engine_render_indexed.argtypes = [c.c_void_p, c.POINTER(IndexedFrame), c.POINTER(RenderStats)]
        dll.rasterm_engine_render_indexed.restype = c.c_int32
        dll.rasterm_engine_clear.argtypes = [c.c_void_p]
        dll.rasterm_engine_clear.restype = c.c_int32
        dll.rasterm_engine_reset.argtypes = [c.c_void_p]
        dll.rasterm_engine_reset.restype = c.c_int32
        dll.rasterm_engine_get_stats.argtypes = [c.c_void_p, c.POINTER(RenderStats)]
        dll.rasterm_engine_get_stats.restype = c.c_int32
        dll.rasterm_engine_get_capabilities.argtypes = [c.c_void_p, c.POINTER(TerminalCapabilities)]
        dll.rasterm_engine_get_capabilities.restype = c.c_int32
        dll.rasterm_engine_last_error.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
        dll.rasterm_engine_last_error.restype = c.c_int32
        dll.rasterm_presenter_create.argtypes = [c.POINTER(PresenterOptions), c.POINTER(c.c_void_p)]
        dll.rasterm_presenter_create.restype = c.c_int32
        dll.rasterm_presenter_destroy.argtypes = [c.c_void_p]
        dll.rasterm_presenter_submit.argtypes = [c.c_void_p, c.POINTER(Frame)]
        dll.rasterm_presenter_submit.restype = c.c_int32
        dll.rasterm_presenter_submit_indexed.argtypes = [c.c_void_p, c.POINTER(IndexedFrame)]
        dll.rasterm_presenter_submit_indexed.restype = c.c_int32
        dll.rasterm_presenter_get_stats.argtypes = [c.c_void_p, c.POINTER(PresenterStats)]
        dll.rasterm_presenter_get_stats.restype = c.c_int32
        dll.rasterm_presenter_last_error.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
        dll.rasterm_presenter_last_error.restype = c.c_int32

    def version(self):
        major, minor, patch = c.c_uint32(), c.c_uint32(), c.c_uint32()
        self.raw.rasterm_version(c.byref(major), c.byref(minor), c.byref(patch))
        return major.value, minor.value, patch.value

    @staticmethod
    def error_message(function, handle):
        required = c.c_size_t()
        function(handle, None, 0, c.byref(required))
        if required.value == 0:
            return "rasterm reported an error without a diagnostic"
        buffer = c.create_string_buffer(required.value)
        function(handle, buffer, len(buffer), c.byref(required))
        return buffer.value.decode("utf-8", errors="replace")
