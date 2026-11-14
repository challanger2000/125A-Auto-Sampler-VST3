#pragma once
#include <cstddef>
#include <cstdint>

namespace a125 {

// Presentation-only status model. No allocation, locks, filesystem or
// host-specific API; safe to inspect from the native VSTGUI timer.
enum class RunDisplayStatus {
    Ready, Recording, Finalizing, Complete, Partial, Failed,
    Aborted, MissingRecorder
};

struct RunDisplaySnapshot {
    std::uint32_t generation=0;
    std::uint32_t captureEndedGeneration=0;
    std::uint32_t writerFinishedGeneration=0;
    std::uint32_t interruptedGeneration=0;
    std::uint32_t observed=0, written=0, rejected=0, silent=0, errors=0;
    std::uint64_t dropped=0;
    std::size_t expected=0;
    bool recordingAnchorActive=false;
};

inline RunDisplayStatus runDisplayStatus(const RunDisplaySnapshot& s) noexcept {
    if(s.generation==0) return RunDisplayStatus::Ready;
    if(s.writerFinishedGeneration==s.generation) {
        if(s.interruptedGeneration==s.generation)
            return RunDisplayStatus::Aborted;
        if(s.expected>0 && s.written==s.expected && !s.rejected &&
           !s.silent && !s.errors && !s.dropped)
            return RunDisplayStatus::Complete;
        return s.written>0?RunDisplayStatus::Partial:RunDisplayStatus::Failed;
    }
    if(s.interruptedGeneration==s.generation)
        return RunDisplayStatus::Aborted;
    if(s.captureEndedGeneration==s.generation) {
        // No completed capture means the Recorder was absent or not routed.
        // Never leave the GUI stuck at FINALIZING in this condition.
        if(s.observed==0) return RunDisplayStatus::MissingRecorder;
        return RunDisplayStatus::Finalizing;
    }
    if(s.recordingAnchorActive) return RunDisplayStatus::Recording;
    return RunDisplayStatus::Ready;
}

inline const char* runDisplayText(RunDisplayStatus status) noexcept {
    switch(status) {
    case RunDisplayStatus::Ready: return "READY / STOPPED";
    case RunDisplayStatus::Recording: return "RECORDING";
    case RunDisplayStatus::Finalizing: return "FINALIZING";
    case RunDisplayStatus::Complete: return "COMPLETE";
    case RunDisplayStatus::Partial: return "PARTIAL / FAILED";
    case RunDisplayStatus::Failed: return "FAILED";
    case RunDisplayStatus::Aborted: return "ABORTED";
    case RunDisplayStatus::MissingRecorder: return "NO AUDIO RECORDER";
    }
    return "UNKNOWN";
}
} // namespace a125
