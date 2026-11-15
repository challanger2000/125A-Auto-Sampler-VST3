#pragma once
// P028 – A musical GUI note convention matching the built-in Steinberg
// keyboard: MIDI 60 = C3, 36 = C1, 84 = C5. No MIDI/PCM mapping is changed.
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace a125 {
inline std::string midiNoteName(int number) {
    if(number<0 || number>127)return {};
    constexpr std::array<std::string_view,12> names{
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return std::string(names[static_cast<std::size_t>(number%12)])+
           std::to_string(number/12-2);
}

inline std::optional<int> parseMidiNoteName(std::string_view value) noexcept {
    const auto blank=[](char ch) {
        return ch==' ' || ch=='\t' || ch=='\r' || ch=='\n';
    };
    while(!value.empty() && blank(value.front()))value.remove_prefix(1);
    while(!value.empty() && blank(value.back()))value.remove_suffix(1);
    if(value.empty())return std::nullopt;
    int midi=0;
    if(value.front()>='0' && value.front()<='9') {
        const auto* begin=value.data();
        const auto* end=begin+value.size();
        auto [ptr,error]=std::from_chars(begin,end,midi);
        if(error==std::errc{} && ptr==end && midi<=127)
            return midi;
        return std::nullopt;
    }
    char note=value.front();
    if(note>='a' && note<='g')note=static_cast<char>(note-'a'+'A');
    int semitone=0;
    switch(note) {
    case 'C':semitone=0;break;
    case 'D':semitone=2;break;
    case 'E':semitone=4;break;
    case 'F':semitone=5;break;
    case 'G':semitone=7;break;
    case 'A':semitone=9;break;
    case 'B':semitone=11;break;
    default:return std::nullopt;
    }
    value.remove_prefix(1);
    if(!value.empty() && (value.front()=='#' ||
                           value.front()=='b' || value.front()=='B')) {
        semitone+=value.front()=='#'?1:-1;
        value.remove_prefix(1);
    }
    if(value.empty())return std::nullopt;
    int octave=0;
    auto [ptr,error]=std::from_chars(value.data(),
                                    value.data()+value.size(),octave);
    if(error!=std::errc{} || ptr!=value.data()+value.size())
        return std::nullopt;
    if(octave< -2 || octave>8)return std::nullopt;
    midi=(octave+2)*12+semitone;
    if(midi<0 || midi>127)return std::nullopt;
    return midi;
}
} // namespace a125
