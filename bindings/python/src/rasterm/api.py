# SPDX-License-Identifier: Apache-2.0 

from __future__ import annotations

import ctypes as c
from dataclasses import dataclass
from enum import IntEnum
from os import PathLike
from typing import Any, Iterable, Optional, Sequence, Tuple, Union

from . import _ffi

Buffer = Any
LibraryPath = Optional[Union[PathLike[str], str]]


class PixelFormat(IntEnum):
    RGB24 = 0
    BGR24 = 1
    RGBA32 = 2
    BGRA32 = 3
    RGB565 = 4
    XRGB1555 = 5
    RGBA4444 = 6

    @property
    def bytes_per_pixel(self) -> int:
        return 3 if self in (self.RGB24, self.BGR24) else 4 if self in (
            self.RGBA32, self.BGRA32) else 2


class QualityProfile(IntEnum):
    REALTIME = 0
    ADAPTIVE_VIDEO = 1
    HIGH = 2


class ToneMapOperator(IntEnum):
    NONE = 0
    REINHARD = 1
    HABLE = 2
    ACES = 3


class DitherMode(IntEnum):
    NONE = 0
    ORDERED_BAYER_4X4 = 1
    FLOYD_STEINBERG = 2


class ColorPrimaries(IntEnum):
    UNSPECIFIED = 0
    BT709 = 1
    BT2020 = 2
    DISPLAY_P3 = 3


class TransferFunction(IntEnum):
    UNSPECIFIED = 0
    SRGB = 1
    LINEAR = 2
    BT709 = 3
    GAMMA22 = 4
    PQ = 5
    HLG = 6


class MatrixCoefficients(IntEnum):
    UNSPECIFIED = 0
    IDENTITY = 1
    BT601 = 2
    BT709 = 3
    BT2020_NCL = 4


class ColorRange(IntEnum):
    UNSPECIFIED = 0
    LIMITED = 1
    FULL = 2


@dataclass(frozen=True)
class ColorMetadata:
    primaries: ColorPrimaries = ColorPrimaries.UNSPECIFIED
    transfer: TransferFunction = TransferFunction.UNSPECIFIED
    matrix: MatrixCoefficients = MatrixCoefficients.UNSPECIFIED
    range: ColorRange = ColorRange.UNSPECIFIED
    reference_white_nits: float = 203.0
    mastering_peak_nits: float = 1000.0


_DEFAULT_OUTPUT_COLOR = ColorMetadata(
    ColorPrimaries.BT709,
    TransferFunction.SRGB,
    MatrixCoefficients.IDENTITY,
    ColorRange.FULL,
)
_DEFAULT_SOURCE_COLOR = ColorMetadata()


@dataclass(frozen=True)
class DamageRect:
    x: int
    y: int
    width: int
    height: int


@dataclass(frozen=True)
class RgbColor:
    red: int
    green: int
    blue: int


@dataclass
class EngineOptions:
    quality: QualityProfile = QualityProfile.REALTIME
    use_alternate_screen: bool = False
    preserve_cursor: bool = True
    enable_dirty_regions: bool = False
    require_sixel_support: bool = False
    use_synchronized_output: bool = True
    maximum_output_bytes: int = 0
    backpressure_threshold_milliseconds: float = 12.0
    convert_to_srgb: bool = True
    tone_map: ToneMapOperator = ToneMapOperator.ACES
    output_peak_nits: float = 203.0
    realtime_dither: DitherMode = DitherMode.NONE
    adaptive_palette_lock_frames: int = 12
    scene_cut_threshold: float = 0.30


@dataclass
class PresenterOptions:
    engine: EngineOptions
    maximum_frames_per_second: float = 0.0

    def __init__(self, engine: Optional[EngineOptions] = None,
                 maximum_frames_per_second: float = 0.0):
        self.engine = engine if engine is not None else EngineOptions()
        self.maximum_frames_per_second = maximum_frames_per_second


@dataclass(frozen=True)
class RenderStats:
    rendered: bool
    full_frame: bool
    payload_bytes: int
    dirty_regions: int
    width: int
    height: int
    colors_used: int
    encode_milliseconds: float
    present_milliseconds: float
    frames_per_second: float
    payload_bytes_per_second: float
    terminal_write_p95_milliseconds: float
    terminal_write_p99_milliseconds: float
    output_failures: int
    backpressure_events: int
    payload_limit_drops: int


