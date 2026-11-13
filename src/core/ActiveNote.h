#pragma once
#include <cstdint>
#include <optional>

namespace a125 {

// Tracks the actual accepted Note-On from VST3 IEventList.
// At transport stop, a DAW loop/jump or Run=Off, the generator must send a
// matching Note-Off even if the playhead has already moved elsewhere.
struct PendingNote { int pitch=0; int noteId=0; };
class ActiveNote {
public:
    void acceptedOn(int pitch,int noteId) noexcept {
        current_=PendingNote{pitch,noteId};
    }
    void acceptedOff(int noteId) noexcept {
        if(current_ && current_->noteId==noteId) current_.reset();
    }
    std::optional<PendingNote> pending() const noexcept {return current_;}
    void reset() noexcept {current_.reset();}
private:
    std::optional<PendingNote> current_;
};
} // namespace a125
