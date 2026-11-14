#pragma once
// 125A instrument archives: ordinary, unencrypted ZIP (method STORE).
// No ZIP DLL, no custom compression, no audio-thread I/O.
// This small reader deliberately supports only the format we actually write.
// All lengths, CRCs, offsets and file names are checked before publication.
#include "BuiltInSampler.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace a125 {

struct ArchiveEntry {
    std::string name;
    std::uint32_t crc=0, size=0, offset=0;
};
inline std::uint32_t zipCRC(std::uint32_t crc,const unsigned char* data,std::size_t count) noexcept {
    for(std::size_t i=0;i<count;++i){
        crc^=data[i];
        for(int bit=0;bit<8;++bit)
            crc=(crc>>1) ^ (0xEDB88320u & (0u-(crc&1u)));
    }
    return crc;
}
inline void zip16(std::ostream& o,std::uint16_t v){
    o.put(static_cast<char>(v));o.put(static_cast<char>(v>>8));
}
inline void zip32(std::ostream& o,std::uint32_t v){
    zip16(o,static_cast<std::uint16_t>(v));
    zip16(o,static_cast<std::uint16_t>(v>>16));
}
inline void zipBytes(std::ostream& o,std::string_view v){
    o.write(v.data(),static_cast<std::streamsize>(v.size()));
}
inline std::uint32_t zipPos(std::ostream& o){
    const auto pos=o.tellp();
    if(pos<0 || static_cast<std::uint64_t>(pos)>=0xFFFFFFFFull)
        throw std::runtime_error("125A archive exceeds ZIP32 size limit");
    return static_cast<std::uint32_t>(pos);
}
inline std::string json125Escape(std::string_view s){
    std::string out;
    for(unsigned char c:s){
        switch(c){
        case '"':out+="\\\"";break;
        case '\\':out+="\\\\";break;
        case '\n':out+="\\n";break;
        case '\r':out+="\\r";break;
        case '\t':out+="\\t";break;
        default:
            if(c<32){char b[7];std::snprintf(b,sizeof(b),"\\u%04X",c);out+=b;}
            else out+=static_cast<char>(c);
        }
    }
    return out;
}
inline bool valid125AName(const std::string& name){
    int root=0,vel=0,rr=0;
    return name=="instrument.json" || parseSampleName(name,root,vel,rr);
}
inline std::string instrumentManifest(const std::string& name,std::size_t count){
    if(name.empty() || name.size()>192)
        throw std::invalid_argument("125A instrument name is empty or too long");
    return std::string("{\n  \"format\":\"125A\",\n  \"version\":1,\n  \"name\":\"")+
        json125Escape(name)+"\",\n  \"sampleCount\":"+std::to_string(count)+
        ",\n  \"sampleFormat\":\"PCM24 stereo WAV\",\n  \"mapping\":\"root-note-velocity-round-robin filenames\"\n}\n";
}

