/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <opencv2/core/mat.hpp>

#include <string>

namespace rasterm::rPlayer {

[[nodiscard]] cv::Mat loadColorManagedBgr(const std::string& path);

}