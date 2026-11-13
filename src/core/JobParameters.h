#pragma once
#include "JobSchedule.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace a125 {
// Stable processor parameter order. The Run switch intentionally is not saved.
enum Parameter : std::size_t {
    Run=0,FirstNote,LastNote,NoteStep,RoundRobins,VelocityLayers,
    Hold,Release,Gap,Velocity1,Velocity2,Velocity3,ParameterCount
};
using ParameterValues=std::array<double,ParameterCount>;
inline constexpr ParameterValues defaultParameters{
    0.,60./127.,60./127.,0.,7./15.,0.,
    (1.-.25)/7.75,(2.-.25)/9.75,(.1-.05)/.45,
    100./127.,60./127.,120./127.
};
inline int midiValue(double value) noexcept {
    return static_cast<int>(std::lround(std::clamp(value,0.,1.)*126.))+1;
}
inline int stepped(double value,int minimum,int maximum) noexcept {
    return minimum+static_cast<int>(std::lround(
        std::clamp(value,0.,1.)*static_cast<double>(maximum-minimum)));
}
inline JobConfig makeJobFromParameters(
        const ParameterValues& p,double sampleRate) noexcept {
    const auto sr=std::clamp(sampleRate,8000.,384000.);
    JobConfig c;
    c.firstNote=stepped(p[FirstNote],0,127);
    c.lastNote=std::max(c.firstNote,stepped(p[LastNote],0,127));
    c.semitoneStep=stepped(p[NoteStep],1,12);
    c.roundRobins=stepped(p[RoundRobins],1,16);
    c.velocityLayers=stepped(p[VelocityLayers],1,3);
    c.velocities={midiValue(p[Velocity1]),midiValue(p[Velocity2]),midiValue(p[Velocity3])};
    // Sort only enabled layers: musical layer order is consistently low-to-high.
    std::sort(c.velocities.begin(),c.velocities.begin()+c.velocityLayers);
    c.holdFrames=static_cast<std::uint64_t>(std::llround(
        (.25+std::clamp(p[Hold],0.,1.)*7.75)*sr));
    c.tailFrames=static_cast<std::uint64_t>(std::llround(
        (.25+std::clamp(p[Release],0.,1.)*9.75)*sr));
    c.gapFrames=static_cast<std::uint64_t>(std::llround(
        (.05+std::clamp(p[Gap],0.,1.)*.45)*sr));
    return c;
}
} // namespace a125
