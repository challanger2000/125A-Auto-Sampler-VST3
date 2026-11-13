#include "bass_engine.hpp"
#include <cstdlib>
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED: " #x << "\n"; std::exit(1); } } while(false)
#include <cmath>
#include <iostream>
using a125::BassEngine;
int main() {
    BassEngine e;
    e.setSampleRate(44100.);
    for(int i=0;i<128;++i)REQUIRE(e.renderSample()==0.);
    e.noteOn(40,.75);
    double energy=0.;
    for(int i=0;i<4096;++i) {
        auto s=e.renderSample();
        REQUIRE(std::isfinite(s)&&std::abs(s)<=1.);
        energy+=s*s;
    }
    REQUIRE(energy>.01);
    e.noteOff(40);
    for(int i=0;i<88200;++i)e.renderSample();
    REQUIRE(!e.active());
    auto brightness=[](double vel) {
        BassEngine t;
        t.setSampleRate(44100.);
        t.setParameter(a125::VelocityColor,1.);
        t.setParameter(a125::CrossMod,.8);
        t.noteOn(40,vel);
        double diff=0.,power=0.,last=0.;
        for(int i=0;i<4096;++i) {
            double sample=t.renderSample();
            if(i>64){diff+=(sample-last)*(sample-last);power+=sample*sample;}
            last=sample;
        }
        return diff/(power+1.e-12);
    };
    REQUIRE(std::abs(brightness(.2)-brightness(.95))>1.e-4);
    e.setParameter(a125::Shape,10.);
    e.setParameter(a125::CrossMod,-1.);
    REQUIRE(e.parameter(a125::Shape)==1.);
    REQUIRE(e.parameter(a125::CrossMod)==0.);
    e.setParameter(a125::Output,0.);
    e.noteOn(40,1.);
    for(int i=0;i<4096;++i)REQUIRE(e.renderSample()==0.);
    e.reset();
    REQUIRE(!e.active());
    std::cout<<"125A DSP tests PASS\n";
}
