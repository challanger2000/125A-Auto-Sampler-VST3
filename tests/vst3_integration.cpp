// 125A P005 native integration test, NOT part of the shipping plugin.
// Loads both actual VST3 classes, generates their MIDI events, supplies a
// synthetic oscillator as stand-in for a DAW-hosted instrument, and checks
// the exact WAV files written by the real recorder worker.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VST3::Hosting;
namespace {
constexpr int kRate=44100;
constexpr int kBlock=256;
constexpr double kTau=6.28318530717958647692;
void require(bool value,const char* what) {
    if(!value)throw std::runtime_error(what);
}
struct Instance {
    IPtr<IComponent> component;
    IAudioProcessor* processor=nullptr;
    bool active=false;
    Instance()=default;
    Instance(const Instance&)=delete;
    Instance& operator=(const Instance&)=delete;
    Instance(Instance&& rhs) noexcept:
        component(std::move(rhs.component)),processor(rhs.processor),active(rhs.active) {
        rhs.processor=nullptr;rhs.active=false;
    }
    ~Instance(){
        if(active && component)(void)component->setActive(false);
        if(processor)processor->release();
        if(component)(void)component->terminate();
    }
};
Instance load(PluginFactory& factory,HostApplication& host,const std::string& target) {
    for(const auto& cls:factory.classInfos()){
        if(cls.category()!=kVstAudioEffectClass || cls.name()!=target)continue;
        Instance plug;
        plug.component=factory.createInstance<IComponent>(cls.ID());
        require(bool(plug.component),"cannot create requested VST3 processor class");
        require(plug.component->initialize(&host)==kResultOk,"VST3 initialize failed");
        require(plug.component->queryInterface(IAudioProcessor::iid,
               reinterpret_cast<void**>(&plug.processor))==kResultOk &&
               plug.processor,"VST3 IAudioProcessor unavailable");
        require(plug.processor->canProcessSampleSize(kSample32)==kResultTrue,
                "VST3 32-bit PCM unsupported");
        ProcessSetup setup{};
        setup.sampleRate=kRate;
        setup.processMode=kRealtime;
        setup.symbolicSampleSize=kSample32;
        setup.maxSamplesPerBlock=kBlock;
        require(plug.processor->setupProcessing(setup)==kResultOk,
                "VST3 setupProcessing failed");
        require(plug.component->setActive(true)==kResultOk,
                "VST3 setActive(true) failed");
        plug.active=true;
        return plug;
    }
    throw std::runtime_error("expected 125A plugin class missing");
}
std::size_t nonzeroWav(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    require(bool(in),"cannot open exported WAV");
    const std::vector<std::uint8_t> bytes(
        std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>{});
    require(bytes.size()>44+12000*6,"WAV missing sustained source note");
    require(std::string(bytes.begin(),bytes.begin()+4)=="RIFF","WAV RIFF missing");
    require(std::string(bytes.begin()+8,bytes.begin()+12)=="WAVE","WAV WAVE missing");
    require(std::string(bytes.begin()+36,bytes.begin()+40)=="data","WAV data missing");
    require(bytes[22]==2 && bytes[23]==0,"WAV must be stereo");
    require(bytes[34]==24 && bytes[35]==0,"WAV must be PCM 24-bit");
    std::size_t count=0;
    for(std::size_t i=44;i<bytes.size();i+=6)
        if(bytes[i] || bytes[i+1] || bytes[i+2])++count;
    require(count>12000,"WAV must contain genuinely audible synthetic signal");
    return bytes.size();
}
int run(const char* vst3path){
    const auto base=std::filesystem::absolute("p005-real-vst3-output");
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base/"Documents");
    require(_putenv_s("USERPROFILE",base.string().c_str())==0,
            "cannot isolate recorder output directory");
    std::string error;
    auto module=Module::create(vst3path,error);
    require(bool(module),"cannot load actual 125A VST3 bundle");
    HostApplication host;
    auto factory=module->getFactory();
    factory.setHostContext(&host);

    auto generator=load(factory,host,"125A AutoSampler MIDI");
    auto recorder=load(factory,host,"125A AutoSampler Recorder");
    require(generator.component->getBusCount(kEvent,kOutput)==1,
            "MIDI generator must expose an event output bus");
    require(recorder.component->getBusCount(kAudio,kInput)==1 &&
            recorder.component->getBusCount(kAudio,kOutput)==1,
            "recorder must expose stereo input/output");
    (void)generator.component->activateBus(kEvent,kOutput,0,true);