// Only called after a fully successful capture; no overwriting a pre-existing
// instrument archive. An interrupted archive is removed, never published.
inline void write125AArchive(const std::filesystem::path& source,
                             const std::filesystem::path& destination,
                             const std::string& instrumentName){
    namespace fs=std::filesystem;
    if(destination.extension()!=".125A")
        throw std::invalid_argument("expected .125A extension");
    if(fs::exists(destination))
        throw std::runtime_error("125A archive already exists");
    std::vector<fs::path> files;
    for(const auto& f:fs::directory_iterator(source)){
        if(!f.is_regular_file() || f.path().extension()!=".wav")continue;
        const auto filename=f.path().filename().string();
        int n=0,v=0,rr=0;
        if(!parseSampleName(filename,n,v,rr))
            throw std::runtime_error("unknown WAV in 125A source");
        if(f.file_size()<50 || f.file_size()>128ull*1024*1024)
            throw std::runtime_error("invalid WAV size in 125A source");
        files.push_back(f.path());
    }
    if(files.empty() || files.size()>6144)
        throw std::runtime_error("invalid 125A sample count");
    std::sort(files.begin(),files.end());
    const auto manifest=instrumentManifest(instrumentName,files.size());
    const fs::path partial=destination.string()+".partial";
    if(fs::exists(partial))throw std::runtime_error("stale partial archive");
    try {
        std::ofstream out(partial,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot open 125A output");
        std::vector<ArchiveEntry> entries;
        entries.reserve(files.size()+1);
        auto add=[&](const std::string& name,const unsigned char* bytes,
                     std::size_t size){
            if(size>0xFFFFFFFFull || name.size()>0xFFFF)
                throw std::runtime_error("125A entry too large");
            ArchiveEntry entry;
            entry.name=name;entry.size=static_cast<std::uint32_t>(size);
            entry.crc=~zipCRC(0xFFFFFFFFu,bytes,size);
            entry.offset=zipPos(out);
            zip32(out,0x04034B50u);zip16(out,20);zip16(out,0);
            zip16(out,0);zip16(out,0);zip16(out,0);
            zip32(out,entry.crc);zip32(out,entry.size);zip32(out,entry.size);
            zip16(out,static_cast<std::uint16_t>(name.size()));zip16(out,0);
            zipBytes(out,name);
            out.write(reinterpret_cast<const char*>(bytes),
                      static_cast<std::streamsize>(size));
            entries.push_back(std::move(entry));
        };
        add("instrument.json",reinterpret_cast<const unsigned char*>(manifest.data()),
            manifest.size());
        for(const auto& path:files){
            std::ifstream in(path,std::ios::binary|std::ios::ate);
            if(!in)throw std::runtime_error("125A input missing");
            const auto length=in.tellg();
            if(length<50 || length>std::streamoff(128*1024*1024))
                throw std::runtime_error("125A input length changed");
            in.seekg(0);
            std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
            in.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            if(!in)throw std::runtime_error("125A input read failed");
            add(path.filename().string(),bytes.data(),bytes.size());
        }
        const auto central=zipPos(out);
        for(const auto& e:entries){
            zip32(out,0x02014B50u);
            zip16(out,20);zip16(out,20);zip16(out,0);zip16(out,0);
            zip16(out,0);zip16(out,0);
            zip32(out,e.crc);zip32(out,e.size);zip32(out,e.size);
            zip16(out,static_cast<std::uint16_t>(e.name.size()));
            zip16(out,0);zip16(out,0);zip16(out,0);zip16(out,0);
            zip32(out,0);zip32(out,e.offset);zipBytes(out,e.name);
        }
        const auto end=zipPos(out);
        zip32(out,0x06054B50u);
        zip16(out,0);zip16(out,0);
        zip16(out,static_cast<std::uint16_t>(entries.size()));
        zip16(out,static_cast<std::uint16_t>(entries.size()));
        zip32(out,end-central);zip32(out,central);zip16(out,0);
        out.flush();
        if(!out)throw std::runtime_error("125A write failed");
        out.close();
        if(!out)throw std::runtime_error("125A close failed");
        if(fs::exists(destination))
            throw std::runtime_error("125A destination appeared during write");
        fs::rename(partial,destination);
    }catch(...){
        std::error_code ec;
        fs::remove(partial,ec);
        throw;
    }
}

inline std::uint16_t zipRd16(const unsigned char* p) noexcept {
    return samplerLE16(p);
}
inline std::uint32_t zipRd32(const unsigned char* p) noexcept {
    return samplerLE32(p);
}
inline void zipSeek(std::ifstream& in,std::uint64_t offset,
                    unsigned char* dest,std::size_t count){
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset));
    if(!in || count>static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("125A invalid ZIP seek");
    in.read(reinterpret_cast<char*>(dest),static_cast<std::streamsize>(count));
    if(!in)throw std::runtime_error("125A truncated ZIP entry");
}
inline std::vector<ArchiveEntry> inspect125A(
        std::ifstream& in,std::uint64_t length){
    if(length<22 || length>=0xFFFFFFFFull)
        throw std::runtime_error("125A invalid archive length");
    const auto tailLen=static_cast<std::size_t>(std::min<std::uint64_t>(length,65557));
    std::vector<unsigned char> tail(tailLen);
    zipSeek(in,length-tailLen,tail.data(),tailLen);
    std::size_t eocd=tail.size();
    // Search end-of-central-directory backwards; respect declared ZIP comment.
    for(std::size_t i=tailLen-22;;--i){
        if(zipRd32(tail.data()+i)==0x06054B50u &&
           i+22+zipRd16(tail.data()+i+20)==tail.size()){
            eocd=i;break;
        }
        if(i==0)break;
    }
    if(eocd==tail.size())throw std::runtime_error("125A missing ZIP directory");
    const auto* end=tail.data()+eocd;
    const auto count=zipRd16(end+10);
    const auto centralSize=zipRd32(end+12);
    const auto centralOff=zipRd32(end+16);
    const auto eocdOff=length-tailLen+eocd;
    if(zipRd16(end+4)!=0 || zipRd16(end+6)!=0 ||
       count!=zipRd16(end+8) || count<2 || count>6145 ||
       centralOff>=length || static_cast<std::uint64_t>(centralOff)+centralSize!=eocdOff)
        throw std::runtime_error("125A invalid ZIP directory metadata");
    std::vector<unsigned char> central(centralSize);
    zipSeek(in,centralOff,central.data(),central.size());
    std::size_t pos=0;
    std::vector<ArchiveEntry> entries;
    entries.reserve(count);
    std::map<std::string,bool> names;
    for(unsigned i=0;i<count;++i){
        if(pos+46>central.size() || zipRd32(central.data()+pos)!=0x02014B50u)
            throw std::runtime_error("125A corrupt ZIP central directory");
        const auto* c=central.data()+pos;
        const auto flags=zipRd16(c+8),method=zipRd16(c+10);
        const auto size=zipRd32(c+24),packed=zipRd32(c+20);
        const auto nameSize=zipRd16(c+28),extra=zipRd16(c+30),comment=zipRd16(c+32);
        const auto offset=zipRd32(c+42);
        if(flags!=0 || method!=0 || size!=packed || nameSize==0 ||
           nameSize>80 || zipRd16(c+34)!=0 || zipRd16(c+36)!=0 ||
           static_cast<std::uint64_t>(pos)+46+nameSize+extra+comment>central.size() ||
           offset>=centralOff)
            throw std::runtime_error("125A unsupported or malformed ZIP member");
        ArchiveEntry entry;
        entry.name.assign(reinterpret_cast<const char*>(c+46),nameSize);
        entry.size=size;entry.crc=zipRd32(c+16);entry.offset=offset;
        if(!valid125AName(entry.name) || !names.emplace(entry.name,true).second ||
           (entry.name=="instrument.json" && entry.size>4096) ||
           (entry.name!="instrument.json" && (size<50 || size>128u*1024u*1024u)))
            throw std::runtime_error("125A invalid or duplicated entry");
        entries.push_back(std::move(entry));
        pos+=46+nameSize+extra+comment;
    }
    if(pos!=central.size() || !names.contains("instrument.json"))
        throw std::runtime_error("125A ZIP missing manifest or unexpected data");
    return entries;
}
inline std::vector<unsigned char> read125AEntry(
        std::ifstream& in,const ArchiveEntry& e,std::uint64_t centralOff){
    std::array<unsigned char,30> header{};
    if(static_cast<std::uint64_t>(e.offset)+header.size()>centralOff)
        throw std::runtime_error("125A invalid ZIP local offset");
    zipSeek(in,e.offset,header.data(),header.size());
    const auto nameSize=zipRd16(header.data()+26);
    const auto extra=zipRd16(header.data()+28);
    const auto dataOff=static_cast<std::uint64_t>(e.offset)+30+nameSize+extra;
    if(zipRd32(header.data())!=0x04034B50u ||
       zipRd16(header.data()+6)!=0 ||
       zipRd16(header.data()+8)!=0 ||
       zipRd32(header.data()+14)!=e.crc ||
       zipRd32(header.data()+18)!=e.size ||
       zipRd32(header.data()+22)!=e.size ||
       nameSize!=e.name.size() ||
       dataOff+e.size>centralOff)
        throw std::runtime_error("125A local ZIP header mismatch");
    std::vector<unsigned char> name(nameSize);
    zipSeek(in,static_cast<std::uint64_t>(e.offset)+30,name.data(),nameSize);
    if(std::string(reinterpret_cast<const char*>(name.data()),name.size())!=e.name)
        throw std::runtime_error("125A local ZIP name mismatch");
    std::vector<unsigned char> data(e.size);
    zipSeek(in,dataOff,data.data(),data.size());
    if(~zipCRC(0xFFFFFFFFu,data.data(),data.size())!=e.crc)
        throw std::runtime_error("125A ZIP sample CRC mismatch");
    return data;
}

