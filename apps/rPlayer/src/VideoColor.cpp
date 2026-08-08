/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/VideoColor.hpp>

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
}

namespace rasterm::rPlayer {
namespace {

ColorPrimaries primaries(const AVColorPrimaries value) noexcept
{
    switch (value) {
    case AVCOL_PRI_BT709: return ColorPrimaries::Bt709;
    case AVCOL_PRI_BT2020: return ColorPrimaries::Bt2020;
    case AVCOL_PRI_SMPTE432: return ColorPrimaries::DisplayP3;
    default: return ColorPrimaries::Unspecified;
    }
}

TransferFunction transfer(const AVColorTransferCharacteristic value) noexcept
{
    switch (value) {
    case AVCOL_TRC_BT709: return TransferFunction::Bt709;
    case AVCOL_TRC_IEC61966_2_1: return TransferFunction::Srgb;
    case AVCOL_TRC_LINEAR: return TransferFunction::Linear;
    case AVCOL_TRC_GAMMA22: return TransferFunction::Gamma22;
    case AVCOL_TRC_SMPTE2084: return TransferFunction::Pq;
    case AVCOL_TRC_ARIB_STD_B67: return TransferFunction::Hlg;
    default: return TransferFunction::Unspecified;
    }
}

MatrixCoefficients matrix(const AVColorSpace value) noexcept
{
    switch (value) {
    case AVCOL_SPC_RGB: return MatrixCoefficients::Identity;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M: return MatrixCoefficients::Bt601;
    case AVCOL_SPC_BT709: return MatrixCoefficients::Bt709;
    case AVCOL_SPC_BT2020_NCL: return MatrixCoefficients::Bt2020NonConstant;
    default: return MatrixCoefficients::Unspecified;
    }
}

ColorRange range(const AVColorRange value) noexcept
{
    switch (value) {
    case AVCOL_RANGE_MPEG: return ColorRange::Limited;
    case AVCOL_RANGE_JPEG: return ColorRange::Full;
    default: return ColorRange::Unspecified;
    }
}

}

ColorMetadata probeVideoColor(const std::string& path) noexcept
{
    ColorMetadata result{
        .primaries = ColorPrimaries::Unspecified,
        .transfer = TransferFunction::Unspecified,
        .matrix = MatrixCoefficients::Unspecified,
        .range = ColorRange::Unspecified,
    };
    AVFormatContext* format = nullptr;
    if (avformat_open_input(&format, path.c_str(), nullptr, nullptr) < 0 || format == nullptr) {
        return result;
    }
    if (avformat_find_stream_info(format, nullptr) >= 0) {
        const int streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (streamIndex >= 0) {
            const AVCodecParameters* parameters = format->streams[streamIndex]->codecpar;
            result.primaries = primaries(parameters->color_primaries);
            result.transfer = transfer(parameters->color_trc);
            result.matrix = matrix(parameters->color_space);
            result.range = range(parameters->color_range);
        }
    }
    avformat_close_input(&format);
    return result;
}

ColorMetadata decodedRgbColor(ColorMetadata source) noexcept
{
    source.matrix = MatrixCoefficients::Identity;
    source.range = ColorRange::Full;
    if (source.primaries == ColorPrimaries::Unspecified) source.primaries = ColorPrimaries::Bt709;
    if (source.transfer == TransferFunction::Unspecified) source.transfer = TransferFunction::Srgb;
    return source;
}

}