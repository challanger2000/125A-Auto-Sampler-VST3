#pragma once
// Strict bounded parser for the seven documented fields of the 125A v1
// instrument.json schema. Unknown, duplicate, malformed, or future fields
// fail closed. No third-party JSON DLL or realtime parsing.
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace a125 {
class ManifestCursor final {
public:
    explicit ManifestCursor(std::string_view text):text_(text){}
    bool take(char wanted) noexcept {
        ws();
        if(pos_>=text_.size() || text_[pos_]!=wanted)return false;
        ++pos_;
        return true;
    }
    bool done() noexcept {ws();return pos_==text_.size();}
    bool string(std::string& out) {
        if(!take('"'))return false;
        out.clear();
        while(pos_<text_.size()){
            const unsigned char ch=static_cast<unsigned char>(text_[pos_++]);
            if(ch=='"')return true;
            if(ch<0x20)return false;
            if(ch!='\\'){out+=static_cast<char>(ch);continue;}
            if(pos_==text_.size())return false;
            switch(text_[pos_++]){
                case '"':out+='"';break;
                case '\\':out+='\\';break;
                case '/':out+='/';break;
                case 'n':out+='\n';break;
                case 'r':out+='\r';break;
                case 't':out+='\t';break;
                case 'b':out+='\b';break;
                case 'f':out+='\f';break;
                case 'u':{
                    std::uint32_t cp=0;
                    if(!hex4(cp))return false;
                    if(cp>=0xD800 && cp<=0xDBFF){
                        if(pos_+2>text_.size() || text_[pos_]!='\\' ||
                           text_[pos_+1]!='u')return false;
                        pos_+=2;
                        std::uint32_t low=0;
                        if(!hex4(low) || low<0xDC00 || low>0xDFFF)return false;
                        cp=0x10000+((cp-0xD800)<<10)+(low-0xDC00);
                    }else if(cp>=0xDC00 && cp<=0xDFFF)return false;
                    if(cp<0x80)out+=static_cast<char>(cp);
                    else if(cp<0x800){
                        out+=static_cast<char>(0xC0|(cp>>6));
                        out+=static_cast<char>(0x80|(cp&63));
                    }else if(cp<0x10000){
                        out+=static_cast<char>(0xE0|(cp>>12));
                        out+=static_cast<char>(0x80|((cp>>6)&63));
                        out+=static_cast<char>(0x80|(cp&63));
                    }else{
                        out+=static_cast<char>(0xF0|(cp>>18));
                        out+=static_cast<char>(0x80|((cp>>12)&63));
                        out+=static_cast<char>(0x80|((cp>>6)&63));
                        out+=static_cast<char>(0x80|(cp&63));
                    }
                    break;
                }
                default:return false;
            }
        }
        return false;
    }
    bool unsignedInteger(std::uint64_t& value) noexcept {
        ws();
        const auto start=pos_;
        if(pos_>=text_.size())return false;
        if(text_[pos_]=='0'){
            ++pos_;
            if(pos_<text_.size() && digit(text_[pos_]))return false;
        }else{
            if(text_[pos_]<'1' || text_[pos_]>'9')return false;
            do{++pos_;}while(pos_<text_.size() && digit(text_[pos_]));
        }
        const auto* begin=text_.data()+start;
        const auto result=std::from_chars(begin,text_.data()+pos_,value);
        return result.ec==std::errc{} && result.ptr==text_.data()+pos_;
    }
    bool number(double& value) noexcept {
        ws();
        const auto start=pos_;
        if(pos_<text_.size() && text_[pos_]=='-')++pos_;
        if(pos_==text_.size())return false;
        if(text_[pos_]=='0'){
            ++pos_;
            if(pos_<text_.size() && digit(text_[pos_]))return false;
        }else{
            if(text_[pos_]<'1' || text_[pos_]>'9')return false;
            do{++pos_;}while(pos_<text_.size() && digit(text_[pos_]));
        }
        if(pos_<text_.size() && text_[pos_]=='.'){
            ++pos_;
            if(pos_==text_.size() || !digit(text_[pos_]))return false;
            do{++pos_;}while(pos_<text_.size() && digit(text_[pos_]));
        }
        if(pos_<text_.size() && (text_[pos_]=='e'||text_[pos_]=='E')){
            ++pos_;
            if(pos_<text_.size() && (text_[pos_]=='+' || text_[pos_]=='-'))++pos_;
            if(pos_==text_.size() || !digit(text_[pos_]))return false;
            do{++pos_;}while(pos_<text_.size() && digit(text_[pos_]));
        }
        const auto result=std::from_chars(text_.data()+start,text_.data()+pos_,value);
        return result.ec==std::errc{} &&
            result.ptr==text_.data()+pos_ && std::isfinite(value);
    }
private:
    static bool digit(char ch) noexcept {return ch>='0' && ch<='9';}
    void ws() noexcept {
        while(pos_<text_.size() && (text_[pos_]==' ' || text_[pos_]=='\n' ||
              text_[pos_]=='\r' || text_[pos_]=='\t'))++pos_;
    }
    bool hex4(std::uint32_t& value) noexcept {
        if(text_.size()-pos_<4)return false;
        value=0;
        for(int i=0;i<4;++i){
            const unsigned char ch=static_cast<unsigned char>(text_[pos_++]);
            unsigned digitValue=0;
            if(ch>='0' && ch<='9')digitValue=ch-'0';
            else if(ch>='a' && ch<='f')digitValue=ch-'a'+10;
            else if(ch>='A' && ch<='F')digitValue=ch-'A'+10;
            else return false;
            value=(value<<4)|digitValue;
        }
        return true;
    }
    std::string_view text_;
    std::size_t pos_=0;
};
inline bool parse125AManifest(std::string_view json,std::size_t expectedCount,
                              std::string& instrumentName,
                              std::array<double,7>& playback) {
    if(json.size()>4096 || expectedCount==0 || expectedCount>6144)return false;
    ManifestCursor cursor(json);
    if(!cursor.take('{'))return false;
    unsigned seen=0;
    do{
        std::string key;
        if(!cursor.string(key) || !cursor.take(':'))return false;
        int field=-1;
        if(key=="format")field=0;
        else if(key=="version")field=1;
        else if(key=="name")field=2;
        else if(key=="sampleCount")field=3;
        else if(key=="sampleFormat")field=4;
        else if(key=="mapping")field=5;
        else if(key=="playbackNormalized")field=6;
        if(field<0 || (seen & (1u<<field)))return false;
        seen|=1u<<field;
        if(field==0 || field==4 || field==5){
            std::string value;
            if(!cursor.string(value))return false;
            if((field==0 && value!="125A") ||
               (field==4 && value!="PCM24 stereo WAV") ||
               (field==5 && value!="root-note-velocity-round-robin filenames"))
                return false;
        }else if(field==1 || field==3){
            std::uint64_t value=0;
            if(!cursor.unsignedInteger(value))return false;
            if((field==1 && value!=1) ||
               (field==3 && value!=expectedCount))return false;
        }else if(field==2){
            if(!cursor.string(instrumentName) || instrumentName.empty() ||
               instrumentName.size()>192)return false;
            for(unsigned char ch:instrumentName)
                if(ch<32)return false;
        }else{
            if(!cursor.take('['))return false;
            for(std::size_t i=0;i<playback.size();++i){
                if(!cursor.number(playback[i]) ||
                   playback[i]<0. || playback[i]>1.)return false;
                if(i+1<playback.size() && !cursor.take(','))return false;
            }
            if(!cursor.take(']'))return false;
        }
        if(cursor.take('}'))return seen==0x7Fu && cursor.done();
    }while(cursor.take(','));
    return false;
}
} // namespace a125
