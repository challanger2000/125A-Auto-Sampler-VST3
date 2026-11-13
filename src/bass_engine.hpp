#pragma once
// Original prototype DSP, not a port or reconstruction of EVM Isotope.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace a125 {
enum Parameter : std::size_t {
    Shape, CrossMod, Cutoff, Resonance, FilterEnv, VelocityColor,
    Decay, Release, Output, ParameterCount
};
inline constexpr std::array<double,ParameterCount> defaults{
    .36,.35,.43,.30,.55,.85,.35,.22,.65
};
class BassEngine {
public:
    void setSampleRate(double sr) noexcept {
        sr_=std::clamp(sr,8000.,384000.); update(); reset();
    }
    void setParameter(std::size_t id,double value) noexcept {
        if(id>=ParameterCount||!std::isfinite(value))return;
        p_[id]=std::clamp(value,0.,1.);
        if(id==Decay||id==Release)update();
    }
    double parameter(std::size_t id) const noexcept {
        return id<ParameterCount?p_[id]:0.;
    }
    void reset() noexcept {
        phase_=mod_=sub_=ic1_=ic2_=amp_=env_=0.;
        on_=gate_=false; note_=-1;
    }
    void noteOn(int note,double velocity) noexcept {
        if(velocity<=0.) {noteOff(note);return;}
        if(note<0||note>127)return;
        note_=note; vel_=std::clamp(velocity,0.,1.);
        hz_=440.*std::exp2((note-69.)/12.);
        phase_=mod_=sub_=ic1_=ic2_=amp_=0.;
        env_=1.; on_=gate_=true;
    }
    void noteOff(int note) noexcept {if(note==note_)gate_=false;}
    bool active() const noexcept {return on_;}
    double renderSample() noexcept {
        if(!on_)return 0.;
        const double target=gate_?1.:0.;
        amp_=target+(amp_-target)*(gate_?attack_:release_);
        env_*=decay_;
        if(!gate_&&amp_<1.e-7){amp_=0.;on_=false;return 0.;}
        const double vc=vel_*p_[VelocityColor];
        const double shape=std::clamp(p_[Shape]+.55*vc+.12*env_,0.,.95);
        const double split=.5+.42*shape;
        const double warped=phase_<split
            ?.5*phase_/split
            :.5+.5*(phase_-split)/(1.-split);
        const double pd=phase_+shape*(warped-phase_);
        const double index=p_[CrossMod]*(.1+2.5*vc+.8*env_);
        const double osc=std::sin(tau*pd+index*std::sin(tau*mod_));
        const double signal=.8*(.84*osc+.16*std::sin(tau*sub_));
        advance(phase_,hz_/sr_);
        advance(mod_,2.*hz_/sr_);
        advance(sub_,.5*hz_/sr_);
        const double cutoff=std::clamp(65.*std::exp2(
            7.*p_[Cutoff]+3.5*p_[FilterEnv]*env_+3.*vc),30.,.44*sr_);
        const double g=std::tan(pi*cutoff/sr_);
        const double k=1./(.65+7.*p_[Resonance]);
        const double a=1./(1.+g*(g+k));
        const double v1=a*(ic1_+g*(signal-ic2_));
        const double v2=ic2_+g*v1;
        ic1_=2.*v1-ic1_;ic2_=2.*v2-ic2_;
        const double s=std::tanh(1.6*v2)*amp_*(.75+.25*vel_)*
                       p_[Output]*.75;
        return std::isfinite(s)?s:0.;
    }
private:
    static constexpr double pi=3.14159265358979323846,tau=2.*pi;
    static void advance(double& phase,double step) noexcept {
        phase+=step;phase-=std::floor(phase);
    }
    void update() noexcept {
        attack_=std::exp(-1./(.002*sr_));
        const double d=.015+.9*p_[Decay]*p_[Decay];
        const double r=.015+1.4*p_[Release]*p_[Release];
        decay_=std::exp(-1./(d*sr_));
        release_=std::exp(-1./(r*sr_));
    }
    std::array<double,ParameterCount> p_=defaults;
    double sr_=44100.,hz_=82.41,vel_=1.;
    double phase_=0.,mod_=0.,sub_=0.,ic1_=0.,ic2_=0.,amp_=0.,env_=0.;
    double attack_=0.,decay_=0.,release_=0.;
    int note_=-1;bool on_=false,gate_=false;
};
}
