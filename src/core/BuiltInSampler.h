#pragma once
#include "SampleProcessing.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <charconv>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace a125 {

// 125A BUILT-IN SAMPLER. No SFZ interpreter, no third-party player, no host.
// All WAV reading and bank publication are done outside the audio callback.
struct SampleZone {
    int root=60, velocity=100, rr=1;
    std::uint32_t sampleRate=44100;
    std::vector<StereoFrame> pcm;
};
struct SampleBank {
    std::vector<SampleZone> zones;
};

inline std::uint16_t samplerLE16(const unsigned char* b) noexcept {
    return static_cast<std::uint16_t>(b[0] | (static_cast<unsigned>(b[1])<<8));
}
inline std::uint32_t samplerLE32(const unsigned char* b) noexcept {
    return static_cast<std::uint32_t>(b[0]) |
        (static_cast<std::uint32_t>(b[1])<<8) |
        (static_cast<std::uint32_t>(b[2])<<16) |
        (static_cast<std::uint32_t>(b[3])<<24);
}
inline bool parseSampleName(const std::string& s,int& note,int& vel,int& rr) noexcept {
    std::string_view remainder(s);
    const auto literal=[&](std::string_view prefix) {
        if(!remainder.starts_with(prefix))return false;
        remainder.remove_prefix(prefix.size());
        return true;
    };
    const auto number=[&](int& value) {
        const auto* begin=remainder.data();
        const auto [end,err]=std::from_chars(begin,begin+remainder.size(),value);
        if(err!=std::errc{} || end==begin)return false;
        remainder.remove_prefix(static_cast<std::size_t>(end-begin));
        return true;
    };
    return literal("N") && number(note) && literal("_V") && number(vel) &&
           literal("_RR") && number(rr) && literal(".wav") && remainder.empty() &&
           note>=0 && note<=127 && vel>=1 && vel<=127 && rr>=1 && rr<=16;
}

// Read the exact standard 44-byte PCM24 stereo files emitted by our recorder.
// Reject truncated/foreign files instead of playing uninitialized samples.
inline bool readCapturedWav(const std::filesystem::path& path,SampleZone& zone) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) return false;
    const auto length=in.tellg();
    if(length<44 || length>std::streamoff(128*1024*1024)) return false;
    in.seekg(0);
    std::array<unsigned char,44> header{};
    in.read(reinterpret_cast<char*>(header.data()),44);
    if(!in || std::string(reinterpret_cast<char*>(header.data()),4)!="RIFF" ||
       std::string(reinterpret_cast<char*>(header.data()+8),4)!="WAVE" ||
       std::string(reinterpret_cast<char*>(header.data()+12),4)!="fmt " ||
       samplerLE32(header.data()+16)!=16 || samplerLE16(header.data()+20)!=1 ||
       samplerLE16(header.data()+22)!=2 || samplerLE16(header.data()+34)!=24 ||
       samplerLE16(header.data()+32)!=6 ||
       std::string(reinterpret_cast<char*>(header.data()+36),4)!="data")
        return false;
    const auto rate=samplerLE32(header.data()+24);
    const auto size=samplerLE32(header.data()+40);
    if(rate<8000 || rate>384000 || size==0 || size%6!=0 ||
       static_cast<std::uint64_t>(size)+44!=static_cast<std::uint64_t>(length) ||
       samplerLE32(header.data()+4)!=size+36 ||
       samplerLE32(header.data()+28)!=rate*6u) return false;
    zone.sampleRate=rate;
    zone.pcm.resize(size/6);
    for(auto& frame:zone.pcm) {
        unsigned char bytes[6]{};
        in.read(reinterpret_cast<char*>(bytes),6);
        if(!in) return false;
        for(int ch=0;ch<2;++ch) {
            const auto b=bytes+ch*3;
            auto raw=static_cast<std::int32_t>(
                static_cast<std::uint32_t>(b[0]) |
                (static_cast<std::uint32_t>(b[1])<<8) |
                (static_cast<std::uint32_t>(b[2])<<16));
            if(raw & 0x800000) raw|=static_cast<std::int32_t>(0xFF000000);
            frame[static_cast<std::size_t>(ch)]=static_cast<float>(raw/8388608.);
        }
    }
    return true;
}

// Loading is restricted to the GUI/control thread, never process().
// Complete captured note/velocity groups must have RR 1..N without gaps.
inline std::unique_ptr<SampleBank> loadCapturedBank(
        const std::filesystem::path& directory) {
    std::error_code ec;
    if(!std::filesystem::is_directory(directory,ec) || ec) return {};
    auto bank=std::make_unique<SampleBank>();
    std::map<std::pair<int,int>,std::set<int>> groups;
    std::uint32_t sampleRate=0;
    for(const auto& file:std::filesystem::directory_iterator(directory,ec)) {
        if(ec) return {};
        if(!file.is_regular_file())continue;
        const auto name=file.path().filename().string();
        if(file.path().extension()!=".wav")continue;
        int note=0,vel=0,rr=0;
        if(!parseSampleName(name,note,vel,rr)) return {};
        if(bank->zones.size()>=6144 || !groups[{note,vel}].insert(rr).second)
            return {};
        SampleZone z;
        z.root=note;z.velocity=vel;z.rr=rr;
        if(!readCapturedWav(file.path(),z) ||
           (sampleRate && sampleRate!=z.sampleRate)) return {};
        sampleRate=z.sampleRate;
        bank->zones.emplace_back(std::move(z));
    }
    if(ec || bank->zones.empty())return {};
    for(const auto& [key,rounds]:groups) {
        int count=0;
        for(int rr:rounds) if(rr!=++count) return {};
    }
    std::sort(bank->zones.begin(),bank->zones.end(),[](const auto& a,const auto& b){
        if(a.root!=b.root)return a.root<b.root;
        if(a.velocity!=b.velocity)return a.velocity<b.velocity;
        return a.rr<b.rr;
    });
    return bank;
}

