#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace a125 {

// Immutable for the duration of one job; constructed without memory allocation.
struct JobConfig {
    int firstNote=60;
    int lastNote=60;
    int semitoneStep=1;
    int roundRobins=8;
    int velocityLayers=1;
    std::array<int,3> velocities{100,80,120};
    std::uint64_t holdFrames=44100;
    std::uint64_t tailFrames=88200;
    std::uint64_t gapFrames=4410;

    bool valid() const noexcept {
        if (firstNote<0 || firstNote>127 || lastNote<firstNote || lastNote>127 ||
            semitoneStep<1 || semitoneStep>12 || roundRobins<1 || roundRobins>16 ||
            velocityLayers<1 || velocityLayers>3 || holdFrames==0 || tailFrames==0 ||
            holdFrames>100000000 || tailFrames>100000000 || gapFrames>100000000)
            return false;
        for(int i=0;i<velocityLayers;++i)
            if(velocities[static_cast<std::size_t>(i)]<1 ||
               velocities[static_cast<std::size_t>(i)]>127) return false;
        return true;
    }
    std::size_t noteCount() const noexcept {
        return 1+static_cast<std::size_t>((lastNote-firstNote)/semitoneStep);
    }
    std::size_t takeCount() const noexcept {
        return valid()?noteCount()*static_cast<std::size_t>(velocityLayers)*
            static_cast<std::size_t>(roundRobins):0;
    }
    std::uint64_t period() const noexcept { return holdFrames+tailFrames+gapFrames; }
};

struct JobTake {
    int note=0;
    int velocity=0;
    int roundRobin=0;
    std::size_t takeIndex=0;
    std::uint64_t start=0, noteOff=0, captureEnd=0;
};
inline JobTake jobTake(const JobConfig& c, std::size_t index) noexcept {
    const auto idx=index;
    const auto rr=1+static_cast<int>(index%static_cast<std::size_t>(c.roundRobins));
    index/=static_cast<std::size_t>(c.roundRobins);
    const auto layer=static_cast<std::size_t>(index%static_cast<std::size_t>(c.velocityLayers));
    index/=static_cast<std::size_t>(c.velocityLayers);
    const int note=c.firstNote+static_cast<int>(index)*c.semitoneStep;
    const auto start=static_cast<std::uint64_t>(idx)*c.period();
    return {note,c.velocities[layer],rr,idx,start,start+c.holdFrames,
            start+c.holdFrames+c.tailFrames};
}

struct JobCapture {
    JobTake take;
    std::uint64_t offset=0;
    bool last=false;
};
// O(1), heap-free per captured audio frame. The configured gap is never recorded.
inline std::optional<JobCapture> locateJobFrame(
        const JobConfig& c,std::uint64_t relative) noexcept {
    if(!c.valid()) return std::nullopt;
    const auto idx=relative/c.period();
    if(idx>=c.takeCount()) return std::nullopt;
    const auto offset=relative%c.period();
    if(offset>=c.holdFrames+c.tailFrames) return std::nullopt;
    return JobCapture{jobTake(c,static_cast<std::size_t>(idx)),offset,
                      offset+1==c.holdFrames+c.tailFrames};
}
} // namespace a125
