#include "core/BuiltInSampler.h"
#include "core/InstrumentArchive.h"
#include <iostream>
#include <limits>
std::unique_ptr<a125::SampleBank> bank(bool rr=false){
 auto b=std::make_unique<a125::SampleBank>();
 a125::SampleZone z;z.root=60;z.velocity=100;z.rr=1;z.pcm.assign(4096,{.25f,-.125f});
 b->zones.push_back(z);
 if(rr){z.rr=2;z.pcm.assign(4096,{.75f,-.375f});b->zones.push_back(z);}
 return b;
}
int main(){
 a125::BuiltInSampler p,ref;
 p.publish(bank());p.setNormalize(true);ref.publish(bank());ref.setNormalize(true);
 for(int i=0;i<32;++i)p.noteOn(60,100,44100.,i);
 std::cout<<"stacked="<<p.renderOne()[0]<<'\n';
 p.publish(bank());p.requestVoiceReset();p.noteOn(60,100,44100.,100);
 ref.noteOn(60,100,44100.,100);
 std::cout<<"recall_first="<<p.renderOne()[0]<<" fresh_first="<<ref.renderOne()[0]<<'\n';
 a125::BuiltInSampler r,rrref;r.publish(bank(true));rrref.publish(bank(true));
 r.noteOn(60,100,44100.);std::cout<<"rr_before="<<r.renderOne()[0]<<'\n';
 r.publish(bank(true));r.requestVoiceReset();r.noteOn(60,100,44100.);
 rrref.noteOn(60,100,44100.);
 std::cout<<"rr_recall="<<r.renderOne()[0]<<" rr_fresh="<<rrref.renderOne()[0]<<'\n';
 std::string json=a125::instrumentManifest(std::string("bad")+char(0xff),1,{}),name;
 std::array<double,7> values{};
 std::cout<<"invalid_utf8_accepted="<<a125::parse125AManifest(json,1,name,values)<<'\n';
}