inline bool readCapturedWavBytes(const std::vector<unsigned char>& bytes,
                                SampleZone& zone){
    if(bytes.size()<50 || bytes.size()>128ull*1024*1024)return false;
    const auto* header=bytes.data();
    if(std::string(reinterpret_cast<const char*>(header),4)!="RIFF" ||
       std::string(reinterpret_cast<const char*>(header+8),4)!="WAVE" ||
       std::string(reinterpret_cast<const char*>(header+12),4)!="fmt " ||
       samplerLE32(header+16)!=16 || samplerLE16(header+20)!=1 ||
       samplerLE16(header+22)!=2 || samplerLE16(header+34)!=24 ||
       samplerLE16(header+32)!=6 ||
       std::string(reinterpret_cast<const char*>(header+36),4)!="data")
        return false;
    const auto rate=samplerLE32(header+24),size=samplerLE32(header+40);
    if(rate<8000 || rate>384000 || !size || size%6!=0 ||
       static_cast<std::uint64_t>(size)+44!=bytes.size() ||
       samplerLE32(header+4)!=size+36 || samplerLE32(header+28)!=rate*6u)
        return false;
    zone.sampleRate=rate;
    zone.pcm.resize(size/6);
    for(std::size_t i=0;i<zone.pcm.size();++i){
        const auto* src=bytes.data()+44+i*6;
        for(int ch=0;ch<2;++ch){
            const auto* p=src+ch*3;
            std::int32_t raw=static_cast<std::int32_t>(
                static_cast<std::uint32_t>(p[0]) |
                (static_cast<std::uint32_t>(p[1])<<8) |
                (static_cast<std::uint32_t>(p[2])<<16));
            if(raw&0x800000)raw|=static_cast<std::int32_t>(0xFF000000u);
            zone.pcm[i][static_cast<std::size_t>(ch)]=static_cast<float>(raw/8388608.);
        }
    }
    return true;
}