@dataclass(frozen=True)
class PresenterStats:
    submitted_frames: int
    presented_frames: int
    replaced_frames: int
    latest_render: RenderStats


@dataclass(frozen=True)
class TerminalCapabilities:
    custom_output: bool
    valid_output_handle: bool
    console_output: bool
    virtual_terminal_output: bool
    sixel: int
    synchronized_output: int
    columns: int
    rows: int
    pixel_width: int
    pixel_height: int
    cell_pixel_width: int
    cell_pixel_height: int


class RastermError(RuntimeError):
    def __init__(self, code: int, message: str):
        self.code = code
        super().__init__(f"rasterm error {code}: {message}")


class _BufferView:
    def __init__(self, source: Buffer):
        try:
            view = memoryview(source)
        except TypeError as error:
            raise TypeError("pixels must support Python's buffer protocol") from error
        if not view.c_contiguous:
            raise ValueError("pixels must be C contiguous")
        self.view = view.cast("B")
        array_type = c.c_uint8 * self.view.nbytes
        self.array = (array_type.from_buffer_copy(self.view) if self.view.readonly
                      else array_type.from_buffer(self.view))

    @property
    def pointer(self):
        return c.cast(self.array, c.POINTER(c.c_uint8))


class Frame:
    def __init__(self, pixels: Buffer, width: int, height: int, stride: int,
                 format: PixelFormat = PixelFormat.RGBA32, *, frame_id: int = 0,
                 timestamp_nanoseconds: int = -(1 << 63),
                 damage: Optional[Iterable[DamageRect]] = None,
                 color: ColorMetadata = _DEFAULT_OUTPUT_COLOR,
                 source_color: ColorMetadata = _DEFAULT_SOURCE_COLOR):
        self.pixels = pixels
        self.width = width
        self.height = height
        self.stride = stride
        self.format = PixelFormat(format)
        self.frame_id = frame_id
        self.timestamp_nanoseconds = timestamp_nanoseconds
        self.damage = None if damage is None else tuple(damage)
        self.color = color
        self.source_color = source_color

    def _native(self, library: _ffi.Library):
        view = _BufferView(self.pixels)
        _validate_surface(view.view.nbytes, self.width, self.height, self.stride,
                          self.format.bytes_per_pixel)
        raw = _ffi.Frame()
        library.raw.rasterm_frame_init(c.byref(raw))
        raw.data = view.pointer
        raw.width = self.width
        raw.height = self.height
        raw.stride = self.stride
        raw.format = int(self.format)
        damage = _apply_metadata(raw.metadata, self.frame_id,
                                 self.timestamp_nanoseconds, self.damage,
                                 self.color, self.source_color)
        return raw, view, damage


class IndexedFrame:
    def __init__(self, indices: Buffer, width: int, height: int, stride: int,
                 palette: Sequence[RgbColor], *, frame_id: int = 0,
                 timestamp_nanoseconds: int = -(1 << 63),
                 damage: Optional[Iterable[DamageRect]] = None,
                 color: ColorMetadata = _DEFAULT_OUTPUT_COLOR,
                 source_color: ColorMetadata = _DEFAULT_SOURCE_COLOR):
        if len(palette) > 256:
            raise ValueError("an indexed palette cannot contain more than 256 colors")
        self.indices = indices
        self.width = width
        self.height = height
        self.stride = stride
        self.palette = tuple(palette)
        self.frame_id = frame_id
        self.timestamp_nanoseconds = timestamp_nanoseconds
        self.damage = None if damage is None else tuple(damage)
        self.color = color
        self.source_color = source_color

    def _native(self, library: _ffi.Library):
        view = _BufferView(self.indices)
        _validate_surface(view.view.nbytes, self.width, self.height, self.stride, 1)
        palette_type = _ffi.RgbColor * len(self.palette)
        palette = palette_type(*(
            _ffi.RgbColor(color.red, color.green, color.blue) for color in self.palette))
        raw = _ffi.IndexedFrame()
        library.raw.rasterm_indexed_frame_init(c.byref(raw))
        raw.indices = view.pointer
        raw.width = self.width
        raw.height = self.height
        raw.stride = self.stride
        raw.palette = c.cast(palette, c.POINTER(_ffi.RgbColor))
        raw.palette_size = len(palette)
        damage = _apply_metadata(raw.metadata, self.frame_id,
                                 self.timestamp_nanoseconds, self.damage,
                                 self.color, self.source_color)
        return raw, view, palette, damage


