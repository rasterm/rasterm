/* SPDX-License-Identifier: Apache-2.0 */

#include <render/DamagePlanner.hpp>
#include <render/Renderer.hpp>

#include <rasterm/Damage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

int main()
{
    std::mt19937 random(0x52415354u);
    std::uniform_int_distribution<int> frameDimension(1, 4096);
    std::uniform_int_distribution<int> cellDimension(1, 64);

    for (int iteration = 0; iteration < 20'000; ++iteration) {
        const int width = frameDimension(random);
        const int height = frameDimension(random);
        const rasterm::CellPixelSize cell{ cellDimension(random), cellDimension(random) };
        std::uniform_int_distribution<int> xPosition(0, width - 1);
        std::uniform_int_distribution<int> yPosition(0, height - 1);
        const int x = xPosition(random);
        const int y = yPosition(random);
        std::uniform_int_distribution<int> damageWidth(1, width - x);
        std::uniform_int_distribution<int> damageHeight(1, height - y);
        const rasterm::DamageRegion source{ x, y, damageWidth(random), damageHeight(random) };
        const auto aligned = rasterm::alignDamageToCells(source, width, height, cell);
        if (!aligned.isValid() || aligned.x < 0 || aligned.y < 0 ||
            aligned.x > source.x || aligned.y > source.y ||
            static_cast<std::int64_t>(aligned.x) + aligned.width <
                static_cast<std::int64_t>(source.x) + source.width ||
            static_cast<std::int64_t>(aligned.y) + aligned.height <
                static_cast<std::int64_t>(source.y) + source.height ||
            aligned.x + aligned.width > width || aligned.y + aligned.height > height ||
            aligned.x % cell.width != 0 || aligned.y % cell.height != 0 ||
            (aligned.x + aligned.width != width && aligned.width % cell.width != 0) ||
            (aligned.y + aligned.height != height && aligned.height % cell.height != 0)) {
            return 1;
        }

        std::vector<rasterm::DamageRegion> regions;
        const int count = 1 + static_cast<int>(random() % 8);
        regions.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            const int regionX = xPosition(random);
            const int regionY = yPosition(random);
            regions.push_back(rasterm::alignDamageToCells({
                regionX, regionY,
                1 + static_cast<int>(random() % static_cast<unsigned>(width - regionX)),
                1 + static_cast<int>(random() % static_cast<unsigned>(height - regionY)),
            }, width, height, cell));
        }
        const auto plan = rasterm::planDamagePresentation(
            regions, width, height, cell, (random() & 1u) != 0,
            static_cast<int>(random() % 257));
        if (regions.empty() || plan.fullFrame.total() == 0 || plan.regional.total() == 0 ||
            (plan.useFullFrame && plan.fullFrame.total() > plan.regional.total())) {
            return 2;
        }
        if (!std::is_sorted(regions.begin(), regions.end(), [](const auto left,
                                                                const auto right) {
                return left.y != right.y ? left.y < right.y : left.x < right.x;
            })) {
            return 3;
        }
        for (const auto region : regions) {
            if (!region.isValid() || region.x < 0 || region.y < 0 ||
                region.x + region.width > width || region.y + region.height > height) {
                return 4;
            }
        }
    }

    constexpr int maximum = (std::numeric_limits<int>::max)();
    const auto clipped = rasterm::alignDamageToCells({ -10, -10, 20, 20 }, 100, 100,
                                                       { 7, 13 });
    if (clipped.x != 0 || clipped.y != 0 || clipped.width != 14 || clipped.height != 13) {
        return 5;
    }
    if (rasterm::alignDamageToCells({ maximum - 4, maximum - 4, maximum, maximum },
                                    100, 100, { 10, 20 }).isValid()) {
        return 6;
    }
    std::vector<rasterm::DamageRegion> overflowRegions{
        { maximum - 8, 0, 8, 1 }, { maximum - 4, 0, 8, 1 },
    };
    rasterm::normalizePresentationDamage(overflowRegions);
    if (overflowRegions.size() != 2 ||
        std::any_of(overflowRegions.begin(), overflowRegions.end(),
                    [](const auto region) { return !region.isValid(); })) {
        return 9;
    }
    const rasterm::DamageRect overflow{ maximum - 4, 0, 10, 1 };
    if (rasterm::DamageView{ &overflow, 1, true }.isValidFor(maximum, 1)) return 7;

    const std::array safe{ rasterm::DamageRegion{ 0, 0, 10, 99 } };
    const std::array bottom{ rasterm::DamageRegion{ 0, 80, 10, 20 } };
    if (!rasterm::canPresentWithoutBottomScroll(safe, 100) ||
        rasterm::canPresentWithoutBottomScroll(bottom, 100)) {
        return 8;
    }
    return 0;
}
