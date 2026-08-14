# SPDX-License-Identifier: Apache-2.0

import ctypes
import unittest

import rasterm
from rasterm import _ffi
from rasterm.api import _engine_options


class BindingTests(unittest.TestCase):
    def test_v1_x64_layout(self):
        if ctypes.sizeof(ctypes.c_void_p) != 8:
            self.skipTest("the published V1 baseline is x64")
        self.assertEqual(ctypes.sizeof(_ffi.ColorMetadata), 40)
        self.assertEqual(ctypes.sizeof(_ffi.FrameMetadata), 152)
        self.assertEqual(ctypes.sizeof(_ffi.EngineOptions), 160)
        self.assertEqual(ctypes.sizeof(_ffi.Frame), 224)
        self.assertEqual(ctypes.sizeof(_ffi.PresenterOptions), 240)
        self.assertEqual(ctypes.sizeof(_ffi.IndexedFrame), 232)
        self.assertEqual(ctypes.sizeof(_ffi.RenderStats), 176)
        self.assertEqual(ctypes.sizeof(_ffi.TerminalCapabilities), 120)
        self.assertEqual(ctypes.sizeof(_ffi.PresenterStats), 272)

    def test_loaded_abi_and_version(self):
        self.assertEqual(rasterm.version(), (1, 3, 0))

    def test_frame_accepts_read_only_buffer(self):
        library = _ffi.Library()
        frame = rasterm.Frame(bytes(16), 2, 2, 8)
        raw, *_keepalive = frame._native(library)
        self.assertEqual((raw.width, raw.height, raw.stride), (2, 2, 8))
        self.assertEqual(raw.metadata.color.reference_white_nits, 203.0)
        self.assertEqual(raw.metadata.color.transfer, 1)
        self.assertEqual(raw.metadata.source_color.reference_white_nits, 203.0)

    def test_frame_rejects_short_buffer(self):
        library = _ffi.Library()
        frame = rasterm.Frame(bytes(15), 2, 2, 8)
        with self.assertRaises(ValueError):
            frame._native(library)

    def test_safe_defaults_match_c_initializer(self):
        library = _ffi.Library()
        native = _ffi.EngineOptions()
        library.raw.rasterm_engine_options_init(ctypes.byref(native))
        mapped = _engine_options(library, rasterm.EngineOptions())
        fields = (
            "quality", "use_alternate_screen", "preserve_cursor",
            "enable_dirty_regions", "require_sixel_support",
            "use_synchronized_output", "maximum_output_bytes",
            "backpressure_threshold_milliseconds", "convert_to_srgb", "tone_map",
            "output_peak_nits", "realtime_dither", "adaptive_palette_lock_frames",
            "scene_cut_threshold",
        )
        for field in fields:
            self.assertEqual(getattr(mapped, field), getattr(native, field), field)


if __name__ == "__main__":
    unittest.main()
  
