#include "bass_engine.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstpresetkeys.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>

namespace a125 {
using namespace Steinberg;
using namespace Steinberg::Vst;
static const FUID processorId(0x7DB17ED9,0x07B84F5E,0xA2DB65C0,0xC641D125);
static const FUID controllerId(0xBB761C92,0x66AF4B6F,0x9BBE4380,0xE0AD5126);
static constexpr int32 stateVersion=1;

class Processor final : public AudioEffect {
public:
    Processor() {setControllerClass(controllerId);engine_.setSampleRate(44100.);}
    static FUnknown* createInstance(void*) {return (IAudioProcessor*)new Processor;}
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto r=AudioEffect::initialize(context);
        if(r!=kResultOk)return r;
        addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo);
        addEventInput(STR16("MIDI In"),16);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32 nIn,
                         SpeakerArrangement* out,int32 nOut) override {
        if(nIn==0&&nOut==1&&out&&out[0]==SpeakerArr::kStereo)
            return AudioEffect::setBusArrangements(nullptr,0,out,nOut);
        return kResultFalse;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 fmt) override {
        return (fmt==kSample32||fmt==kSample64)?kResultTrue:kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        auto r=AudioEffect::setupProcessing(setup);
        if(r==kResultOk)engine_.setSampleRate(setup.sampleRate);
        return r;
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(!active)engine_.reset();
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API getState(IBStream* stream) override {
        if(!stream)return kResultFalse;
        IBStreamer s(stream,kLittleEndian);
        if(!s.writeInt32(stateVersion))return kResultFalse;
        for(std::size_t i=0;i<ParameterCount;++i)
            if(!s.writeFloat(static_cast<float>(engine_.parameter(i))))
                return kResultFalse;
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* stream) override {
        if(!stream)return kResultFalse;
        IBStreamer s(stream,kLittleEndian);
        int32 version=0;
        if(!s.readInt32(version)||version!=stateVersion)return kResultFalse;
        std::array<float,ParameterCount> values{};
        for(auto& value:values)if(!s.readFloat(value))return kResultFalse;
        for(std::size_t i=0;i<ParameterCount;++i)engine_.setParameter(i,values[i]);
        return kResultOk;
    }
    tresult PLUGIN_API process(ProcessData& data) override {
        // P001: final automation point per block. MIDI is sample-offset accurate.
        if(auto* changes=data.inputParameterChanges) {
            for(int32 i=0;i<changes->getParameterCount();++i) {
                auto* q=changes->getParameterData(i);
                if(!q||q->getPointCount()<=0)continue;
                int32 offset=0;ParamValue value=0;
                if(q->getPoint(q->getPointCount()-1,offset,value)==kResultOk)
                    engine_.setParameter(q->getParameterId(),value);
            }
        }
        if(!data.outputs||data.numOutputs<1||data.numSamples<=0)return kResultOk;
        auto& bus=data.outputs[0];
        if(bus.numChannels!=2)return kResultFalse;
        auto* events=data.inputEvents;
        const int32 total=events?events->getEventCount():0;
        int32 eventIndex=0;
        Event next{};
        bool have=total>0&&events->getEvent(0,next)==kResultOk;
        auto dispatch=[&](int32 sampleIndex) {
            while(have&&next.sampleOffset<=sampleIndex) {
                if(next.type==Event::kNoteOnEvent)
                    engine_.noteOn(next.noteOn.pitch,next.noteOn.velocity);
                else if(next.type==Event::kNoteOffEvent)
                    engine_.noteOff(next.noteOff.pitch);
                ++eventIndex;
                have=eventIndex<total&&events->getEvent(eventIndex,next)==kResultOk;
            }
        };
        bool audible=false;
        if(data.symbolicSampleSize==kSample32) {
            if(!bus.channelBuffers32||!bus.channelBuffers32[0]||
               !bus.channelBuffers32[1])return kResultFalse;
            for(int32 i=0;i<data.numSamples;++i) {
                dispatch(i);
                const auto sample=static_cast<float>(engine_.renderSample());
                bus.channelBuffers32[0][i]=sample;
                bus.channelBuffers32[1][i]=sample;
                audible|=(sample!=0.f);
            }
        } else if(data.symbolicSampleSize==kSample64) {
            if(!bus.channelBuffers64||!bus.channelBuffers64[0]||
               !bus.channelBuffers64[1])return kResultFalse;
            for(int32 i=0;i<data.numSamples;++i) {
                dispatch(i);
                const auto sample=engine_.renderSample();
                bus.channelBuffers64[0][i]=sample;
                bus.channelBuffers64[1][i]=sample;
                audible|=(sample!=0.);
            }
        } else return kResultFalse;
        bus.silenceFlags=audible?0:3;
        return kResultOk;
    }
private:
    BassEngine engine_{};
};

class Controller final : public EditController {
public:
    static FUnknown* createInstance(void*) {return (IEditController*)new Controller;}
    tresult PLUGIN_API initialize(FUnknown* context) override {
        auto r=EditController::initialize(context);
        if(r!=kResultOk)return r;
        const TChar* names[]={
            STR16("Shape"),STR16("Cross Mod"),STR16("Cutoff"),
            STR16("Resonance"),STR16("Filter Env"),STR16("Velocity Color"),
            STR16("Decay"),STR16("Release"),STR16("Output")
        };
        for(std::size_t i=0;i<ParameterCount;++i)
            parameters.addParameter(names[i],nullptr,0,defaults[i],
                     ParameterInfo::kCanAutomate,static_cast<ParamID>(i));
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        if(!stream)return kResultFalse;
        IBStreamer s(stream,kLittleEndian);
        int32 version=0;
        if(!s.readInt32(version)||version!=stateVersion)return kResultFalse;
        for(std::size_t i=0;i<ParameterCount;++i) {
            float value=0.f;
            if(!s.readFloat(value))return kResultFalse;
            setParamNormalized(static_cast<ParamID>(i),
                               std::clamp<double>(value,0.,1.));
        }
        return kResultOk;
    }
};
} // namespace a125

BEGIN_FACTORY_DEF("125A","https://github.com/challanger2000/125A-Digital-Bass-VST3","")
    DEF_CLASS2(INLINE_UID_FROM_FUID(a125::processorId),
        Steinberg::PClassInfo::kManyInstances,
        kVstAudioEffectClass,"125A Digital Bass",
        Steinberg::Vst::kDistributable,Steinberg::Vst::PlugType::kInstrumentSynth,
        "0.1.0",kVstVersionString,a125::Processor::createInstance)
    DEF_CLASS2(INLINE_UID_FROM_FUID(a125::controllerId),
        Steinberg::PClassInfo::kManyInstances,
        kVstComponentControllerClass,"125A Digital Bass Controller",
        0,"","0.1.0",kVstVersionString,
        a125::Controller::createInstance)
END_FACTORY
