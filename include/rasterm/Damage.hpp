/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstddef>

namespace rasterm {

struct DamageRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool isValid() const noexcept { return width > 0 && height > 0; }
};

struct DamageView {
    const DamageRect* rectangles = nullptr;
    std::size_t count = 0;
    bool supplied = false;

    [[nodiscard]] bool isValidFor(const int frameWidth, const int frameHeight) const noexcept
    {
        if (frameWidth <= 0 || frameHeight <= 0) {
            return false;
        }
        if (!supplied) {
            return rectangles == nullptr && count == 0;
        }
        if (count > 0 && rectangles == nullptr) {
            return false;
        }
        for (std::size_t index = 0; index < count; ++index) {
            const auto& rectangle = rectangles[index];
            if (!rectangle.isValid() || rectangle.x < 0 || rectangle.y < 0 ||
                rectangle.x > frameWidth || rectangle.y > frameHeight ||
                rectangle.width > frameWidth - rectangle.x ||
                rectangle.height > frameHeight - rectangle.y) {
                return false;
            }
        }
        return true;
    }
};

}