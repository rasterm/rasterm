/* SPDX-License-Identifier: Apache-2.0 */

#include <render/DamagePlanner.hpp>
#include <render/Renderer.hpp>

#include <algorithm>
#include <limits>

namespace rasterm {
namespace {

std::uint64_t saturatedAdd(const std::uint64_t left, const std::uint64_t right) noexcept
{
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    return left > maximum - right ? maximum : left + right;
}

std::uint64_t area(const DamageRegion region) noexcept
{
    return static_cast<std::uint64_t>(region.width) * static_cast<std::uint64_t>(region.height);
}

bool mergeWithoutInflation(DamageRegion& left, const DamageRegion right) noexcept
{
    const auto leftRight = static_cast<std::int64_t>(left.x) + left.width;
    const auto rightRight = static_cast<std::int64_t>(right.x) + right.width;
    const auto leftBottom = static_cast<std::int64_t>(left.y) + left.height;
    const auto rightBottom = static_cast<std::int64_t>(right.y) + right.height;
    const auto x1 = static_cast<std::int64_t>(std::min(left.x, right.x));
    const auto y1 = static_cast<std::int64_t>(std::min(left.y, right.y));
    const auto x2 = std::max(leftRight, rightRight);
    const auto y2 = std::max(leftBottom, rightBottom);
    const auto overlapWidth = std::max<std::int64_t>(
        0, std::min(leftRight, rightRight) - std::max(left.x, right.x));
    const auto overlapHeight = std::max<std::int64_t>(
        0, std::min(leftBottom, rightBottom) - std::max(left.y, right.y));
    const std::uint64_t unionArea = area(left) + area(right) -
        static_cast<std::uint64_t>(overlapWidth) * overlapHeight;
    if (x1 < (std::numeric_limits<int>::min)() || y1 < (std::numeric_limits<int>::min)() ||
        x2 > (std::numeric_limits<int>::max)() || y2 > (std::numeric_limits<int>::max)() ||
        x2 - x1 > (std::numeric_limits<int>::max)() ||
        y2 - y1 > (std::numeric_limits<int>::max)()) {
        return false;
    }
    const DamageRegion bounds{ static_cast<int>(x1), static_cast<int>(y1),
                               static_cast<int>(x2 - x1), static_cast<int>(y2 - y1) };
    if (area(bounds) != unionArea) return false;
    left = bounds;
    return true;
}

DamageRegion boundsOf(const std::span<const DamageRegion> regions) noexcept
{
    DamageRegion bounds = regions.front();
    for (const DamageRegion region : regions.subspan(1)) {
        const int right = std::max(bounds.x + bounds.width, region.x + region.width);
        const int bottom = std::max(bounds.y + bounds.height, region.y + region.height);
        bounds.x = std::min(bounds.x, region.x);
        bounds.y = std::min(bounds.y, region.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
    }
    return bounds;
}

std::uint64_t estimatedSixelBytes(const DamageRegion region) noexcept
{
    const std::uint64_t pixels = area(region);
    const std::uint64_t bands = (static_cast<std::uint64_t>(region.height) + 5) / 6;
    const std::uint64_t bandOverhead = bands *
        (static_cast<std::uint64_t>(region.width) / 16 + 8);
    return saturatedAdd(pixels / 3, bandOverhead);
}

PresentationCost estimate(const std::span<const DamageRegion> regions,
                          const bool synchronizedOutput, const int estimatedColors,
                          const bool positioned, const CellPixelSize cellSize,
                          const bool independentRegionPalettes) noexcept
{
    PresentationCost cost;
    const auto digits = [](std::uint64_t value) {
        std::uint64_t result = 1;
        while (value >= 10) {
            value /= 10;
            ++result;
        }
        return result;
    };
    for (const DamageRegion region : regions) {
        cost.sixelBytes = saturatedAdd(cost.sixelBytes, estimatedSixelBytes(region));
        cost.sixelBytes = saturatedAdd(cost.sixelBytes, 40);
        if (positioned) {
            const auto row = static_cast<std::uint64_t>(region.y / cellSize.height + 1);
            const auto column = static_cast<std::uint64_t>(region.x / cellSize.width + 1);
            cost.cursorBytes = saturatedAdd(cost.cursorBytes, 4 + digits(row) + digits(column));
        }
    }
    const std::uint64_t paletteCount = positioned && independentRegionPalettes
        ? regions.size() : 1;
    cost.paletteBytes = static_cast<std::uint64_t>(std::max(0, estimatedColors)) * 14 *
        paletteCount;
    cost.synchronizationBytes = synchronizedOutput ? 16 : 0;
    return cost;
}

}

DamageRegion alignDamageToCells(const DamageRegion region, const int frameWidth,
                                const int frameHeight, const CellPixelSize cellSize) noexcept
{
    if (!region.isValid() || frameWidth <= 0 || frameHeight <= 0 || cellSize.width <= 0 ||
        cellSize.height <= 0) {
        return {};
    }
    const auto sourceRight = static_cast<std::int64_t>(region.x) + region.width;
    const auto sourceBottom = static_cast<std::int64_t>(region.y) + region.height;
    const int clippedX = std::clamp(region.x, 0, frameWidth);
    const int clippedY = std::clamp(region.y, 0, frameHeight);
    const auto right = std::clamp<std::int64_t>(sourceRight, 0, frameWidth);
    const auto bottom = std::clamp<std::int64_t>(sourceBottom, 0, frameHeight);
    if (right <= clippedX || bottom <= clippedY) return {};

    const int x = (clippedX / cellSize.width) * cellSize.width;
    const int y = (clippedY / cellSize.height) * cellSize.height;
    const auto alignedRight = ((right + cellSize.width - 1) / cellSize.width) * cellSize.width;
    const auto alignedBottom = ((bottom + cellSize.height - 1) / cellSize.height) * cellSize.height;
    const int clippedRight = static_cast<int>(std::min<std::int64_t>(frameWidth, alignedRight));
    const int clippedBottom = static_cast<int>(std::min<std::int64_t>(frameHeight, alignedBottom));
    return { x, y, clippedRight - x, clippedBottom - y };
}

void normalizePresentationDamage(std::vector<DamageRegion>& regions)
{
    regions.erase(std::remove_if(regions.begin(), regions.end(), [](const DamageRegion region) {
        return !region.isValid();
    }), regions.end());
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t first = 0; first < regions.size() && !merged; ++first) {
            for (std::size_t second = first + 1; second < regions.size(); ++second) {
                if (mergeWithoutInflation(regions[first], regions[second])) {
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(second));
                    merged = true;
                    break;
                }
            }
        }
    }
    std::sort(regions.begin(), regions.end(), [](const DamageRegion left,
                                                  const DamageRegion right) {
        return left.y != right.y ? left.y < right.y : left.x < right.x;
    });
}

