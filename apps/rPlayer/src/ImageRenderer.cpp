/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/ImageRenderer.hpp>
#include <rPlayer/IccImageLoader.hpp>

#include <rasterm/rasterm.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>

namespace rasterm::rPlayer {

namespace {
int interpolation(const ScaleFilter filter)
{
    switch (filter) {
    case ScaleFilter::Nearest: return cv::INTER_NEAREST;
    case ScaleFilter::Linear: return cv::INTER_LINEAR;
    case ScaleFilter::Cubic: return cv::INTER_CUBIC;
    case ScaleFilter::Area: default: return cv::INTER_AREA;
    }
}
}

bool renderImageFile(const std::string& path, const ImageOptions& options)
{
    const cv::Mat bgr = loadColorManagedBgr(path);
    if (bgr.empty()) {
        std::cerr << "Failed to load image: " << path << '\n';
        return false;
    }

    Engine engine;
    const Status initialization = engine.initialize({ .quality = QualityProfile::HighQuality });
    if (!initialization) {
        std::cerr << "rasterm failed to initialize: " << initialization.message << '\n';
        return false;
    }
    const TerminalGeometry terminal = engine.terminalGeometry();
    const Extent bounds{ terminal.pixelWidth, terminal.pixelHeight / 6 * 6 };
    const ScaleLayout layout = calculateScaleLayout({ bgr.cols, bgr.rows }, bounds,
                                                    options.scaling.policy);
    const cv::Mat selected = bgr(cv::Rect(layout.source.x, layout.source.y,
                                          layout.source.width, layout.source.height));
    cv::Mat resized;
    cv::resize(selected, resized, { layout.destination.width, layout.destination.height }, 0, 0,
               interpolation(options.scaling.filter));
    cv::Mat enhanced;
    resized.convertTo(enhanced, -1, options.contrast, 0.0);
    if (options.sharpening > 0.0f) {
        cv::Mat blurred;
        cv::GaussianBlur(enhanced, blurred, {}, 1.0);
        cv::addWeighted(enhanced, 1.0 + options.sharpening, blurred,
                        -options.sharpening, 0.0, enhanced);
    }
    cv::Mat canvas(bounds.height, bounds.width, CV_8UC3,
                   cv::Scalar(options.scaling.background.blue,
                              options.scaling.background.green,
                              options.scaling.background.red));
    const int destinationX = std::max(0, layout.destination.x);
    const int destinationY = std::max(0, layout.destination.y);
    const int sourceX = std::max(0, -layout.destination.x);
    const int sourceY = std::max(0, -layout.destination.y);
    const int copyWidth = std::min(enhanced.cols - sourceX, canvas.cols - destinationX);
    const int copyHeight = std::min(enhanced.rows - sourceY, canvas.rows - destinationY);
    if (copyWidth > 0 && copyHeight > 0) {
        enhanced(cv::Rect(sourceX, sourceY, copyWidth, copyHeight)).copyTo(
            canvas(cv::Rect(destinationX, destinationY, copyWidth, copyHeight)));
    }

    engine.clear();
    engine.renderFrame(canvas.ptr<std::uint8_t>(), canvas.cols, canvas.rows,
                       static_cast<std::ptrdiff_t>(canvas.step), PixelFormat::BGR24);
    return true;
}

}