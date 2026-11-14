#pragma once
namespace a125 {

// The user must explicitly switch Run OFF and back ON for every new job.
// Transport stop/loop, missing MIDI output bus, or plugin deactivation
// disarm the job. Merely pressing DAW Play again never starts new exports.
class RunGate {
public:
    bool observe(bool requested, bool ready) noexcept {
        if(!requested) {
            armed_=true;
            return false;
        }
        if(!ready) {
            armed_=false;
            return false;
        }
        if(!armed_) return false;
        armed_=false;
        return true;
    }
    void disarm() noexcept { armed_=false; }
private:
    bool armed_=true;
};
} // namespace a125
