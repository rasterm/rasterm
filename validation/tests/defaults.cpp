/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>
#include <rasterm/rasterm.hpp>

namespace {

bool matches(const rasterm_color_metadata& c, const rasterm::ColorMetadata& cpp)
{
    return c.primaries == static_cast<int>(cpp.primaries) &&
        c.transfer == static_cast<int>(cpp.transfer) &&
        c.matrix == static_cast<int>(cpp.matrix) && c.range == static_cast<int>(cpp.range) &&
        c.reference_white_nits == cpp.referenceWhiteNits &&
        c.mastering_peak_nits == cpp.masteringPeakNits;
}

}

int main()
{
    static_assert(RASTERM_DEFAULT_QUALITY == static_cast<int>(rasterm::QualityProfile::Realtime));
    static_assert(RASTERM_DEFAULT_TONE_MAP == static_cast<int>(rasterm::ToneMapOperator::Aces));
    static_assert(RASTERM_DEFAULT_OUTPUT_CHUNK_BYTES == 64U * 1024U);

    rasterm_engine_options c;
    rasterm_engine_options_init(&c);
    const rasterm::EngineOptions cpp;
    if (c.quality != static_cast<int>(cpp.quality) ||
        (c.use_alternate_screen != 0) != cpp.useAlternateScreen ||
        (c.preserve_cursor != 0) != cpp.preserveCursor ||
        (c.enable_dirty_regions != 0) != cpp.enableDirtyRegions ||
        (c.require_sixel_support != 0) != cpp.requireSixelSupport ||
        (c.use_synchronized_output != 0) != cpp.useSynchronizedOutput ||
        c.maximum_output_bytes != cpp.maximumOutputBytes ||
        c.backpressure_threshold_milliseconds != cpp.backpressureThresholdMilliseconds ||
        (c.convert_to_srgb != 0) != cpp.color.convertToSrgb ||
        c.tone_map != static_cast<int>(cpp.color.toneMap) ||
        c.output_peak_nits != cpp.color.outputPeakNits ||
        c.realtime_dither != static_cast<int>(cpp.color.realtimeDither) ||
        c.adaptive_palette_lock_frames != cpp.color.adaptivePaletteLockFrames ||
        c.scene_cut_threshold != cpp.color.sceneCutThreshold ||
        c.sixel_support_override != static_cast<int>(cpp.terminalOverrides.sixel) ||
        c.synchronized_output_override !=
            static_cast<int>(cpp.terminalOverrides.synchronizedOutput) ||
        c.override_columns != cpp.terminalOverrides.geometry.columns ||
        c.override_cell_pixel_width != cpp.terminalOverrides.geometry.cellPixelWidth ||
        (c.persist_palette_registers != 0) != cpp.encoder.persistPaletteRegisters ||
        c.palette_refresh_frames != cpp.encoder.paletteRefreshFrames ||
        c.output_chunk_bytes != cpp.encoder.outputChunkBytes ||
        c.maximum_encoder_threads != cpp.encoder.maximumThreads ||
        (c.independent_region_quantization != 0) !=
            cpp.encoder.independentRegionQuantization ||
        c.output_context != nullptr || c.write != nullptr || c.flush != nullptr ||
        cpp.quality != static_cast<rasterm::QualityProfile>(RASTERM_DEFAULT_QUALITY) ||
        cpp.encoder.outputChunkBytes != RASTERM_DEFAULT_OUTPUT_CHUNK_BYTES) return 1;

    rasterm_frame frame;
    rasterm_frame_init(&frame);
    const rasterm::FrameMetadata metadata;
    if (frame.data != nullptr || frame.width != 0 || frame.height != 0 || frame.stride != 0 ||
        frame.format != RASTERM_PIXEL_RGB24 ||
        frame.metadata.frame_id != metadata.frameId ||
        frame.metadata.timestamp_nanoseconds != metadata.timestampNanoseconds ||
        !matches(frame.metadata.color, metadata.color) ||
        !matches(frame.metadata.source_color, metadata.sourceColor) ||
        frame.metadata.damage_rects != nullptr || metadata.damage.rectangles != nullptr ||
        frame.metadata.damage_count != metadata.damage.count ||
        (frame.metadata.damage_supplied != 0) != metadata.damage.supplied) return 2;

    rasterm_presenter_options presenter;
    rasterm_presenter_options_init(&presenter);
    const rasterm::PresenterOptions cppPresenter;
    if (presenter.maximum_frames_per_second != cppPresenter.maximumFramesPerSecond ||
        presenter.engine.quality != static_cast<int>(cppPresenter.engine.quality) ||
        (presenter.engine.preserve_cursor != 0) != cppPresenter.engine.preserveCursor) {
        return 3;
    }

    rasterm_indexed_frame indexed;
    rasterm_indexed_frame_init(&indexed);
    const rasterm::IndexedFrameView cppIndexed;
    if (indexed.indices != cppIndexed.indices || indexed.width != cppIndexed.width ||
        indexed.height != cppIndexed.height || indexed.stride != cppIndexed.stride ||
        indexed.palette != nullptr || cppIndexed.palette.colors != nullptr ||
        indexed.palette_size != cppIndexed.palette.size ||
        !matches(indexed.metadata.color, cppIndexed.metadata.color)) return 4;

    rasterm_render_stats stats;
    rasterm_render_stats_init(&stats);
    const rasterm::RenderStats cppStats;
    if (stats.error != static_cast<int>(cppStats.error) ||
        (stats.rendered != 0) != cppStats.rendered ||
        stats.payload_bytes != cppStats.payloadBytes || stats.width != cppStats.width ||
        stats.output_failures != cppStats.outputFailures ||
        stats.wire_bytes != cppStats.wireBytes || stats.scratch_bytes != cppStats.scratchBytes ||
        stats.validation_milliseconds != cppStats.validationMilliseconds) return 5;

    rasterm_terminal_capabilities capabilities;
    rasterm_terminal_capabilities_init(&capabilities);
    const rasterm::TerminalCapabilities cppCapabilities;
    if ((capabilities.custom_output != 0) != cppCapabilities.customOutput ||
        capabilities.sixel != static_cast<int>(cppCapabilities.sixel) ||
        capabilities.columns != cppCapabilities.geometry.columns ||
        capabilities.pixel_width != cppCapabilities.geometry.pixelWidth) return 6;

    rasterm_presenter_stats presenterStats;
    rasterm_presenter_stats_init(&presenterStats);
    const rasterm::PresenterStats cppPresenterStats;
    if (presenterStats.submitted_frames != cppPresenterStats.submittedFrames ||
        presenterStats.presented_frames != cppPresenterStats.presentedFrames ||
        presenterStats.replaced_frames != cppPresenterStats.replacedFrames ||
        presenterStats.unchanged_frames != cppPresenterStats.unchangedFrames ||
        presenterStats.failed_frames != cppPresenterStats.failedFrames ||
        presenterStats.rejected_frames != cppPresenterStats.rejectedFrames ||
        presenterStats.cancelled_frames != cppPresenterStats.cancelledFrames ||
        presenterStats.latest_render.error != static_cast<int>(cppPresenterStats.latestRender.error)) {
        return 7;
    }
    return 0;
}
