#pragma once
#include <cstdint>

namespace a125 {

// Capture completeness checker, independent of the host and filesystem.
// One producer (audio) and one consumer (file worker); used on consumer only.
// A WAV may be written only after every exact sample offset was observed.
struct CaptureFrameInfo {
    std::uint32_t generation = 0;
    std::uint32_t take = 0;
    std::uint64_t offset = 0;
    std::uint64_t expectedFrames = 0;
    bool last = false;
};

struct CaptureDecision {
    bool started = false;          // This frame belongs to a new take
    bool abandonedPrevious = false; // Prior take never completed
    bool acceptFrame = false;      // Current sample is contiguous and valid
    bool completed = false;        // Entire expected capture received
    bool rejected = false;         // Final/invalid frame, no WAV is permitted
};

class TakeIntegrity {
public:
    CaptureDecision consume(const CaptureFrameInfo& frame) noexcept {
        CaptureDecision decision;
        if (!active_ || frame.generation != generation_ || frame.take != take_) {
            decision.abandonedPrevious = active_;
            active_ = true;
            invalid_ = false;
            generation_ = frame.generation;
            take_ = frame.take;
            nextOffset_ = 0;
            expectedFrames_ = frame.expectedFrames;
            decision.started = true;
        }
        const bool valid = frame.expectedFrames != 0 &&
            frame.expectedFrames == expectedFrames_ &&
            frame.offset == nextOffset_ &&
            frame.offset < expectedFrames_ &&
            frame.last == (frame.offset == expectedFrames_ - 1);
        if (!valid) invalid_ = true;
        decision.acceptFrame = !invalid_;
        if (frame.offset != UINT64_MAX) nextOffset_ = frame.offset + 1;
        else invalid_ = true;
        // An incorrectly early last marker is always rejected.
        // An absent final marker leaves active_ true; a new take or shutdown
        // reports an abandoned capture rather than exporting a partial WAV.
        if (frame.last || (expectedFrames_ != 0 && nextOffset_ >= expectedFrames_)) {
            decision.completed = !invalid_ && frame.last &&
                nextOffset_ == expectedFrames_;
            decision.rejected = !decision.completed;
            active_ = false;
        }
        return decision;
    }
    bool abandon() noexcept {
        const bool wasActive = active_;
        active_ = false;
        invalid_ = false;
        nextOffset_ = 0;
        return wasActive;
    }
private:
    std::uint32_t generation_ = 0, take_ = 0;
    std::uint64_t nextOffset_ = 0, expectedFrames_ = 0;
    bool active_ = false, invalid_ = false;
};
} // namespace a125
