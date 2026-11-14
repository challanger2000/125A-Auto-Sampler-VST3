#pragma once
#include "JobSchedule.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace a125 {

// A portable SFZ v1 keyboard map. All paths are relative to the adjacent WAVs.
// This is offline-only code, called exclusively from the recorder's file worker.
inline std::string sampleFileName(const JobTake& take) {
    return "N"+std::to_string(take.note)+"_V"+
        std::to_string(take.velocity)+"_RR"+
        (take.roundRobin<10?"0":"")+std::to_string(take.roundRobin)+".wav";
}

// SFZ group velocity zones cover 1..127 without holes or overlap.
// Equal velocity layers cannot be mapped unambiguously and are rejected.
inline std::optional<std::string> sfzBankText(const JobConfig& config) {
    if(!config.valid() || config.takeCount()==0) return std::nullopt;
    std::vector<int> velocities(
        config.velocities.begin(),
        config.velocities.begin()+config.velocityLayers);
    std::sort(velocities.begin(),velocities.end());
    if(std::adjacent_find(velocities.begin(),velocities.end())!=velocities.end())
        return std::nullopt;
    std::vector<int> notes;
    for(int note=config.firstNote;note<=config.lastNote;note+=config.semitoneStep)
        notes.push_back(note);
    if(notes.empty()) return std::nullopt;

    std::string sfz="// 125A AutoSampler - portable SFZ v1\n";
    sfz+="// Generated from actual captured WAVs. Keep this file with its WAVs.\n";
    sfz+="// One-shot playback: sustain looping has not been analyzed.\n";
    for(std::size_t ni=0;ni<notes.size();++ni) {
        const int note=notes[ni];
        const int lowKey=ni==0?config.firstNote:
            (notes[ni-1]+note)/2+1;
        const int highKey=ni+1==notes.size()?config.lastNote:
            (note+notes[ni+1])/2;
        for(std::size_t vi=0;vi<velocities.size();++vi) {
            const int velocity=velocities[vi];
            const int lowVel=vi==0?1:(velocities[vi-1]+velocity)/2+1;
            const int highVel=vi+1==velocities.size()?127:
                (velocity+velocities[vi+1])/2;
            sfz+="\n<group> lokey="+std::to_string(lowKey)+
                " hikey="+std::to_string(highKey)+
                " pitch_keycenter="+std::to_string(note)+
                " lovel="+std::to_string(lowVel)+
                " hivel="+std::to_string(highVel)+
                " seq_length="+std::to_string(config.roundRobins)+
                " loop_mode=one_shot\n";
            for(int rr=1;rr<=config.roundRobins;++rr) {
                const JobTake take{note,velocity,rr};
                sfz+="<region> seq_position="+std::to_string(rr)+
                    " sample="+sampleFileName(take)+"\n";
            }
        }
    }
    return sfz;
}

// Fail closed: do not advertise a playable bank if even one recorded sample
// is absent/empty or if the SFZ cannot be written and renamed successfully.
// A temporary file prevents partial .sfz files after interrupted disk writes.
inline bool writeCompleteSfzBank(const JobConfig& config,
                                const std::filesystem::path& directory) {
    const auto content=sfzBankText(config);
    if(!content) return false;
    std::error_code ec;
    for(std::size_t i=0;i<config.takeCount();++i) {
        const auto file=directory/sampleFileName(jobTake(config,i));
        if(!std::filesystem::is_regular_file(file,ec) || ec) return false;
        const auto size=std::filesystem::file_size(file,ec);
        if(ec || size<=44) return false;
    }
    const auto temporary=directory/"125A AutoSampler.sfz.tmp";
    const auto destination=directory/"125A AutoSampler.sfz";
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out) return false;
        out.write(content->data(),static_cast<std::streamsize>(content->size()));
        out.flush();
        if(!out) {
            out.close();
            std::filesystem::remove(temporary,ec);
            return false;
        }
    }
    std::filesystem::rename(temporary,destination,ec);
    if(ec) {
        std::filesystem::remove(temporary,ec);
        return false;
    }
    return true;
}
} // namespace a125
