// 125A P005 native integration test, NOT part of the shipping plugin.
// Loads both actual VST3 classes, generates their MIDI events, supplies a
// synthetic oscillator as stand-in for a DAW-hosted instrument, and checks
// the exact WAV files written by the real recorder worker.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include "base/source/fobject.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
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
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VST3::Hosting;
namespace {
int kRate=44100;
int kBlock=256;
constexpr double kTau=6.28318530717958647692;
void require(bool value,const char* what) {
    if(!value)throw std::runtime_error(what);
}
class TestHostFrame final : public FObject, public IPlugFrame {
public:
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* desired) override {
        if(!view || !desired || desired->getWidth()<=0 ||
           desired->getHeight()<=0) return kInvalidArgument;
        return view->onSize(desired);
    }
    OBJ_METHODS(TestHostFrame,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IPlugFrame)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};
LRESULT CALLBACK testWindowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    return DefWindowProcW(hwnd,msg,w,l);
}
void smokeNativeGui(IComponent* component){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    require(SUCCEEDED(com),"cannot initialize Win32 COM STA for VSTGUI");
    IEditController* controller=nullptr;
    require(component->queryInterface(IEditController::iid,
              reinterpret_cast<void**>(&controller))==kResultOk && controller,
            "MIDI VST3 combined edit controller not available");
    require(controller->getParameterCount()==12,
            "native MIDI editor must expose 12 VST3 parameters");
    IPlugView* view=controller->createView(ViewType::kEditor);
    require(view,"MIDI VST3 supplies no real editor");
    ViewRect rect{};
    require(view->getSize(&rect)==kResultTrue &&
            rect.getWidth()>=600 && rect.getHeight()>=400,
            "native GUI has invalid editor geometry");
    require(view->isPlatformTypeSupported(kPlatformTypeHWND)==kResultTrue,
            "native GUI does not support HWND");
    WNDCLASSW wc{};
    wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpfnWndProc=testWindowProc;
    wc.lpszClassName=L"125A_AutoSampler_P005_GUI";
    require(RegisterClassW(&wc) ||
            GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"register GUI host class");
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"125A AutoSampler Test",
         WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
         rect.getWidth()+32,rect.getHeight()+64,
         nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(hwnd!=nullptr,"cannot create actual Win32 GUI parent");
    auto* frame=new TestHostFrame();
    require(view->setFrame(frame)==kResultTrue,"cannot set VST3 editor host frame");
    require(view->attached(hwnd,kPlatformTypeHWND)==kResultTrue,
            "actual VSTGUI editor refused HWND attach");
    ShowWindow(hwnd,SW_SHOWNA);
    UpdateWindow(hwnd);
    // Exercise the 250ms progress-status timer on a real Win32 UI thread.
    const auto end=GetTickCount64()+350;
    MSG msg{};
    do{
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        Sleep(2);
    }while(GetTickCount64()<end);
    require(GetWindow(hwnd,GW_CHILD)!=nullptr,
            "VSTGUI failed to create its own editor child window");
    require(view->removed()==kResultTrue,"VSTGUI could not detach cleanly");
    require(view->setFrame(nullptr)==kResultTrue,"VSTGUI host frame release failed");
    frame->release();
    view->release();
    controller->release();
    DestroyWindow(hwnd);
    CoUninitialize();
    std::cout<<"[PASS] REAL VSTGUI editor: 12 parameters, Win32 HWND attach, "
                "live timer and clean detach\\n";
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
int run(const char* vst3path,bool matrix){
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
    smokeNativeGui(generator.component.get());
    require(generator.component->getBusCount(kEvent,kOutput)==1,
            "MIDI generator must expose an event output bus");
    require(recorder.component->getBusCount(kAudio,kInput)==1 &&
            recorder.component->getBusCount(kAudio,kOutput)==1,
            "recorder must expose stereo input/output");
    (void)generator.component->activateBus(kEvent,kOutput,0,true);

    std::vector<Sample32> genL(static_cast<std::size_t>(kBlock)),genR(static_cast<std::size_t>(kBlock)),
        audioL(static_cast<std::size_t>(kBlock)),audioR(static_cast<std::size_t>(kBlock)),
        passL(static_cast<std::size_t>(kBlock)),passR(static_cast<std::size_t>(kBlock));
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
    ParameterChanges params(7);
    auto param=[&](ParamID id,double value){
        int32 queueIndex=0,pointIndex=0;
        auto* q=params.addParameterData(id,queueIndex);
        require(q && q->addPoint(0,value,pointIndex)==kResultOk,
                "cannot configure VST3 parameter");
    };
    param(1000,1.); // RUN
    if(matrix){
        param(1001,60./127.);
        param(1002,61./127.);
        param(1005,1.); // Three velocities
        param(1009,39./126.); // 40
        param(1010,79./126.); // 80
        param(1011,119./126.); // 120
    }
    double phase=0.;
    float amplitude=0.f;
    bool held=false;
    int noteOns=0,noteOffs=0;
    std::set<int32> noteIDs;
    const int expectedTakes=matrix?48:8;
    const std::int64_t period=static_cast<std::int64_t>(kRate)*3+kRate/10;
    const std::int64_t total=kRate/2+static_cast<std::int64_t>(expectedTakes)*period+kBlock;
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
                    const auto pitch=event.noteOn.pitch;
                    require(pitch==60 || (matrix && pitch==61),"wrong generated MIDI pitch");
                    const auto velocity=static_cast<int>(std::lround(event.noteOn.velocity*127.));
                    require(matrix?(velocity==40 || velocity==80 || velocity==120):
                        velocity==100,"wrong generated MIDI velocity");
                    require(noteIDs.insert(event.noteOn.noteId).second,
                        "repeated MIDI noteID across RR/velocity/note");
                    held=true;
                    amplitude=.4f*event.noteOn.velocity;
                    ++noteOns;
                } else if(event.type==Event::kNoteOffEvent){
                    require(event.noteOff.pitch==60 ||
                            (matrix && event.noteOff.pitch==61),"wrong Note-Off pitch");
                    require(noteIDs.count(event.noteOff.noteId)==1,
                        "Note-Off must reference previously emitted Note-On");
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
        // Throttle periodic batches instead of sleeping on every 64-frame
        // block (Windows default timer granularity can turn Sleep(1) into
        // 15ms). The recorder still runs concurrently; any overrun fails QA.
        if ((start / kBlock) % 8 == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        else
            std::this_thread::yield();
    }
    require(noteOns==expectedTakes && noteOffs==expectedTakes &&
            static_cast<int>(noteIDs.size())==expectedTakes,
            "actual VST3 MIDI notes differ from configured note/velocity/RR matrix");
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
    // Regression: a DAW transport stop and plugin reactivation must NOT
    // silently restart sampling when Run is still ON from the previous job.
    auto callGenerator=[&](std::int64_t at,ParameterChanges* changes){
        EventList events(8);
        ProcessContext context{};
        context.state=ProcessContext::kPlaying;
        context.sampleRate=kRate;
        context.projectTimeSamples=at;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=kBlock;
        data.numOutputs=1;data.outputs=&genOut;
        data.outputEvents=&events;
        data.inputParameterChanges=changes;
        data.processContext=&context;
        require(generator.processor->process(data)==kResultOk,
                "MIDI generator rearm callback failed");
        require(events.getEventCount()==0,
                "unarmed generator must not emit unexpected MIDI events");
    };
    callGenerator(0,nullptr); // no explicit restart: zero events
    ParameterChanges rearmOff(1);
    int32 offQueueIndex=0,offPointIndex=0;
    auto* rearmOffQueue=rearmOff.addParameterData(1000,offQueueIndex);
    require(rearmOffQueue && rearmOffQueue->addPoint(0,0.,offPointIndex)==kResultOk,
            "cannot rearm Run OFF");
    callGenerator(kBlock,&rearmOff);
    ParameterChanges rearmOn(1);
    int32 onQueueIndex=0,onPointIndex=0;
    auto* rearmOnQueue=rearmOn.addParameterData(1000,onQueueIndex);
    require(rearmOnQueue && rearmOnQueue->addPoint(0,1.,onPointIndex)==kResultOk,
            "cannot restart with explicit Run ON");
    callGenerator(2*kBlock,&rearmOn);
    bool recovered=false;
    for(std::int64_t at=3*kBlock;at<kRate+3*kBlock && !recovered;at+=kBlock) {
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
    std::set<std::string> expectedNames;
    for(int note=60;note<=(matrix?61:60);++note)
        for(const int vel:matrix?std::vector<int>{40,80,120}:std::vector<int>{100})
            for(int rr=1;rr<=8;++rr)
                expectedNames.insert("N"+std::to_string(note)+"_V"+std::to_string(vel)+
                    "_RR"+(rr<10?"0":"")+std::to_string(rr)+".wav");
    for(const auto& entry:std::filesystem::recursive_directory_iterator(output)){
        if(!entry.is_regular_file())continue;
        if(entry.path().extension()==".wav"){
            (void)nonzeroWav(entry.path());
            require(expectedNames.erase(entry.path().filename().string())==1,
                "wrong, duplicated or unexpected MIDI note/velocity/RR WAV filename");
            ++wavs;
        } else if(entry.path().filename()=="RUN-REPORT.csv"){
            std::ifstream in(entry.path());
            const std::string contents((std::istreambuf_iterator<char>(in)),{});
            require(contents.find("SUMMARY,exported,"+
                std::to_string(expectedTakes))!=std::string::npos,
                    "run report does not match expected successful WAV count");
            require(contents.find("SUMMARY,rejected,0")!=std::string::npos,
                    "run report indicates rejected takes");
            require(contents.find("SUMMARY,droppedFrames,0")!=std::string::npos,
                    "audio frames were dropped");
            ++reports;
        }
    }
    require(static_cast<int>(wavs)==expectedTakes && expectedNames.empty() &&
            reports==1,"real recorder failed full note/velocity/RR WAV matrix");
    std::cout<<"[PASS] REAL VST3 MIDI->synthetic synth->REAL VST3 Recorder: "
             <<noteOns<<" NoteOns, "<<noteOffs<<" NoteOffs, "<<wavs
             <<" 24-bit stereo WAVs at "<<kRate<<" Hz, block "<<kBlock
             <<"; no lost frames\n";
    return 0;
}
}
int main(int argc,char** argv){
    try{
        require(argc>=2 && argc<=5,
            "usage: AutosamplerIntegration <bundle.vst3> [sampleRate] [blockSize] [matrix]");
        if(argc>=3)kRate=std::stoi(argv[2]);
        if(argc>=4)kBlock=std::stoi(argv[3]);
        require((kRate==44100 || kRate==48000 || kRate==96000) &&
                (kBlock==64 || kBlock==256 || kBlock==512 || kBlock==1024),
                "invalid sample rate or block size");
        const bool matrix=argc>=5 && std::string(argv[4])=="matrix";
        return run(argv[1],matrix);
    }catch(const std::exception& e){
        std::cerr<<"[FAIL] P005 REAL VST3 integration: "<<e.what()<<"\n";
        return 1;
    }
}
