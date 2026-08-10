# SPDX-License-Identifier: Apache-2.0 

from dataclasses import dataclass
from enum import IntEnum
from os import PathLike
from typing import Any, Iterable, Optional, Sequence, Tuple, Union

class PixelFormat(IntEnum):
    RGB24: int
    BGR24: int
    RGBA32: int
    BGRA32: int
    RGB565: int
    XRGB1555: int
    RGBA4444: int
    @property
    def bytes_per_pixel(self) -> int: ...

class QualityProfile(IntEnum):
    REALTIME: int
    ADAPTIVE_VIDEO: int
    HIGH: int

class ToneMapOperator(IntEnum):
    NONE: int
    REINHARD: int
    HABLE: int
    ACES: int

class DitherMode(IntEnum):
    NONE: int
    ORDERED_BAYER_4X4: int
    FLOYD_STEINBERG: int

class ColorPrimaries(IntEnum):
    UNSPECIFIED: int
    BT709: int
    BT2020: int
    DISPLAY_P3: int

class TransferFunction(IntEnum):
    UNSPECIFIED: int
    SRGB: int
    LINEAR: int
    BT709: int
    GAMMA22: int
    PQ: int
    HLG: int

class MatrixCoefficients(IntEnum):
    UNSPECIFIED: int
    IDENTITY: int
    BT601: int
    BT709: int
    BT2020_NCL: int

class ColorRange(IntEnum):
    UNSPECIFIED: int
    LIMITED: int
    FULL: int

@dataclass(frozen=True)
class ColorMetadata:
    primaries: ColorPrimaries = ...
    transfer: TransferFunction = ...
    matrix: MatrixCoefficients = ...
    range: ColorRange = ...
    reference_white_nits: float = ...
    mastering_peak_nits: float = ...

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
    quality: QualityProfile = ...
    use_alternate_screen: bool = ...
    preserve_cursor: bool = ...
    enable_dirty_regions: bool = ...
    require_sixel_support: bool = ...
    use_synchronized_output: bool = ...
    maximum_output_bytes: int = ...
    backpressure_threshold_milliseconds: float = ...
    convert_to_srgb: bool = ...
    tone_map: ToneMapOperator = ...
    output_peak_nits: float = ...
    realtime_dither: DitherMode = ...
    adaptive_palette_lock_frames: int = ...
    scene_cut_threshold: float = ...

@dataclass
class PresenterOptions:
    engine: EngineOptions
    maximum_frames_per_second: float
    def __init__(self, engine: Optional[EngineOptions] = ...,
                 maximum_frames_per_second: float = ...) -> None: ...

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
    code: int

class Frame:
    def __init__(self, pixels: Any, width: int, height: int, stride: int,
                 format: PixelFormat = ..., *, frame_id: int = ...,
                 timestamp_nanoseconds: int = ...,
                 damage: Optional[Iterable[DamageRect]] = ...,
                 color: ColorMetadata = ...,
                 source_color: ColorMetadata = ...) -> None: ...

class IndexedFrame:
    def __init__(self, indices: Any, width: int, height: int, stride: int,
                 palette: Sequence[RgbColor], *, frame_id: int = ...,
                 timestamp_nanoseconds: int = ...,
                 damage: Optional[Iterable[DamageRect]] = ...,
                 color: ColorMetadata = ...,
                 source_color: ColorMetadata = ...) -> None: ...

class Engine:
    def __init__(self, options: Optional[EngineOptions] = ..., *,
                 library: Optional[Union[PathLike[str], str]] = ...) -> None: ...
    def render(self, frame: Frame) -> RenderStats: ...
    def render_indexed(self, frame: IndexedFrame) -> RenderStats: ...
    def clear(self) -> None: ...
    def reset(self) -> None: ...
    def stats(self) -> RenderStats: ...
    def capabilities(self) -> TerminalCapabilities: ...
    def close(self) -> None: ...
    def __enter__(self) -> Engine: ...
    def __exit__(self, *args: Any) -> None: ...

class Presenter:
    def __init__(self, options: Optional[PresenterOptions] = ..., *,
                 library: Optional[Union[PathLike[str], str]] = ...) -> None: ...
    def submit(self, frame: Frame) -> None: ...
    def submit_indexed(self, frame: IndexedFrame) -> None: ...
    def stats(self) -> PresenterStats: ...
    def close(self) -> None: ...
    def __enter__(self) -> Presenter: ...
    def __exit__(self, *args: Any) -> None: ...

def version(*, library: Optional[Union[PathLike[str], str]] = ...) -> Tuple[int, int, int]: ...

__version__: str
