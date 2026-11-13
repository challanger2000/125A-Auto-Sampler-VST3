#pragma once
#include "SamplingPlan.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace a125 {
struct CaptureLocation {
    std::size_t takeIndex = 0;
    std::uint64_t takeFrame = 0;
    bool lastFrame = false;
};

// Each take records from Note-On through the complete configured release
// window. Gaps between takes are NOT recorded. No allocation, no locks.
inline std::optional<CaptureLocation> locateCaptureFrame(
    std::span<const Take> plan, std::uint64_t relativeFrame) noexcept {
    const auto it = std::upper_bound(
        plan.begin(), plan.end(), relativeFrame,
        [](std::uint64_t at, const Take& take) { return at < take.noteOnFrame; });
    if (it == plan.begin()) return std::nullopt;
    const auto& take = *std::prev(it);
    if (relativeFrame >= take.captureEndFrame) return std::nullopt;
    return CaptureLocation{static_cast<std::size_t>(std::distance(plan.begin(), std::prev(it))),
        relativeFrame - take.noteOnFrame, relativeFrame + 1 == take.captureEndFrame};
}
} // namespace a125
