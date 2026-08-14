/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <damage/DamageRegion.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace rasterm {

struct CellPixelSize;

struct PresentationCost {
    std::uint64_t sixelBytes = 0;
    std::uint64_t cursorBytes = 0;
    std::uint64_t paletteBytes = 0;
    std::uint64_t synchronizationBytes = 0;

    [[nodiscard]] std::uint64_t total() const noexcept
    {
        const auto add = [](const std::uint64_t left, const std::uint64_t right) {
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            return left > maximum - right ? maximum : left + right;
        };
        return add(add(sixelBytes, cursorBytes), add(paletteBytes, synchronizationBytes));
    }
};

struct DamagePlan {
    PresentationCost fullFrame{};
    PresentationCost regional{};
    bool useFullFrame = false;
    bool mergedRegions = false;
};

[[nodiscard]] DamageRegion alignDamageToCells(DamageRegion region, int frameWidth,
                                               int frameHeight,
                                               CellPixelSize cellSize) noexcept;
void normalizePresentationDamage(std::vector<DamageRegion>& regions);
[[nodiscard]] bool canPresentWithoutBottomScroll(std::span<const DamageRegion> regions,
                                                 int frameHeight) noexcept;
[[nodiscard]] DamagePlan planDamagePresentation(std::vector<DamageRegion>& regions,
                                                int frameWidth, int frameHeight,
                                                CellPixelSize cellSize,
                                                bool synchronizedOutput,
                                                int estimatedColors = 64,
                                                bool independentRegionPalettes = false);

}
