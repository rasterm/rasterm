/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <rasterm/Scaling.hpp>

#include <string>

namespace rasterm::rPlayer {

bool renderImageFile(const std::string& path, const ImageOptions& options = {});

}