#include "core/BuiltInSampler.h"
#include <iostream>
std::unique_ptr<a125::SampleBank> make(float a,float b){auto x=std::make_unique<a125::SampleBank>();for(int i=1;i<=2;++i){a125::SampleZone z;z.pcm.assign(1024,{i==1?a:b,0.f});z.rr=i;x->zones.push_back(z);}return x;}
int main(){a125::BuiltInSampler p;p.setGate(true);p.setEnvelope(0,0,1,0);p.publish(make(.1f,.2f));p.noteOn(60,100,44100);std::cout<<"old_rr01="<<p.renderOne()[0]<<'\n';p.noteOff(60);p.publish(make(.3f,.7f));p.noteOn(60,100,44100);float actual=p.renderOne()[0];std::cout<<"new_bank_first="<<actual<<" expected=0.3\n";p.noteOff(60);p.requestVoiceReset();p.noteOn(60,100,44100);std::cout<<"after_explicit_reset="<<p.renderOne()[0]<<'\n';return std::abs(actual-.3f)<1e-6f?0:1;}