class BuiltInSampler {
public:
    static constexpr std::size_t maxVoices=32;
    // Loading a replacement does not deallocate a bank while an ASIO
    // callback might still be playing it. The old bank lives until teardown.
    bool publish(std::unique_ptr<SampleBank> bank) {
        if(!bank || bank->zones.empty() || owned_.size()>=16) return false;
        const auto* ptr=bank.get();
        owned_.push_back(std::move(bank));
        current_.store(ptr,std::memory_order_release);
        return true;
    }
    std::size_t loadedZones() const noexcept {
        const auto* bank=current_.load(std::memory_order_acquire);
        return bank?bank->zones.size():0;
    }
    void noteOn(int key,int velocity,double outputRate) noexcept {
        if(key<0 || key>127 || velocity<1 || velocity>127 ||
           outputRate<8000 || outputRate>384000)return;
        const auto* bank=current_.load(std::memory_order_acquire);
        if(!bank || bank->zones.empty())return;
        int nearestNote=1000;
        for(const auto& zone:bank->zones)
            nearestNote=std::min(nearestNote,std::abs(key-zone.root));
        int chosenRoot=-1;
        for(const auto& zone:bank->zones) {
            if(std::abs(key-zone.root)==nearestNote) {
                chosenRoot=zone.root;break;
            }
        }
        int nearestVelocity=1000;
        for(const auto& zone:bank->zones)
            if(zone.root==chosenRoot)
                nearestVelocity=std::min(nearestVelocity,std::abs(velocity-zone.velocity));
        int selectedVel=-1;
        for(const auto& zone:bank->zones) {
            if(zone.root==chosenRoot && std::abs(velocity-zone.velocity)==nearestVelocity) {
                selectedVel=zone.velocity;break;
            }
        }
        int variants=0;
        for(const auto& zone:bank->zones)
            if(zone.root==chosenRoot && zone.velocity==selectedVel)++variants;
        if(variants==0)return;
        auto& sequence=rrCounters_[static_cast<std::size_t>(chosenRoot)*128+
                                   static_cast<std::size_t>(selectedVel)];
        const unsigned selected=sequence++%static_cast<unsigned>(variants);
        const SampleZone* chosen=nullptr;
        for(const auto& zone:bank->zones)
            if(zone.root==chosenRoot && zone.velocity==selectedVel &&
               static_cast<unsigned>(zone.rr)==selected+1) {
                chosen=&zone;break;
            }
        if(!chosen)return;
        // Reuse the first empty voice, or steal the oldest (smallest stamp).
        Voice* voice=nullptr;
        for(auto& v:voices_)if(!v.zone){voice=&v;break;}
        if(!voice)voice=&*std::min_element(voices_.begin(),voices_.end(),
            [](const Voice& a,const Voice& b){return a.stamp<b.stamp;});
        voice->zone=chosen;
        voice->position=0.;
        voice->increment=(static_cast<double>(chosen->sampleRate)/outputRate)*
            std::pow(2.,static_cast<double>(key-chosen->root)/12.);
        voice->stamp=++timestamp_;
        voice->key=key;
    }
    void noteOff(int) noexcept {
        // These WAVs contain the entire captured release. In this first
        // iteration they are played one-shot, not prematurely truncated.
    }
    StereoFrame renderOne() noexcept {
        StereoFrame mix{};
        for(auto& voice:voices_) {
            if(!voice.zone)continue;
            const auto& wave=voice.zone->pcm;
            const auto index=static_cast<std::size_t>(voice.position);
            if(index>=wave.size()) {voice.zone=nullptr;continue;}
            const auto next=std::min(index+1,wave.size()-1);
            const auto frac=static_cast<float>(voice.position-index);
            for(int ch=0;ch<2;++ch)
                mix[static_cast<std::size_t>(ch)]+=
                    wave[index][static_cast<std::size_t>(ch)]*(1.f-frac)+
                    wave[next][static_cast<std::size_t>(ch)]*frac;
            voice.position+=voice.increment;
            if(voice.position>=static_cast<double>(wave.size()))voice.zone=nullptr;
        }
        for(float& x:mix)x=std::clamp(x,-1.f,1.f);
        return mix;
    }
private:
    struct Voice {
        const SampleZone* zone=nullptr;
        double position=0.,increment=1.;
        std::uint64_t stamp=0;
        int key=0;
    };
    std::vector<std::unique_ptr<SampleBank>> owned_;
    std::atomic<const SampleBank*> current_{nullptr};
    std::array<Voice,maxVoices> voices_{};
    std::array<std::uint32_t,128*128> rrCounters_{};
    std::uint64_t timestamp_=0;
};
} // namespace a125