def _validate_surface(length: int, width: int, height: int,
                      stride: int, bytes_per_pixel: int):
    if not 0 < width <= 0x7fffffff or not 0 < height <= 0x7fffffff:
        raise ValueError("frame dimensions must be positive signed 32-bit values")
    row_bytes = width * bytes_per_pixel
    if stride < row_bytes or stride > 0x7fffffffffffffff:
        raise ValueError("frame stride is smaller than one pixel row")
    if length < stride * (height - 1) + row_bytes:
        raise ValueError("pixel buffer is smaller than the described frame")


def _apply_metadata(raw: _ffi.FrameMetadata, frame_id: int, timestamp: int,
                    damage: Optional[Tuple[DamageRect, ...]], color: ColorMetadata,
                    source_color: ColorMetadata):
    raw.frame_id = frame_id
    raw.timestamp_nanoseconds = timestamp
    _apply_color_metadata(raw.color, color)
    _apply_color_metadata(raw.source_color, source_color)
    if damage is None:
        return None
    damage_type = _ffi.DamageRect * len(damage)
    native = damage_type(*(
        _ffi.DamageRect(rect.x, rect.y, rect.width, rect.height) for rect in damage))
    raw.damage_rects = c.cast(native, c.POINTER(_ffi.DamageRect))
    raw.damage_count = len(native)
    raw.damage_supplied = 1
    return native


def _apply_color_metadata(raw: _ffi.ColorMetadata, metadata: ColorMetadata):
    raw.primaries = int(metadata.primaries)
    raw.transfer = int(metadata.transfer)
    raw.matrix = int(metadata.matrix)
    raw.range = int(metadata.range)
    raw.reference_white_nits = metadata.reference_white_nits
    raw.mastering_peak_nits = metadata.mastering_peak_nits


def _engine_options(library: _ffi.Library, options: EngineOptions):
    raw = _ffi.EngineOptions()
    library.raw.rasterm_engine_options_init(c.byref(raw))
    raw.quality = int(options.quality)
    raw.use_alternate_screen = options.use_alternate_screen
    raw.preserve_cursor = options.preserve_cursor
    raw.enable_dirty_regions = options.enable_dirty_regions
    raw.require_sixel_support = options.require_sixel_support
    raw.use_synchronized_output = options.use_synchronized_output
    raw.maximum_output_bytes = options.maximum_output_bytes
    raw.backpressure_threshold_milliseconds = options.backpressure_threshold_milliseconds
    raw.convert_to_srgb = options.convert_to_srgb
    raw.tone_map = int(options.tone_map)
    raw.output_peak_nits = options.output_peak_nits
    raw.realtime_dither = int(options.realtime_dither)
    raw.adaptive_palette_lock_frames = options.adaptive_palette_lock_frames
    raw.scene_cut_threshold = options.scene_cut_threshold
    return raw


def _render_stats(raw: _ffi.RenderStats):
    return RenderStats(
        bool(raw.rendered), bool(raw.full_frame), raw.payload_bytes,
        raw.dirty_regions, raw.width, raw.height, raw.colors_used,
        raw.encode_milliseconds, raw.present_milliseconds, raw.frames_per_second,
        raw.payload_bytes_per_second, raw.terminal_write_p95_milliseconds,
        raw.terminal_write_p99_milliseconds, raw.output_failures,
        raw.backpressure_events, raw.payload_limit_drops)