bool canPresentWithoutBottomScroll(const std::span<const DamageRegion> regions,
                                   const int frameHeight) noexcept
{
    return std::none_of(regions.begin(), regions.end(), [frameHeight](const DamageRegion region) {
        return static_cast<std::int64_t>(region.y) + region.height == frameHeight;
    });
}

DamagePlan planDamagePresentation(std::vector<DamageRegion>& regions, const int frameWidth,
                                  const int frameHeight, const CellPixelSize cellSize,
                                  const bool synchronizedOutput, const int estimatedColors,
                                  const bool independentRegionPalettes)
{
    DamagePlan plan;
    if (regions.empty() || frameWidth <= 0 || frameHeight <= 0) return plan;
    normalizePresentationDamage(regions);
    if (regions.empty()) return plan;

    const DamageRegion full{ 0, 0, frameWidth, frameHeight };
    plan.fullFrame = estimate(std::span(&full, 1), synchronizedOutput, estimatedColors, false,
                              cellSize, false);
    plan.regional = estimate(regions, synchronizedOutput, estimatedColors, true, cellSize,
                             independentRegionPalettes);

    if (regions.size() > 1) {
        const DamageRegion bounds = boundsOf(regions);
        const PresentationCost merged = estimate(std::span(&bounds, 1), synchronizedOutput,
                                                 estimatedColors, true, cellSize,
                                                 independentRegionPalettes);
        if (merged.total() <= plan.regional.total()) {
            regions.assign(1, bounds);
            plan.regional = merged;
            plan.mergedRegions = true;
        }
    }
    plan.useFullFrame = plan.fullFrame.total() <= plan.regional.total();
    return plan;
}

}