    std::array<Sample32,kBlock> genL{},genR{},audioL{},audioR{},passL{},passR{};
    std::array<Sample32*,2> genPtrs{genL.data(),genR.data()};
    std::array<Sample32*,2> audioPtrs{audioL.data(),audioR.data()};
    std::array<Sample32*,2> passPtrs{passL.data(),passR.data()};
    AudioBusBuffers genOut{};
    genOut.numChannels=2;genOut.channelBuffers32=genPtrs.data();
    AudioBusBuffers source{};
    source.numChannels=2;source.channelBuffers32=audioPtrs.data();
    AudioBusBuffers passthrough{};
    passthrough.numChannels=2;passthrough.channelBuffers32=passPtrs.data();

    EventList midiOut(32);
    ParameterChanges params(1);
    int32 queueIndex=0,pointIndex=0;
    auto* runQueue=params.addParameterData(1000,queueIndex);
    require(runQueue && runQueue->addPoint(0,1.,pointIndex)==kResultOk,
            "cannot start MIDI via VST3 parameter queue");
    double phase=0.;
    float amplitude=0.f;
    bool held=false;
    int noteOns=0,noteOffs=0;
    constexpr std::int64_t period=44100+88200+4410;
    constexpr std::int64_t total=22050+8*period+512;
    // Feed the exact same sample positions through both plugin components.
    // A 1ms pause approximates a working host and lets the I/O worker drain.
    for(std::int64_t start=0;start<total;start+=kBlock){
        const int32 frames=static_cast<int32>(
            std::min<std::int64_t>(kBlock,total-start));
        ProcessContext context{};
        context.state=ProcessContext::kPlaying;
        context.sampleRate=kRate;
        context.projectTimeSamples=start;
        midiOut.clear();
        ProcessData midi{};
        midi.processMode=kRealtime;
        midi.symbolicSampleSize=kSample32;
        midi.numSamples=frames;
        midi.numOutputs=1;midi.outputs=&genOut;
        midi.outputEvents=&midiOut;
        midi.inputParameterChanges=start==0?&params:nullptr;
        midi.processContext=&context;
        require(generator.processor->process(midi)==kResultOk,
                "real MIDI generator process failed");
        std::array<Event,32> blockEvents{};
        const int32 eventCount=midiOut.getEventCount();
        require(eventCount>=0 && eventCount<=32,"invalid MIDI event count");
        for(int32 i=0;i<eventCount;++i)
            require(midiOut.getEvent(i,blockEvents[static_cast<std::size_t>(i)])==kResultOk,
                    "cannot read generated event");
        int32 next=0;
        for(int32 i=0;i<frames;++i){
            while(next<eventCount && blockEvents[static_cast<std::size_t>(next)].sampleOffset==i){
                const auto& event=blockEvents[static_cast<std::size_t>(next++)];
                if(event.type==Event::kNoteOnEvent){
                    require(event.noteOn.pitch==60,"wrong generated MIDI pitch");
                    require(std::abs(event.noteOn.velocity-100.f/127.f)<.00001f,
                            "wrong generated MIDI velocity");
                    held=true;
                    amplitude=.4f*event.noteOn.velocity;
                    ++noteOns;
                } else if(event.type==Event::kNoteOffEvent){
                    require(event.noteOff.pitch==60,"wrong Note-Off pitch");
                    held=false;++noteOffs;
                }
            }
            if(!held)amplitude*=.9994f;
            const auto tone=static_cast<float>(std::sin(phase)*amplitude);
            audioL[static_cast<std::size_t>(i)]=tone;
            audioR[static_cast<std::size_t>(i)]=tone*.8f;
            phase+=kTau*261.6255653005986/static_cast<double>(kRate);
            if(phase>=kTau)phase-=kTau;
        }
        ProcessData rec{};
        rec.processMode=kRealtime;
        rec.symbolicSampleSize=kSample32;
        rec.numSamples=frames;
        rec.numInputs=1;rec.inputs=&source;
        rec.numOutputs=1;rec.outputs=&passthrough;
        rec.processContext=&context;
        require(recorder.processor->process(rec)==kResultOk,
                "real recorder process failed");
        for(int32 i=0;i<frames;++i)
            require(std::abs(passL[static_cast<std::size_t>(i)]-
                             audioL[static_cast<std::size_t>(i)])<1.e-7f,
                    "recorder must pass source audio through unchanged");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(noteOns==8 && noteOffs==8,
            "expected eight actual VST3 Note-On and Note-Off pairs");
    require(recorder.component->setActive(false)==kResultOk,
            "cannot flush recorder on deactivation");
    recorder.active=false;
    require(generator.component->setActive(false)==kResultOk,
            "cannot deactivate MIDI generator");
    generator.active=false;

    // Prove actual Note-Off recovery on transport stop, even if the host
    // has already moved the playhead to an unrelated position.
    require(generator.component->setActive(true)==kResultOk,
            "cannot reactivate MIDI generator for stop regression");
    generator.active=true;
    bool recovered=false;
    for(std::int64_t at=0;at<24000 && !recovered;at+=kBlock) {
        EventList outEvents(8);
        ProcessContext running{};
        running.state=ProcessContext::kPlaying;
        running.sampleRate=kRate;
        running.projectTimeSamples=at;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=kBlock;
        data.numOutputs=1;data.outputs=&genOut;
        data.outputEvents=&outEvents;
        data.processContext=&running;
        require(generator.processor->process(data)==kResultOk,
                "MIDI generator could not restart");
        for(int32 i=0;i<outEvents.getEventCount();++i) {
            Event on{};
            require(outEvents.getEvent(i,on)==kResultOk,"cannot read restart event");
            if(on.type!=Event::kNoteOnEvent)continue;
            require(on.noteOn.pitch==60,"restart pitch mismatch");
            EventList stoppedEvents(8);
            ParameterChanges stopValues(1);
            int32 index=0,point=0;
            auto* change=stopValues.addParameterData(1000,index);
            require(change && change->addPoint(0,0.,point)==kResultOk,
                    "cannot request STOP via plugin parameter");
            ProcessContext stopped{};
            stopped.sampleRate=kRate;
            stopped.projectTimeSamples=-12345; // intentionally unrelated playhead
            ProcessData offData{};
            offData.processMode=kRealtime;
            offData.symbolicSampleSize=kSample32;
            offData.numSamples=kBlock;
            offData.numOutputs=1;offData.outputs=&genOut;
            offData.outputEvents=&stoppedEvents;
            offData.inputParameterChanges=&stopValues;
            offData.processContext=&stopped;
            require(generator.processor->process(offData)==kResultOk,
                    "STOP callback failed");
            require(stoppedEvents.getEventCount()==1,
                    "STOP must send one real pending Note-Off");
            Event off{};
            require(stoppedEvents.getEvent(0,off)==kResultOk &&
                    off.type==Event::kNoteOffEvent &&
                    off.noteOff.pitch==60 && off.noteOff.noteId==on.noteOn.noteId,
                    "STOP note-off must match accepted Note-On by ID");
            recovered=true;
            break;
        }
    }
    require(recovered,"no restart Note-On / STOP note-off observed");
    require(generator.component->setActive(false)==kResultOk,
            "cannot deactivate generator after STOP test");
    generator.active=false;

    const auto output=base/"Documents"/"125A AutoSampler";
    require(std::filesystem::exists(output),"recorder created no run folder");
    std::size_t wavs=0,reports=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(output)){
        if(!entry.is_regular_file())continue;
        if(entry.path().extension()==".wav"){
            (void)nonzeroWav(entry.path());
            ++wavs;
        } else if(entry.path().filename()=="RUN-REPORT.csv"){
            std::ifstream in(entry.path());
            const std::string contents((std::istreambuf_iterator<char>(in)),{});
            require(contents.find("SUMMARY,exported,8")!=std::string::npos,
                    "run report missing 8 successful WAVs");
            require(contents.find("SUMMARY,rejected,0")!=std::string::npos,
                    "run report indicates rejected takes");
            require(contents.find("SUMMARY,droppedFrames,0")!=std::string::npos,
                    "audio frames were dropped");
            ++reports;
        }
    }
    require(wavs==8 && reports==1,
            "real recorder must export 8 distinct WAVs and one run report");
    std::cout<<"[PASS] REAL VST3 MIDI->synthetic synth->REAL VST3 Recorder: "
             <<noteOns<<" NoteOns, "<<noteOffs<<" NoteOffs, "<<wavs
             <<" 24-bit stereo WAVs; no lost frames\n";
    return 0;
}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"usage: autosampler_plugin_integration <path-to-bundle.vst3>");
        return run(argv[1]);
    }catch(const std::exception& e){
        std::cerr<<"[FAIL] P005 REAL VST3 integration: "<<e.what()<<"\n";
        return 1;
    }
}
