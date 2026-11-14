from pathlib import Path
import shutil,subprocess
out=Path('/workspace/autosampler-reaudit-f7d2/loose-race');out.mkdir(exist_ok=True)
repo=Path('/workspace/125A-Auto-Sampler-VST3')
shutil.copytree(repo/'src/core',out/'src/core',dirs_exist_ok=True)
p=out/'src/core/BuiltInSampler.h';s=p.read_text();anchor='    std::ifstream in(path,std::ios::binary|std::ios::ate);'
assert s.count(anchor)==1
s=s.replace(anchor,'    extern void auditReplaceAfterStat(const std::filesystem::path&);\n    auditReplaceAfterStat(path);\n'+anchor);p.write_text(s)
(out/'race.cpp').write_text(r'''
#include "core/BuiltInSampler.h"
#include <iostream>
#include <cstring>
namespace a125 {
bool auditActive=true;
void auditReplaceAfterStat(const std::filesystem::path& path){
 if(!auditActive)return;
 std::ofstream o(path,std::ios::binary|std::ios::trunc);
 const uint32_t payload=120u*1024u*1024u; // divisible by 6
 o.write("RIFF",4);writeLE32(o,payload+36);o.write("WAVEfmt ",8);writeLE32(o,16);
 writeLE16(o,1);writeLE16(o,2);writeLE32(o,44100);writeLE32(o,44100*6);
 writeLE16(o,6);writeLE16(o,24);o.write("data",4);writeLE32(o,payload);
 o.seekp(44ull+payload-1);o.put(0);o.close();
}
}
int main(){
 const auto root=std::filesystem::path("fixtures");std::filesystem::create_directories(root);
 for(int n=60;n<64;++n){std::vector<a125::StereoFrame> one(1,{.25f,.125f});a125::writeWav24((root/("N"+std::to_string(n)+"_V100_RR01.wav")).string(),44100,one);}
 auto bank=a125::loadCapturedBank(root);uint64_t bytes=0;
 if(bank)for(const auto& zone:bank->zones)bytes+=zone.pcm.size()*sizeof(a125::StereoFrame);
 std::cout<<"accepted="<<bool(bank)<<" decoded="<<bytes<<" limit="<<a125::kMaxBankDecodedBytes<<'\n';
 bank.reset();a125::auditActive=false;
 auto positive=a125::loadCapturedBank(root);
 std::cout<<"unchanged_large_files_rejected="<<!positive<<'\n';
 std::filesystem::remove_all(root);
 return !positive && bytes>a125::kMaxBankDecodedBytes ? 0:1;
 // return bank && bytes>a125::kMaxBankDecodedBytes ? 0:1;
}
''')
subprocess.run(['g++','-std=c++23','-O2','-I',str(out/'src'),str(out/'race.cpp'),'-o',str(out/'race')],check=True)
r=subprocess.run([str(out/'race')],cwd=out,capture_output=True,text=True);(out/'result.log').write_text(r.stdout+r.stderr);print(r.stdout,r.stderr);raise SystemExit(r.returncode)