inline std::unique_ptr<SampleBank> load125AArchive(
       const std::filesystem::path& archive,std::string* instrumentName=nullptr){
    try {
        std::ifstream in(archive,std::ios::binary|std::ios::ate);
        if(!in)return {};
        const auto end=in.tellg();
        if(end<22)return {};
        const auto length=static_cast<std::uint64_t>(end);
        auto entries=inspect125A(in,length);
        const auto manifestIt=std::find_if(entries.begin(),entries.end(),
            [](const ArchiveEntry& e){return e.name=="instrument.json";});
        if(manifestIt==entries.end())return {};
        const auto manifestBytes=read125AEntry(in,*manifestIt,length);
        const std::string manifest(manifestBytes.begin(),manifestBytes.end());
        // Readers accept only our manifest schema, never arbitrary ZIP files.
        if(manifest.find("\"format\":\"125A\"")==std::string::npos ||
           manifest.find("\"version\":1")==std::string::npos ||
           manifest.find("\"sampleCount\":"+std::to_string(entries.size()-1))==
               std::string::npos)return {};
        auto bank=std::make_unique<SampleBank>();
        std::map<std::pair<int,int>,std::set<int>> groups;
        std::uint32_t sampleRate=0;
        for(const auto& entry:entries){
            if(entry.name=="instrument.json")continue;
            SampleZone zone;
            if(!parseSampleName(entry.name,zone.root,zone.velocity,zone.rr))
                return {};
            auto pcm=read125AEntry(in,entry,length);
            if(!readCapturedWavBytes(pcm,zone) ||
               (sampleRate!=0 && sampleRate!=zone.sampleRate))return {};
            sampleRate=zone.sampleRate;
            groups[{zone.root,zone.velocity}].insert(zone.rr);
            bank->zones.push_back(std::move(zone));
        }
        for(const auto& [key,rounds]:groups){
            int expected=0;
            for(int rr:rounds)if(rr!=++expected)return {};
        }
        std::sort(bank->zones.begin(),bank->zones.end(),[](const auto& a,const auto& b){
            if(a.root!=b.root)return a.root<b.root;
            if(a.velocity!=b.velocity)return a.velocity<b.velocity;
            return a.rr<b.rr;
        });
        float peak=0.f;
        for(const auto& zone:bank->zones)
            for(const auto& frame:zone.pcm)
                for(float x:frame)peak=std::max(peak,std::abs(x));
        if(peak>=.001f && std::isfinite(peak))
            bank->normalizeGain=std::pow(10.f,-1.f/20.f)/peak;
        if(instrumentName){
            const auto nameStart=manifest.find("\"name\":\"");
            if(nameStart!=std::string::npos){
                const auto first=nameStart+8;
                const auto last=manifest.find('"',first);
                if(last!=std::string::npos)*instrumentName=manifest.substr(first,last-first);
            }
        }
        return bank;
    }catch(const std::exception&){return {};}
}
} // namespace a125
