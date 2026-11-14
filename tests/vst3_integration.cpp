// 125A P005 native integration test, NOT part of the shipping plugin.
// Loads both actual VST3 classes, generates their MIDI events, supplies a
// synthetic oscillator as stand-in for a DAW-hosted instrument, and checks
// the exact WAV files written by the real recorder worker.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include "base/source/fobject.h"
#include "pluginterfaces/gui/iplugview.h"
#include "core/InstrumentArchive.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/memoryibstream.h"
#include "base/source/fstreamer.h"
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
    require(controller->getParameterCount()==18,
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
    std::cout<<"[PASS] REAL VSTGUI editor: 18 parameters, Win32 HWND attach, "
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
int run(const char* vst3path,bool matrix,bool customFolder){
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

    auto generator=load(factory,host,"125A AutoSampler");
    auto recorder=load(factory,host,"125A AutoSampler Recorder");
    smokeNativeGui(generator.component.get());
    const auto chosenRoot=base/"Chosen Export Folder"/"Takes";
    if(customFolder){
        std::filesystem::create_directories(chosenRoot);
        ResizableMemoryIBStream memory;
        IBStreamer io(&memory,kLittleEndian);
        require(io.writeInt32(0x125A1001),"P010 state version serialize");
        const std::array<double,11> oldParameters{
            60./127.,60./127.,0.,7./15.,0.,
            (1.-.25)/7.75,(2.-.25)/9.75,(.1-.05)/.45,
            99./126.,59./126.,119./126.
        };
        for(const auto value:oldParameters)
            require(io.writeDouble(value),"P010 parameter serialize");
        auto name=chosenRoot.u8string();
        require(io.writeInt32(static_cast<int32>(name.size())),
                "P010 folder length serialize");
        int32 bytes=0;
        require(memory.write(name.data(),static_cast<int32>(name.size()),&bytes)==kResultOk &&
                bytes==static_cast<int32>(name.size()),"P010 export directory serialize");
        memory.rewind();
        require(generator.component->setState(&memory)==kResultOk,
                "P010 cannot restore custom output folder");
        ResizableMemoryIBStream saved;
        require(generator.component->getState(&saved)==kResultOk,
                "P010 folder not saved into project state");
        saved.rewind();
        require(generator.component->setState(&saved)==kResultOk,
                "P010 export directory failed project save/reload");
    }
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
    ParameterChanges params(11);
    auto param=[&](ParamID id,double value){
        int32 queueIndex=0,pointIndex=0;
        auto* q=params.addParameterData(id,queueIndex);
        require(q && q->addPoint(0,value,pointIndex)==kResultOk,
                "cannot configure VST3 parameter");
    };
    param(1000,1.); // RUN
    param(1004,7./15.); // Explicitly stress 8 RRs; user default is 1.
    param(1006,(.5-.25)/7.75); // Short capture, including restored P010 projects.
    param(1007,(.5-.25)/9.75); // Short tail, not the historical 2 seconds.
    if(matrix){
        param(1003,0.); // Matrix needs adjacent semitone roots, not octave skips.
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
    const std::int64_t period=static_cast<std::int64_t>(kRate)+kRate/10;
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

    // P008: the real recorder must close the CSV and flush the last WAV
    // automatically. The DAW is still playing, and BOTH plugins remain
    // active. setActive(false) must not be required for a completed job.
    const auto autoRoot=customFolder?chosenRoot:base/"Documents"/"125A AutoSampler";
    bool finalizedWithoutDeactivation=false;
    for(int attempt=0;attempt<150 && !finalizedWithoutDeactivation;++attempt){
        if(std::filesystem::exists(autoRoot)){
            for(const auto& file:std::filesystem::recursive_directory_iterator(autoRoot)){
                if(!file.is_regular_file() || file.path().filename()!="RUN-REPORT.csv")
                    continue;
                std::ifstream reader(file.path(),std::ios::binary);
                const std::string text((std::istreambuf_iterator<char>(reader)),{});
                if(text.find("SUMMARY,exported,"+std::to_string(expectedTakes))!=std::string::npos &&
                   text.find("SUMMARY,droppedFrames,0")!=std::string::npos)
                    finalizedWithoutDeactivation=true;
            }
        }
        if(!finalizedWithoutDeactivation)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    require(finalizedWithoutDeactivation,
        "last WAV and RUN-REPORT must finish without stopping/deactivating the DAW");
    // Keep transport running past the end. No phantom notes or WAVs.
    {
        EventList afterEnd(8);
        ProcessContext playing{};
        playing.state=ProcessContext::kPlaying;
        playing.sampleRate=kRate;
        playing.projectTimeSamples=total+kBlock;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=kBlock;
        data.numOutputs=1;data.outputs=&genOut;
        data.outputEvents=&afterEnd;
        data.processContext=&playing;
        require(generator.processor->process(data)==kResultOk &&
                afterEnd.getEventCount()==0,
                "completed sampler must emit no phantom MIDI notes");
    }
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

    // P014: this test never calls setState() or opens an editor after
    // recording. The sample bank MUST load in the real VST3 on its own
    // background thread. GUI-timer-only import would fail this test.
    {
        require(generator.component->getBusCount(kEvent,kInput)==1,
                "125A internal sampler needs a MIDI input");
        (void)generator.component->activateBus(kEvent,kInput,0,true);
        require(generator.component->setActive(true)==kResultOk,
                "cannot reactivate 125A for headless auto-bank playback");
        generator.active=true;
        bool headlessAudible=false;
        for(int attempt=0;attempt<80 && !headlessAudible;++attempt){
            EventList liveMidi(2);
            Event note{};
            note.type=Event::kNoteOnEvent;
            note.sampleOffset=0;note.busIndex=0;
            note.noteOn.channel=0;
            note.noteOn.pitch=static_cast<int16>(matrix?61:60);
            note.noteOn.velocity=static_cast<float>(matrix?80:100)/127.f;
            note.noteOn.noteId=100+attempt;
            require(liveMidi.addEvent(note)==kResultOk,
                    "cannot send headless playback MIDI note");
            ProcessData render{};
            render.processMode=kRealtime;
            render.symbolicSampleSize=kSample32;
            render.numSamples=kBlock;
            render.numOutputs=1;render.outputs=&genOut;
            render.inputEvents=&liveMidi;
            require(generator.processor->process(render)==kResultOk,
                    "headless 125A render failed");
            int audible=0;
            for(int32 i=0;i<kBlock;++i){
                const auto idx=static_cast<std::size_t>(i);
                if(std::abs(genL[idx])>0.002f && std::abs(genR[idx])>0.001f)
                    ++audible;
            }
            headlessAudible=audible>kBlock/4;
            if(!headlessAudible)
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
        require(headlessAudible,
                "P014: recorded WAV bank not playable without GUI or project setState");

        // P016: native VST3 playback regression, not just a C++ core test.
        // Eight coherent MIDI notes used to drive the per-bank -1 dBFS
        // normalized voices into the hard output clipper.
        EventList stackedMidi(16);
        for(int v=0;v<8;++v) {
            Event e{};
            e.type=Event::kNoteOnEvent;
            e.busIndex=0;e.sampleOffset=0;
            e.noteOn.channel=0;
            e.noteOn.pitch=static_cast<int16>(matrix?61:60);
            e.noteOn.velocity=static_cast<float>(matrix?80:100)/127.f;
            e.noteOn.noteId=9000+v;
            require(stackedMidi.addEvent(e)==kResultOk,
                "P016 cannot submit 8 simultaneous sample notes");
        }
        EventList noMidi(1);
        float stackedPeak=0.f;
        for(int block=0;block<16;++block) {
            ProcessData render{};
            render.processMode=kRealtime;
            render.symbolicSampleSize=kSample32;
            render.numSamples=kBlock;
            render.numOutputs=1;render.outputs=&genOut;
            render.inputEvents=block==0?&stackedMidi:&noMidi;
            require(generator.processor->process(render)==kResultOk,
                "P016 multi-note playback VST3 callback failed");
            for(int32 i=0;i<kBlock;++i){
                const auto idx=static_cast<std::size_t>(i);
                require(std::isfinite(genL[idx])&&std::isfinite(genR[idx]),
                    "P016 output contains nonfinite PCM");
                stackedPeak=std::max(stackedPeak,
                    std::max(std::abs(genL[idx]),std::abs(genR[idx])));
            }
        }
        require(stackedPeak>.02f && stackedPeak<.9f,
            "P016 normalized eight-voice VST3 output clipped or was silent");
        std::cout<<"[PASS] P016 NATIVE VST3 NORMALIZE HEADROOM: 8 concurrent "
                    "voices without hard clipping, peak="<<stackedPeak<<"\\n";

        require(generator.component->setActive(false)==kResultOk,
                "cannot deactivate headless 125A player");
        generator.active=false;
        std::cout<<"[PASS] P014 HEADLESS AUTO BANK IMPORT: completed WAVs playable "
                    "from same VST3 without GUI or project state\n";
    }

    const auto output=customFolder?chosenRoot:base/"Documents"/"125A AutoSampler";
    require(std::filesystem::exists(output),"recorder created no run folder");
    std::size_t wavs=0,reports=0;
    std::set<std::string> expectedNames;
    std::filesystem::path capturedRunDirectory;
    std::filesystem::path capturedArchive;
    std::size_t archiveCount=0;
    for(int note=60;note<=(matrix?61:60);++note)
        for(const int vel:matrix?std::vector<int>{40,80,120}:std::vector<int>{100})
            for(int rr=1;rr<=8;++rr)
                expectedNames.insert("N"+std::to_string(note)+"_V"+std::to_string(vel)+
                    "_RR"+(rr<10?"0":"")+std::to_string(rr)+".wav");
    for(const auto& entry:std::filesystem::recursive_directory_iterator(output)){
        if(!entry.is_regular_file())continue;
        require(entry.path().extension()!=".sfz",
                "125A is the sampler: automatic third-party SFZ export is unnecessary");
        if(entry.path().extension()==".wav"){
            (void)nonzeroWav(entry.path());
            require(expectedNames.erase(entry.path().filename().string())==1,
                "wrong, duplicated or unexpected note/velocity/RR WAV filename");
            ++wavs;
            capturedRunDirectory=entry.path().parent_path();
        } else if(entry.path().extension()==".125A"){
            capturedArchive=entry.path();
            ++archiveCount;
            std::string instrumentName;
            auto bank=a125::load125AArchive(capturedArchive,&instrumentName);
            require(bank && bank->zones.size()==static_cast<std::size_t>(expectedTakes) &&
                    instrumentName=="Untitled Instrument",
                "P017 captured .125A is not a playable, self-contained ZIP");
        } else if(entry.path().filename()=="RUN-REPORT.csv"){
            std::ifstream in(entry.path());
            const std::string report((std::istreambuf_iterator<char>(in)),{});
            require(report.find("SUMMARY,exported,"+
                std::to_string(expectedTakes))!=std::string::npos,
                    "run report does not match WAV count");
            require(report.find("SUMMARY,rejected,0")!=std::string::npos &&
                    report.find("SUMMARY,droppedFrames,0")!=std::string::npos &&
                    report.find("BANK,125A_WAVS_READY")!=std::string::npos,
                    "native bank readiness not confirmed by recorder");
            require(report.find("ARCHIVE,125A_READY")!=std::string::npos,
                "P017 recorder did not persist a complete .125A archive");
            ++reports;
        }
    }
    require(static_cast<int>(wavs)==expectedTakes && expectedNames.empty() &&
            reports==1 && archiveCount==1 && !capturedArchive.empty() &&
            !capturedRunDirectory.empty(),
            "native 125A bank must contain every requested sample WAV");
    if(customFolder){
        require(!std::filesystem::exists(base/"Documents"/"125A AutoSampler"),
                "P010 leaked custom WAVs into default Documents directory");
    }
    // P013: the SAME 125A instrument must directly play the just-recorded
    // WAV bank. No SFZ file, second sampler, or external playback VST.
    {
        ResizableMemoryIBStream bankState;
        IBStreamer io(&bankState,kLittleEndian);
        require(io.writeInt32(0x125A1301),"native player state version");
        const std::array<double,11> params{
            60./127.,60./127.,0.,7./15.,0.,
            (1.-.25)/7.75,(2.-.25)/9.75,(.1-.05)/.45,
            99./126.,59./126.,119./126.
        };
        for(double value:params)require(io.writeDouble(value),
            "cannot serialize native sampler parameters");
        require(io.writeInt32(0),"native bank requires no external export path");
        // The DAW state must restore a portable .125A archive, not
        // an obsolete absolute directory full of loose recordings.
        auto folder=capturedArchive.u8string();
        require(io.writeInt32(static_cast<int32>(folder.size())),
            "cannot serialize captured WAV bank folder length");
        int32 actual=0;
        require(bankState.write(folder.data(),static_cast<int32>(folder.size()),
                   &actual)==kResultOk && actual==static_cast<int32>(folder.size()),
            "cannot serialize captured WAV bank");
        bankState.rewind();
        require(generator.component->setState(&bankState)==kResultOk,
            "P017 125A plugin failed to load its own recorded ZIP instrument");
        ResizableMemoryIBStream saved;
        require(generator.component->getState(&saved)==kResultOk,
            "125A cannot save playable bank in project");
        saved.rewind();
        require(generator.component->setState(&saved)==kResultOk,
            "125A cannot recall recorded bank after project save/load");
        require(generator.component->getBusCount(kEvent,kInput)==1,
            "125A must accept live MIDI for native sample playback");
        (void)generator.component->activateBus(kEvent,kInput,0,true);
        require(generator.component->setActive(true)==kResultOk,
            "cannot activate internal 125A player");
        generator.active=true;
        EventList incoming(8);
        Event keyOn{};
        keyOn.type=Event::kNoteOnEvent;
        keyOn.sampleOffset=0;keyOn.busIndex=0;
        keyOn.noteOn.channel=0;
        keyOn.noteOn.pitch=static_cast<int16>(matrix?61:60);
        keyOn.noteOn.velocity=static_cast<float>(matrix?80:100)/127.f;
        keyOn.noteOn.noteId=99;
        require(incoming.addEvent(keyOn)==kResultOk,
            "could not send playable MIDI Note-On into 125A");
        EventList outputMidi(8);
        ProcessData render{};
        render.processMode=kRealtime;
        render.symbolicSampleSize=kSample32;
        render.numSamples=kBlock;
        render.numOutputs=1;render.outputs=&genOut;
        render.inputEvents=&incoming;render.outputEvents=&outputMidi;
        require(generator.processor->process(render)==kResultOk,
            "125A native VST3 player process failed");
        std::size_t audible=0;
        for(int32 i=0;i<kBlock;++i)
            if(std::abs(genL[static_cast<std::size_t>(i)])>0.002f &&
               std::abs(genR[static_cast<std::size_t>(i)])>0.001f)++audible;
        require(audible>static_cast<std::size_t>(kBlock/4),
            "native 125A player rendered silence instead of recorded instrument");
        require(outputMidi.getEventCount()==0,
            "native playback mode should not emit a new sampling sequence");
        require(generator.component->setActive(false)==kResultOk,
            "native player could not deactivate after audio output");
        generator.active=false;
        std::cout<<"[PASS] P017 PORTABLE .125A: ZIP archive auto-export, "
                     "native load and DAW state restore\n";
        std::cout<<"[PASS] P013 BUILT-IN 125A VST3 PLAYER: "
                 <<audible<<" audible PCM frames from real captured WAV bank; "
                 <<"MIDI note / velocity / project restore\n";
    }
    std::cout<<"[PASS] REAL VST3 MIDI->synthetic synth->REAL VST3 Recorder: "
             <<noteOns<<" NoteOns, "<<noteOffs<<" NoteOffs, "<<wavs
             <<" 24-bit stereo WAVs at "<<kRate<<" Hz, block "<<kBlock
             <<"; no lost frames\n";
    return 0;
}

int runAbort(const char* vst3path){
    // Actual VST3 processors stay active during the entire abort test.
    const auto base=std::filesystem::absolute("p009-abort-vst3-output");
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base/"Documents");
    require(_putenv_s("USERPROFILE",base.string().c_str())==0,
            "cannot set isolated P009 recorder output");
    std::string error;
    auto module=Module::create(vst3path,error);
    require(bool(module),"cannot load P009 VST3 bundle");
    HostApplication host;
    auto factory=module->getFactory();
    factory.setHostContext(&host);
    auto generator=load(factory,host,"125A AutoSampler");
    auto recorder=load(factory,host,"125A AutoSampler Recorder");
    (void)generator.component->activateBus(kEvent,kOutput,0,true);

    std::vector<Sample32> midiLeft(kBlock),midiRight(kBlock),
        sourceLeft(kBlock),sourceRight(kBlock),outLeft(kBlock),outRight(kBlock);
    std::array<Sample32*,2> midiChannels{midiLeft.data(),midiRight.data()};
    std::array<Sample32*,2> srcChannels{sourceLeft.data(),sourceRight.data()};
    std::array<Sample32*,2> outChannels{outLeft.data(),outRight.data()};
    AudioBusBuffers midiOutput{},source{},passthrough{};
    midiOutput.numChannels=2;midiOutput.channelBuffers32=midiChannels.data();
    source.numChannels=2;source.channelBuffers32=srcChannels.data();
    passthrough.numChannels=2;passthrough.channelBuffers32=outChannels.data();

    const auto playBlock=[&](std::int64_t at,ParameterChanges* changes,
                             bool& sawOn,bool& sawOff){
        EventList events(8);
        ProcessContext context{};
        context.sampleRate=kRate;
        context.projectTimeSamples=at;
        context.state=ProcessContext::kPlaying;
        ProcessData midi{};
        midi.processMode=kRealtime;midi.symbolicSampleSize=kSample32;
        midi.numSamples=kBlock;midi.numOutputs=1;midi.outputs=&midiOutput;
        midi.outputEvents=&events;midi.processContext=&context;
        midi.inputParameterChanges=changes;
        require(generator.processor->process(midi)==kResultOk,
                "P009 generator process failed");
        for(int32 i=0;i<events.getEventCount();++i){
            Event e{};
            require(events.getEvent(i,e)==kResultOk,"P009 MIDI event unavailable");
            if(e.type==Event::kNoteOnEvent) sawOn=true;
            if(e.type==Event::kNoteOffEvent) sawOff=true;
        }
        for(int i=0;i<kBlock;++i) {
            const auto sound=sawOn && !sawOff ? .2f : 0.f;
            sourceLeft[static_cast<std::size_t>(i)]=sound;
            sourceRight[static_cast<std::size_t>(i)]=sound*.5f;
        }
        ProcessData rec{};
        rec.processMode=kRealtime;rec.symbolicSampleSize=kSample32;
        rec.numSamples=kBlock;rec.numInputs=1;rec.inputs=&source;
        rec.numOutputs=1;rec.outputs=&passthrough;
        rec.processContext=&context;
        require(recorder.processor->process(rec)==kResultOk,
                "P009 recorder process failed");
    };

    ParameterChanges runOn(1);
    int32 qi=0,pi=0;
    auto* on=runOn.addParameterData(1000,qi);
    require(on && on->addPoint(0,1.,pi)==kResultOk,"P009 Run ON unavailable");
    bool sawOn=false,sawOff=false;
    std::int64_t at=0;
    // Begin with the half-second lead. Stop while the FIRST note is still
    // held, well before its 1s NoteOff and 2s release window.
    for(;at<static_cast<std::int64_t>(kRate)*3/4;at+=kBlock){
        playBlock(at,at==0?&runOn:nullptr,sawOn,sawOff);
        if((at/kBlock)%8==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(sawOn && !sawOff,
            "P009 must abort an actively sounding note before NoteOff");
    ParameterChanges runOff(1);
    qi=0;pi=0;
    auto* off=runOff.addParameterData(1000,qi);
    require(off && off->addPoint(0,0.,pi)==kResultOk,"P009 Run OFF unavailable");
    playBlock(at,&runOff,sawOn,sawOff);
    require(sawOff,"P009 STOP must send a NoteOff for the pending sound");

    bool closed=false;
    bool wavFound=false;
    const auto root=base/"Documents"/"125A AutoSampler";
    for(int attempt=0;attempt<150 && !closed;++attempt){
        if(std::filesystem::exists(root)){
            for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
                if(!entry.is_regular_file())continue;
                if(entry.path().extension()==".wav")wavFound=true;
                require(entry.path().extension()!=".sfz",
                        "P011 must not publish a playable bank after STOP");
                if(entry.path().filename()=="RUN-REPORT.csv"){
                    std::ifstream reader(entry.path(),std::ios::binary);
                    const std::string report((std::istreambuf_iterator<char>(reader)),{});
                    if(report.find("SUMMARY,exported,0")!=std::string::npos &&
                       report.find("SUMMARY,rejected,1")!=std::string::npos &&
                       report.find("BANK,SKIPPED_INCOMPLETE")!=std::string::npos &&
                       report.find("ABORTED")!=std::string::npos)
                        closed=true;
                }
            }
        }
        if(!closed)std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    require(!wavFound && closed,
            "mid-take STOP must close report with ABORTED and no partial WAV while active");
    // Leave the DAW playing after STOP. No automatic restart or phantom MIDI.
    for(int n=0;n<12;++n){
        const bool oldOn=sawOn;
        playBlock(at+(n+1)*kBlock,nullptr,sawOn,sawOff);
        require(sawOn==oldOn && sawOff,"P009 unexpected MIDI after Stop");
    }
    // Both components are still active at the moment the CSV was verified.
    require(recorder.component->setActive(false)==kResultOk,
            "P009 recorder deactivation failed");
    recorder.active=false;
    require(generator.component->setActive(false)==kResultOk,
            "P009 generator deactivation failed");
    generator.active=false;
    std::cout<<"[PASS] REAL VST3 mid-take STOP: pending NoteOff, "
                "ABORTED CSV without plugin unload, 0 partial WAVs\n";
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
        const std::string mode=argc>=5?argv[4]:"";
        if(mode=="abort") return runAbort(argv[1]);
        require(mode.empty() || mode=="matrix" || mode=="custom",
                "invalid integration test mode");
        return run(argv[1],mode=="matrix",mode=="custom");
    }catch(const std::exception& e){
        std::cerr<<"[FAIL] P005 REAL VST3 integration: "<<e.what()<<"\n";
        return 1;
    }
}
