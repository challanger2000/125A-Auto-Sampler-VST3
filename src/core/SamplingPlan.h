#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace a125 {

struct Settings {
    int firstNote = 60;
    int lastNote = 60;
    int semitoneStep = 1;
    std::vector<int> velocities {100};
    int roundRobins = 8;
    std::uint64_t holdFrames = 44100;
    std::uint64_t tailFrames = 44100;
    std::uint64_t gapFrames = 4410;
};

struct Take {
    int note = 0;
    int velocity = 0;
    int roundRobin = 0; // One-based, for filenames RR01...RR16
    std::uint64_t noteOnFrame = 0;
    std::uint64_t noteOffFrame = 0;
    std::uint64_t captureEndFrame = 0;
};

inline std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        throw std::overflow_error("sampling timeline overflow");
    return a + b;
}

// Built off the real-time thread. The returned plan is immutable during playback.
// Each take is independent; no RR take may replace another on export.
inline std::vector<Take> makePlan(const Settings& s) {
    if (s.firstNote < 0 || s.firstNote > 127 || s.lastNote < s.firstNote ||
        s.lastNote > 127 || s.semitoneStep < 1 || s.semitoneStep > 12 ||
        s.roundRobins < 1 || s.roundRobins > 16 || s.velocities.empty() ||
        s.velocities.size() > 16 || s.holdFrames == 0 || s.tailFrames == 0)
        throw std::invalid_argument("invalid auto-sampling settings");
    for (int velocity : s.velocities)
        if (velocity < 1 || velocity > 127)
            throw std::invalid_argument("invalid MIDI velocity");

    std::vector<Take> result;
    std::uint64_t cursor = 0;
    for (int note = s.firstNote; note <= s.lastNote; note += s.semitoneStep) {
        for (int velocity : s.velocities) {
            for (int rr = 1; rr <= s.roundRobins; ++rr) {
                const auto off = checkedAdd(cursor, s.holdFrames);
                const auto end = checkedAdd(off, s.tailFrames);
                result.push_back({note, velocity, rr, cursor, off, end});
                cursor = checkedAdd(end, s.gapFrames);
            }
        }
    }
    return result;
}

enum class MidiKind : std::uint8_t { NoteOn, NoteOff };
struct MidiEvent {
    MidiKind kind;
    int note;
    int velocity;
    std::uint32_t sampleOffset;
    std::size_t takeIndex;
};

// Once started, callback execution allocates no memory.
// The caller supplies a no-throw sink, e.g. VST3 IEventList::addEvent.
// Discontinuous DAW transport requires explicit reset() and All-Notes-Off
// before rendering another timeline.
class MidiScheduler {
public:
    explicit MidiScheduler(std::vector<Take> plan) : plan_(std::move(plan)) {}

    void reset() noexcept { nextTake_ = 0; isNoteOn_ = true; }
    bool done() const noexcept { return nextTake_ == plan_.size(); }
    const std::vector<Take>& plan() const noexcept { return plan_; }

    template <class Sink>
    void render(std::uint64_t firstFrame, std::uint32_t frames, Sink&& sink) noexcept {
        if (frames == 0) return;
        const std::uint64_t lastFrame = firstFrame + frames;
        while (nextTake_ < plan_.size()) {
            const auto& take = plan_[nextTake_];
            const auto at = isNoteOn_ ? take.noteOnFrame : take.noteOffFrame;
            if (at >= lastFrame) return;
            if (at >= firstFrame) {
                const auto kind = isNoteOn_ ? MidiKind::NoteOn : MidiKind::NoteOff;
                sink(MidiEvent{kind, take.note, take.velocity,
                               static_cast<std::uint32_t>(at - firstFrame), nextTake_});
            }
            if (isNoteOn_) {
                isNoteOn_ = false;
            } else {
                isNoteOn_ = true;
                ++nextTake_;
            }
        }
    }

private:
    std::vector<Take> plan_;
    std::size_t nextTake_ = 0;
    bool isNoteOn_ = true;
};

} // namespace a125