class Engine:
    def __init__(self, options: Optional[EngineOptions] = None, *, library: LibraryPath = None):
        self._library = _ffi.Library(library)
        self._handle = c.c_void_p()
        raw_options = _engine_options(self._library, options or EngineOptions())
        code = self._library.raw.rasterm_engine_create(
            c.byref(raw_options), c.byref(self._handle))
        if code != _ffi.SUCCESS:
            message = self._error_message()
            self.close()
            raise RastermError(code, message)

    def render(self, frame: Frame) -> RenderStats:
        self._require_open()
        raw, *_keepalive = frame._native(self._library)
        stats = _ffi.RenderStats()
        self._library.raw.rasterm_render_stats_init(c.byref(stats))
        code = self._library.raw.rasterm_engine_render(
            self._handle, c.byref(raw), c.byref(stats))
        self._check(code)
        return _render_stats(stats)

    def render_indexed(self, frame: IndexedFrame) -> RenderStats:
        self._require_open()
        raw, *_keepalive = frame._native(self._library)
        stats = _ffi.RenderStats()
        self._library.raw.rasterm_render_stats_init(c.byref(stats))
        code = self._library.raw.rasterm_engine_render_indexed(
            self._handle, c.byref(raw), c.byref(stats))
        self._check(code)
        return _render_stats(stats)

    def clear(self):
        self._require_open()
        self._check(self._library.raw.rasterm_engine_clear(self._handle))

    def reset(self):
        self._require_open()
        self._check(self._library.raw.rasterm_engine_reset(self._handle))

    def stats(self) -> RenderStats:
        self._require_open()
        stats = _ffi.RenderStats()
        self._library.raw.rasterm_render_stats_init(c.byref(stats))
        self._check(self._library.raw.rasterm_engine_get_stats(
            self._handle, c.byref(stats)))
        return _render_stats(stats)

    def capabilities(self) -> TerminalCapabilities:
        self._require_open()
        raw = _ffi.TerminalCapabilities()
        self._library.raw.rasterm_terminal_capabilities_init(c.byref(raw))
        self._check(self._library.raw.rasterm_engine_get_capabilities(
            self._handle, c.byref(raw)))
        return TerminalCapabilities(
            bool(raw.custom_output), bool(raw.valid_output_handle),
            bool(raw.console_output), bool(raw.virtual_terminal_output),
            raw.sixel, raw.synchronized_output, raw.columns, raw.rows,
            raw.pixel_width, raw.pixel_height, raw.cell_pixel_width, raw.cell_pixel_height)

    def close(self):
        if getattr(self, "_handle", None) and self._handle.value:
            self._library.raw.rasterm_engine_destroy(self._handle)
            self._handle = c.c_void_p()

    def _check(self, code: int):
        if code != _ffi.SUCCESS:
            raise RastermError(code, self._error_message())

    def _error_message(self):
        if self._handle.value:
            return self._library.error_message(
                self._library.raw.rasterm_engine_last_error, self._handle)
        return "rasterm could not create an engine"

    def _require_open(self):
        if not self._handle.value:
            raise RuntimeError("rasterm Engine is closed")

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class Presenter:
    def __init__(self, options: Optional[PresenterOptions] = None, *, library: LibraryPath = None):
        self._library = _ffi.Library(library)
        self._handle = c.c_void_p()
        options = options or PresenterOptions()
        raw_options = _ffi.PresenterOptions()
        self._library.raw.rasterm_presenter_options_init(c.byref(raw_options))
        raw_options.engine = _engine_options(self._library, options.engine)
        raw_options.maximum_frames_per_second = options.maximum_frames_per_second
        code = self._library.raw.rasterm_presenter_create(
            c.byref(raw_options), c.byref(self._handle))
        if code != _ffi.SUCCESS:
            message = self._error_message()
            self.close()
            raise RastermError(code, message)

    def submit(self, frame: Frame):
        self._require_open()
        raw, *_keepalive = frame._native(self._library)
        self._check(self._library.raw.rasterm_presenter_submit(
            self._handle, c.byref(raw)))

    def submit_indexed(self, frame: IndexedFrame):
        self._require_open()
        raw, *_keepalive = frame._native(self._library)
        self._check(self._library.raw.rasterm_presenter_submit_indexed(
            self._handle, c.byref(raw)))

    def stats(self) -> PresenterStats:
        self._require_open()
        raw = _ffi.PresenterStats()
        self._library.raw.rasterm_presenter_stats_init(c.byref(raw))
        self._check(self._library.raw.rasterm_presenter_get_stats(
            self._handle, c.byref(raw)))
        return PresenterStats(raw.submitted_frames, raw.presented_frames,
                              raw.replaced_frames, _render_stats(raw.latest_render))

    def close(self):
        if getattr(self, "_handle", None) and self._handle.value:
            self._library.raw.rasterm_presenter_destroy(self._handle)
            self._handle = c.c_void_p()

    def _check(self, code: int):
        if code != _ffi.SUCCESS:
            raise RastermError(code, self._error_message())

    def _error_message(self):
        if self._handle.value:
            return self._library.error_message(
                self._library.raw.rasterm_presenter_last_error, self._handle)
        return "rasterm could not create a presenter"

    def _require_open(self):
        if not self._handle.value:
            raise RuntimeError("rasterm Presenter is closed")

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


def version(*, library: LibraryPath = None):
    return _ffi.Library(library).version()
