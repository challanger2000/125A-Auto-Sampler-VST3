// 125A Auto Sampler P005: DAW-hosted VST3 MIDI generator and audio recorder.
// A single VST3 bundle exports both components. No third-party VST host.
#include "core/JobSchedule.h"
#include "core/JobParameters.h"
#include "core/CaptureIntegrity.h"
#include "core/ActiveNote.h"
#include "core/RunGate.h"
#include "core/ExportTarget.h"
#include "core/SampleProcessing.h"
#include "core/RunStatus.h"
#include "core/BuiltInSampler.h"
#include "core/InstrumentArchive.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>
#endif
#include <string>
#include <thread>
#include <vector>

namespace {
namespace V = Steinberg::Vst;
using Steinberg::FUnknown;
using Steinberg::tresult;
using Steinberg::int32;
using Steinberg::TBool;
const Steinberg::FUID kMidiID(0xA181019A,0x3DC9472C,0xB69A0F73,0xFA25C101);
const Steinberg::FUID kRecorderID(0xA181019A,0x3DC9472C,0xB69A0F73,0xFA25C102);
constexpr std::int64_t kDisarmed = std::numeric_limits<std::int64_t>::min();

enum : V::ParamID {
    kRunParam=1000, kFirstParam, kLastParam, kStepParam, kRRParam,
    kLayersParam, kHoldParam, kTailParam, kGapParam,
    kVelocity1Param, kVelocity2Param, kVelocity3Param,
    kNormalizeParam, kGateParam, kAttackParam, kDecayParam,
    kSustainParam, kAmpReleaseParam, kGainParam
};
constexpr std::size_t kParamCount=a125::ParameterCount+7;
constexpr std::array<double,kParamCount> kDefaultParams{
    0.,60./127.,60./127.,1.,0.,0.,
    (1.-.25)/7.75,(2.-.25)/9.75,(.1-.05)/.45,
    99./126.,59./126.,119./126., 0.,0.,0.,0.,1.,.15/3.,.5
};
constexpr double kDefaultHold=(1.-0.25)/7.75;
constexpr double kDefaultTail=(2.-0.25)/9.75;
constexpr double kDefaultGap=(0.10-0.05)/0.45;


// Only one generator/recorder pair per DAW process is supported in P003.
// Job fields are published before the release-store to anchor. No mutex or
// heap allocation occurs in process(); the recorder acquires anchor first.
struct Session {
    std::atomic<std::int64_t> anchor{kDisarmed};
    std::atomic<std::uint32_t> generation{0};
    std::atomic<int> sampleRate{44100};
    // Set after the generator's last release has reached the recorder.
    std::atomic<std::uint32_t> captureEndedGeneration{0};
    // Explicit user/host stop before the last sample is an interrupted run.
    std::atomic<std::uint32_t> interruptedGeneration{0};
    // Set by the file writer only after closing RUN-REPORT.csv.
    std::atomic<std::uint32_t> writerFinishedGeneration{0};
    // Observable per-run counts, updated atomically by the recorder.
    std::atomic<std::uint32_t> observed{0},written{0},rejected{0},silent{0},errors{0};
    std::atomic<std::uint64_t> dropped{0};
    a125::ExportTarget exportTarget;
    a125::ExportTarget latestCompletedRun;
    std::atomic<std::uint32_t> readyBankGeneration{0};
    // Controlled GUI/state/recorder threads only; never from process().
    std::string captureName() const {
        std::lock_guard<std::mutex> lock(nameMutex);
        return name;
    }
    bool setCaptureName(const std::string& display) {
        if(display.empty() || display.size()>192)return false;
        std::lock_guard<std::mutex> lock(nameMutex);
        name=display;
        return true;
    }
    mutable std::mutex nameMutex;
    std::string name="Untitled Instrument";
    std::array<std::atomic<double>,7> capturedPlayback{0.,0.,0.,0.,1.,.05,.5};
    void setCapturedPlayback(const a125::ArchivePlayback& value) noexcept {
        for(std::size_t i=0;i<value.params.size();++i)
            capturedPlayback[i].store(value.params[i],std::memory_order_relaxed);
    }
    a125::ArchivePlayback readCapturedPlayback() const noexcept {
        a125::ArchivePlayback value;
        for(std::size_t i=0;i<value.params.size();++i)
            value.params[i]=capturedPlayback[i].load(std::memory_order_relaxed);
        return value;
    }
    std::atomic<int> firstNote{60},lastNote{60},step{12},roundRobins{1},layers{1};
    std::atomic<int> velocity0{100},velocity1{80},velocity2{120};
    std::atomic<std::uint64_t> hold{22050},tail{22050},gap{4410};

    void publish(const a125::JobConfig& config,int sr,std::int64_t newAnchor) noexcept {
        anchor.store(kDisarmed,std::memory_order_release);
        observed.store(0,std::memory_order_relaxed);
        written.store(0,std::memory_order_relaxed);
        rejected.store(0,std::memory_order_relaxed);
        silent.store(0,std::memory_order_relaxed);
        errors.store(0,std::memory_order_relaxed);
        dropped.store(0,std::memory_order_relaxed);
        sampleRate.store(sr,std::memory_order_relaxed);
        firstNote.store(config.firstNote,std::memory_order_relaxed);
        lastNote.store(config.lastNote,std::memory_order_relaxed);
        step.store(config.semitoneStep,std::memory_order_relaxed);
        roundRobins.store(config.roundRobins,std::memory_order_relaxed);
        layers.store(config.velocityLayers,std::memory_order_relaxed);
        velocity0.store(config.velocities[0],std::memory_order_relaxed);
        velocity1.store(config.velocities[1],std::memory_order_relaxed);
        velocity2.store(config.velocities[2],std::memory_order_relaxed);
        hold.store(config.holdFrames,std::memory_order_relaxed);
        tail.store(config.tailFrames,std::memory_order_relaxed);
        gap.store(config.gapFrames,std::memory_order_relaxed);
        generation.fetch_add(1,std::memory_order_release);
        anchor.store(newAnchor,std::memory_order_release);
    }
    a125::JobConfig read() const noexcept {
        a125::JobConfig c;
        c.firstNote=firstNote.load(std::memory_order_relaxed);
        c.lastNote=lastNote.load(std::memory_order_relaxed);
        c.semitoneStep=step.load(std::memory_order_relaxed);
        c.roundRobins=roundRobins.load(std::memory_order_relaxed);
        c.velocityLayers=layers.load(std::memory_order_relaxed);
        c.velocities={velocity0.load(std::memory_order_relaxed),
             velocity1.load(std::memory_order_relaxed),
             velocity2.load(std::memory_order_relaxed)};
        c.holdFrames=hold.load(std::memory_order_relaxed);
        c.tailFrames=tail.load(std::memory_order_relaxed);
        c.gapFrames=gap.load(std::memory_order_relaxed);
        return c;
    }
};
Session& session() noexcept { static Session state; return state; }

void copyAudio(const V::ProcessData& data) noexcept {
    if(data.numInputs<1 || data.numOutputs<1 || !data.inputs || !data.outputs ||
       data.symbolicSampleSize!=V::kSample32) return;
    const auto& in=data.inputs[0];
    auto& out=data.outputs[0];
    out.silenceFlags=in.silenceFlags;
    if(!in.channelBuffers32 || !out.channelBuffers32) return;
    for(int32 ch=0;ch<std::min(in.numChannels,out.numChannels);++ch) {
        auto* src=in.channelBuffers32[ch];
        auto* dst=out.channelBuffers32[ch];
        if(src && dst && src!=dst && data.numSamples>0)
            std::memcpy(dst,src,static_cast<std::size_t>(data.numSamples)*sizeof(float));
    }
}

// Windows native folder picker runs only in the GUI callback, never in process().
std::optional<std::filesystem::path> pickOutputDirectory() {
#ifdef _WIN32
    const HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    const bool uninitialize=SUCCEEDED(init);
    IFileOpenDialog* dialog=nullptr;
    std::optional<std::filesystem::path> chosen;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&dialog))) && dialog){
        DWORD flags=0;
        if(SUCCEEDED(dialog->GetOptions(&flags))) {
            dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
            dialog->SetTitle(L"125A AutoSampler - Export Folder");
            if(dialog->Show(nullptr)==S_OK){
                IShellItem* item=nullptr;
                if(SUCCEEDED(dialog->GetResult(&item)) && item){
                    PWSTR name=nullptr;
                    if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&name)) && name){
                        chosen=std::filesystem::path(name);
                        CoTaskMemFree(name);
                    }
                    item->Release();
                }
            }
        }
        dialog->Release();
    }
    if(uninitialize)CoUninitialize();
    return chosen;
#else
    return std::nullopt;
#endif
}

class MidiGenerator final : public V::SingleComponentEffect,
                            public VSTGUI::VST3EditorDelegate,
                            public VSTGUI::IControlListener {
public:
    static FUnknown* createInstance(void*) {return (V::IAudioProcessor*)new MidiGenerator();}
    ~MidiGenerator() override {stopBankMonitor();}
    tresult PLUGIN_API terminate() override {
        // The Steinberg validator calls terminate()/initialize() on the SAME
        // object. A joinable std::thread cannot be overwritten: join it
        // before reinitialization or MSVC fail-fasts during VST3 validation.
        stopBankMonitor();
        return SingleComponentEffect::terminate();
    }
    void stopBankMonitor() noexcept {
        stopBankWorker_.store(true,std::memory_order_release);
        if(bankWorker_.joinable())bankWorker_.join();
    }
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override {
        if(Steinberg::FIDStringsEqual(name,V::ViewType::kEditor)){
            auto* editor=new VSTGUI::VST3Editor(this,"view","autosampler.uidesc");
            editor->setDelegate(this);
            return editor;
        }
        return nullptr;
    }
    VSTGUI::CView* verifyView(VSTGUI::CView* view,
            const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,
            VSTGUI::VST3Editor*) override {
        if(auto* label=dynamic_cast<VSTGUI::CTextLabel*>(view)){
            if(label->getText()=="STATUS: READY")statusLabel_=label;
            if(label->getText()=="OUTPUT: DEFAULT")folderLabel_=label;
            if(label->getText()=="SAMPLER: EMPTY")samplerLabel_=label;
        }
        if(auto* button=dynamic_cast<VSTGUI::CTextButton*>(view)){
            if(button->getTitle()=="CHOOSE FOLDER"){
                folderButton_=button;
                folderButton_->registerControlListener(this);
            }else if(button->getTitle()=="DEFAULT"){
                defaultButton_=button;
                defaultButton_->registerControlListener(this);
            }else if(button->getTitle()=="LOAD SAMPLES"){
                loadButton_=button;
                loadButton_->registerControlListener(this);
            }
        }
        return view;
    }
    void valueChanged(VSTGUI::CControl* control) override {
        if(control==loadButton_){
            const auto chosen=pickOutputDirectory();
            if(chosen) {
                loadSamples(*chosen);
                updateSamplerLabel();
            }
            return;
        }
        if(control==defaultButton_){
            session().exportTarget.choose({});
            updateFolderLabel();
            return;
        }
        if(control!=folderButton_)return;
        const auto chosen=pickOutputDirectory();
        if(chosen && chosen->is_absolute()){
            session().exportTarget.choose(*chosen);
            updateFolderLabel();
        }
    }
    void updateFolderLabel(){
        if(!folderLabel_)return;
        const auto folder=session().exportTarget.chosenFolder();
        if(folder.empty()){
            folderLabel_->setText("OUTPUT: DEFAULT");
        }else{
            const auto utf8=folder.u8string();
            std::string caption("OUTPUT: ");
            caption.append(reinterpret_cast<const char*>(utf8.data()),utf8.size());
            folderLabel_->setText(caption.c_str());
        }
    }
    void loadSamples(const std::filesystem::path& path,
                     bool applyInstrumentSettings=false){
        // Worker/UI/state threads can all request a bank, but never ASIO.
        // Loaded PCM stays alive so a voice can safely finish during a swap.
        bool loaded=false;
        try {
            a125::ArchivePlayback archivePlayback;
            std::string archiveName;
            const bool archive=path.extension()==".125A";
            auto bank=archive?
                a125::load125AArchive(path,&archiveName,&archivePlayback):
                a125::loadCapturedBank(path);
            if(bank){
                std::lock_guard<std::mutex> guard(bankMutex_);
                if(player_.publish(std::move(bank))){
                    bankFolder_=path;
                    loadFailed_=false;
                    loaded=true;
                }
                if(loaded && archive && applyInstrumentSettings){
                    // GUI-initiated import; never invoke host parameter APIs
                    // from the asynchronous bank monitor or the ASIO callback.
                    for(std::size_t i=0;i<archivePlayback.params.size();++i){
                        const auto index=static_cast<std::size_t>(
                            kNormalizeParam-kRunParam)+i;
                        values_[index].store(archivePlayback.params[i],
                                             std::memory_order_release);
                        SingleComponentEffect::setParamNormalized(
                            static_cast<V::ParamID>(kNormalizeParam+i),
                            archivePlayback.params[i]);
                    }
                    (void)session().setCaptureName(archiveName);
                }
            }
        } catch(const std::exception&) {
            // A missing/corrupt bank must never kill an audio callback.
        }
        if(!loaded){
            std::lock_guard<std::mutex> guard(bankMutex_);
            loadFailed_=true;
        }
    }
    void monitorBanks() noexcept {
        // Recorder publishes only complete banks; playback import must work
        // without any VSTGUI editor, and must never touch process().
        std::uint32_t attemptedGeneration=0;
        while(!stopBankWorker_.load(std::memory_order_acquire)){
            const auto ready=session().readyBankGeneration.load(std::memory_order_acquire);
            const auto current=session().generation.load(std::memory_order_acquire);
            if(ready!=0 && ready!=attemptedGeneration && ready==current){
                const auto path=session().latestCompletedRun.chosenFolder();
                if(!path.empty()){
                    attemptedGeneration=ready; // one read per completed run
                    loadSamples(path);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
    }
    void updateSamplerLabel(){
        if(!samplerLabel_)return;
        char line[104]{};
        std::lock_guard<std::mutex> guard(bankMutex_);
        const auto count=player_.loadedZones();
        if(loadFailed_)std::snprintf(line,sizeof(line),"SAMPLER: BANK LOAD FAILED");
        else if(!count)std::snprintf(line,sizeof(line),"SAMPLER: EMPTY");
        else std::snprintf(line,sizeof(line),"SAMPLER: %zu WAVS READY TO PLAY",count);
        samplerLabel_->setText(line);
    }
    void didOpen(VSTGUI::VST3Editor*) override {
        updateFolderLabel();
        updateSamplerLabel();
        updateStatus();
        timer_=VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>(
            [this](VSTGUI::CVSTGUITimer*){updateStatus();},250);
    }
    void willClose(VSTGUI::VST3Editor*) override {
        timer_=nullptr;
        if(folderButton_)folderButton_->unregisterControlListener(this);
        if(defaultButton_)defaultButton_->unregisterControlListener(this);
        if(loadButton_)loadButton_->unregisterControlListener(this);
        folderButton_=nullptr;
        defaultButton_=nullptr;
        loadButton_=nullptr;
        folderLabel_=nullptr;
        samplerLabel_=nullptr;
        statusLabel_=nullptr;
    }
    void updateStatus() {
        // GUI only displays status; WAV loading happens on the bank worker.
        updateSamplerLabel();
        if(!statusLabel_)return;
        const auto config=makeConfig();
        const auto seen=session().observed.load(std::memory_order_relaxed);
        const auto written=session().written.load(std::memory_order_relaxed);
        const auto rejected=session().rejected.load(std::memory_order_relaxed);
        const auto silent=session().silent.load(std::memory_order_relaxed);
        const auto errors=session().errors.load(std::memory_order_relaxed);
        const auto dropped=session().dropped.load(std::memory_order_relaxed);
        const auto gen=session().generation.load(std::memory_order_acquire);
        const a125::RunDisplaySnapshot snapshot{
            gen,
            session().captureEndedGeneration.load(std::memory_order_acquire),
            session().writerFinishedGeneration.load(std::memory_order_acquire),
            session().interruptedGeneration.load(std::memory_order_acquire),
            seen,written,rejected,silent,errors,dropped,
            config.takeCount(),
            session().anchor.load(std::memory_order_acquire)!=kDisarmed
        };
        const char* state=a125::runDisplayText(a125::runDisplayStatus(snapshot));
        char line[180]{};
        std::snprintf(line,sizeof(line),
            "%s  TAKES %u/%zu   WAV %u   REJECT %u   SILENCE %u   ERR %u   LOST %llu",
            state,seen,config.takeCount(),written,rejected,silent,errors,
            static_cast<unsigned long long>(dropped));
        statusLabel_->setText(line);
    }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        if(SingleComponentEffect::initialize(context)!=Steinberg::kResultOk)
            return Steinberg::kResultFalse;
        addAudioOutput(STR16("Sampler Stereo Out"),V::SpeakerArr::kStereo);
        addEventInput(STR16("Play Notes Into 125A"),1);
        addEventOutput(STR16("Generated MIDI"),1);
        parameters.addParameter(STR16("Run (Play in DAW)"),nullptr,1,0.,
           V::ParameterInfo::kCanAutomate,kRunParam);
        parameters.addParameter(STR16("First MIDI note"),nullptr,127,60./127.,
           V::ParameterInfo::kCanAutomate,kFirstParam);
        parameters.addParameter(STR16("Last MIDI note"),nullptr,127,60./127.,
           V::ParameterInfo::kCanAutomate,kLastParam);
        parameters.addParameter(STR16("Note step (1-12)"),nullptr,11,1.,
           V::ParameterInfo::kCanAutomate,kStepParam);
        parameters.addParameter(STR16("Round Robins (1-16)"),nullptr,15,0.,
           V::ParameterInfo::kCanAutomate,kRRParam);
        parameters.addParameter(STR16("Velocity layers (1-3)"),nullptr,2,0.,
           V::ParameterInfo::kCanAutomate,kLayersParam);
        parameters.addParameter(STR16("Note hold (0.25-8s)"),nullptr,0,kDefaultHold,
           V::ParameterInfo::kCanAutomate,kHoldParam);
        parameters.addParameter(STR16("Release capture (0.25-10s)"),nullptr,0,kDefaultTail,
           V::ParameterInfo::kCanAutomate,kTailParam);
        parameters.addParameter(STR16("Gap (0.05-0.5s)"),nullptr,0,kDefaultGap,
           V::ParameterInfo::kCanAutomate,kGapParam);
        parameters.addParameter(STR16("Velocity 1 (1-127)"),nullptr,126,99./126.,
           V::ParameterInfo::kCanAutomate,kVelocity1Param);
        parameters.addParameter(STR16("Velocity 2 (1-127)"),nullptr,126,59./126.,
           V::ParameterInfo::kCanAutomate,kVelocity2Param);
        parameters.addParameter(STR16("Velocity 3 (1-127)"),nullptr,126,119./126.,
           V::ParameterInfo::kCanAutomate,kVelocity3Param);
        parameters.addParameter(STR16("Normalize bank (-1 dBFS)"),nullptr,1,0.,
           V::ParameterInfo::kCanAutomate,kNormalizeParam);
        parameters.addParameter(STR16("Gate mode (off=One Shot)"),nullptr,1,0.,
           V::ParameterInfo::kCanAutomate,kGateParam);
        parameters.addParameter(STR16("Amp Attack (0-2s)"),nullptr,0,0.,
           V::ParameterInfo::kCanAutomate,kAttackParam);
        parameters.addParameter(STR16("Amp Decay (0-3s)"),nullptr,0,0.,
           V::ParameterInfo::kCanAutomate,kDecayParam);
        parameters.addParameter(STR16("Amp Sustain"),nullptr,0,1.,
           V::ParameterInfo::kCanAutomate,kSustainParam);
        parameters.addParameter(STR16("Amp Release (0-3s)"),nullptr,0,.15/3.,
           V::ParameterInfo::kCanAutomate,kAmpReleaseParam);
        parameters.addParameter(STR16("Gain (-12 to +12 dB)"),nullptr,0,.5,
           V::ParameterInfo::kCanAutomate,kGainParam);
        // Independent of any GUI opening: register a non-realtime bank
        // import worker for successful captures from our recorder class.
        stopBankWorker_.store(false,std::memory_order_release);
        bankWorker_=std::thread([this]{monitorBanks();});
        return Steinberg::kResultOk;
    }

    // Display actual MIDI note/velocity/round-robin integers rather than
    // normalized 0..1 fractions in the editor.
    tresult PLUGIN_API getParamStringByValue(
            V::ParamID tag,V::ParamValue value,V::String128 text) override {
        if(tag<=kRunParam || tag>kGainParam)
            return SingleComponentEffect::getParamStringByValue(tag,value,text);
        if(!text)return Steinberg::kResultFalse;
        char buf[32]{};
        switch(tag) {
        case kFirstParam: case kLastParam:
            std::snprintf(buf,sizeof(buf),"%d",a125::stepped(value,0,127));break;
        case kStepParam:
            std::snprintf(buf,sizeof(buf),"%d",a125::stepped(value,1,12));break;
        case kRRParam:
            std::snprintf(buf,sizeof(buf),"%d",a125::stepped(value,1,16));break;
        case kLayersParam:
            std::snprintf(buf,sizeof(buf),"%d",a125::stepped(value,1,3));break;
        case kVelocity1Param: case kVelocity2Param: case kVelocity3Param:
            std::snprintf(buf,sizeof(buf),"%d",a125::midiValue(value));break;
        case kHoldParam:
            std::snprintf(buf,sizeof(buf),"%.2f",.25+value*7.75);break;
        case kTailParam:
            std::snprintf(buf,sizeof(buf),"%.2f",.25+value*9.75);break;
        case kGapParam:
            std::snprintf(buf,sizeof(buf),"%.2f",.05+value*.45);break;
        case kNormalizeParam: case kGateParam:
            // Numeric format guarantees the host's value-to-string roundtrip.
            std::snprintf(buf,sizeof(buf),"%d",value>=.5?1:0);break;
        case kAttackParam: std::snprintf(buf,sizeof(buf),"%.3f",value*2.);break;
        case kDecayParam: case kAmpReleaseParam:
            std::snprintf(buf,sizeof(buf),"%.3f",value*3.);break;
        case kSustainParam:
            std::snprintf(buf,sizeof(buf),"%d",
                static_cast<int>(std::lround(value*100.)));break;
        case kGainParam:
            std::snprintf(buf,sizeof(buf),"%.1f",-12.+value*24.);break;
        default:return Steinberg::kResultFalse;
        }
        Steinberg::UString(text,128).fromAscii(buf);
        return Steinberg::kResultTrue;
    }
    tresult PLUGIN_API getParamValueByString(
            V::ParamID tag,V::TChar* text,V::ParamValue& normalized) override {
        if(tag<=kRunParam || tag>kGainParam)
            return SingleComponentEffect::getParamValueByString(tag,text,normalized);
        if(!text)return Steinberg::kResultFalse;
        double entered=0.;
        if(!Steinberg::UString(text,128).scanFloat(entered) ||
           !std::isfinite(entered))return Steinberg::kResultFalse;
        switch(tag) {
        case kFirstParam: case kLastParam: normalized=entered/127.;break;
        case kStepParam: normalized=(entered-1.)/11.;break;
        case kRRParam: normalized=(entered-1.)/15.;break;
        case kLayersParam: normalized=(entered-1.)/2.;break;
        case kVelocity1Param: case kVelocity2Param: case kVelocity3Param:
            normalized=(entered-1.)/126.;break;
        case kHoldParam: normalized=(entered-.25)/7.75;break;
        case kTailParam: normalized=(entered-.25)/9.75;break;
        case kGapParam: normalized=(entered-.05)/.45;break;
        case kNormalizeParam: case kGateParam: normalized=entered;break;
        case kAttackParam: normalized=entered/2.;break;
        case kDecayParam: case kAmpReleaseParam: normalized=entered/3.;break;
        case kSustainParam:normalized=entered/100.;break;
        case kGainParam:normalized=(entered+12.)/24.;break;
        default:return Steinberg::kResultFalse;
        }
        normalized=std::clamp(normalized,0.,1.);
        return Steinberg::kResultTrue;
    }
    tresult PLUGIN_API setupProcessing(V::ProcessSetup& setup) override {
        sampleRate_=setup.sampleRate;
        cancel();
        return SingleComponentEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size==V::kSample32?Steinberg::kResultTrue:Steinberg::kResultFalse;
    }
    tresult PLUGIN_API getState(Steinberg::IBStream* state) override {
        if(!state) return Steinberg::kResultFalse;
        Steinberg::IBStreamer io(state,kLittleEndian);
        if(!io.writeInt32(0x125A1801)) return Steinberg::kResultFalse;
        // The Run switch is deliberately not persisted: project reopen
        // must never create WAV files before the user explicitly starts.
        for(std::size_t i=1;i<values_.size();++i)
            if(!io.writeDouble(values_[i].load(std::memory_order_relaxed)))
                return Steinberg::kResultFalse;
        auto folder=session().exportTarget.chosenFolder().u8string();
        if(folder.size()>8192)return Steinberg::kResultFalse;
        const auto length=static_cast<Steinberg::int32>(folder.size());
        if(!io.writeInt32(length))return Steinberg::kResultFalse;
        if(length){
            Steinberg::int32 written=0;
            if(state->write(folder.data(),length,&written)!=Steinberg::kResultOk ||
               written!=length)return Steinberg::kResultFalse;
        }
        std::filesystem::path savedBankPath;
        {
            std::lock_guard<std::mutex> guard(bankMutex_);
            savedBankPath=bankFolder_;
        }
        if(savedBankPath.empty() &&
           session().generation.load(std::memory_order_acquire)!=0 &&
           session().readyBankGeneration.load(std::memory_order_acquire)==
           session().generation.load(std::memory_order_acquire)){
            savedBankPath=session().latestCompletedRun.chosenFolder();
        }
        auto bankPath=savedBankPath.u8string();
        if(bankPath.size()>8192)return Steinberg::kResultFalse;
        const auto bankLength=static_cast<Steinberg::int32>(bankPath.size());
        if(!io.writeInt32(bankLength))return Steinberg::kResultFalse;
        if(bankLength){
            Steinberg::int32 written=0;
            if(state->write(bankPath.data(),bankLength,&written)!=Steinberg::kResultOk ||
               written!=bankLength)return Steinberg::kResultFalse;
        }
        const auto instrumentName=session().captureName();
        const auto nameLength=static_cast<Steinberg::int32>(instrumentName.size());
        if(nameLength<=0 || nameLength>192 || !io.writeInt32(nameLength))
            return Steinberg::kResultFalse;
        Steinberg::int32 written=0;
        if(state->write(instrumentName.data(),nameLength,&written)!=Steinberg::kResultOk ||
           written!=nameLength)return Steinberg::kResultFalse;
        return Steinberg::kResultOk;
    }
    tresult PLUGIN_API setState(Steinberg::IBStream* state) override {
        if(!state) return Steinberg::kResultFalse;
        Steinberg::IBStreamer io(state,kLittleEndian);
        Steinberg::int32 version=0;
        if(!io.readInt32(version) ||
           (version!=0x125A0301 && version!=0x125A0401 &&
            version!=0x125A1001 && version!=0x125A1301 &&
             version!=0x125A1501 && version!=0x125A1701 &&
             version!=0x125A1801))
            return Steinberg::kResultFalse;
        // Older P003 files had 9 parameters. Upgrade without breaking
        // existing test projects; 3 editable velocity values get defaults.
        auto loaded=kDefaultParams;
        loaded[a125::Run]=0.;
        const std::size_t count=version==0x125A0301?9:
            version==0x125A1801?loaded.size():
            (version==0x125A1501 || version==0x125A1701)?
                a125::ParameterCount+6:a125::ParameterCount;
        for(std::size_t i=1;i<count;++i) {
            if(!io.readDouble(loaded[i]) || !std::isfinite(loaded[i]) ||
               loaded[i]<0. || loaded[i]>1.) return Steinberg::kResultFalse;
        }
        std::filesystem::path restoredFolder;
        if(version==0x125A1001 || version==0x125A1301 ||
           version==0x125A1501 || version==0x125A1701 ||
           version==0x125A1801){
            Steinberg::int32 length=0;
            if(!io.readInt32(length) || length<0 || length>8192)
                return Steinberg::kResultFalse;
            if(length){
                std::u8string bytes(static_cast<std::size_t>(length),u8' ');
                Steinberg::int32 read=0;
                if(state->read(bytes.data(),length,&read)!=Steinberg::kResultOk ||
                   read!=length)return Steinberg::kResultFalse;
                restoredFolder=std::filesystem::path(bytes);
                if(!restoredFolder.is_absolute())return Steinberg::kResultFalse;
            }
        }
        std::filesystem::path restoredBank;
        if(version==0x125A1301 || version==0x125A1501 ||
           version==0x125A1701 || version==0x125A1801){
            Steinberg::int32 length=0;
            if(!io.readInt32(length) || length<0 || length>8192)
                return Steinberg::kResultFalse;
            if(length){
                std::u8string bytes(static_cast<std::size_t>(length),u8' ');
                Steinberg::int32 read=0;
                if(state->read(bytes.data(),length,&read)!=Steinberg::kResultOk ||
                   read!=length)return Steinberg::kResultFalse;
                restoredBank=std::filesystem::path(bytes);
                if(!restoredBank.is_absolute())return Steinberg::kResultFalse;
            }
        }
        std::string restoredName="Untitled Instrument";
        if(version==0x125A1701 || version==0x125A1801){
            Steinberg::int32 nameLength=0;
            if(!io.readInt32(nameLength) || nameLength<=0 || nameLength>192)
                return Steinberg::kResultFalse;
            restoredName.resize(static_cast<std::size_t>(nameLength));
            Steinberg::int32 read=0;
            if(state->read(restoredName.data(),nameLength,&read)!=Steinberg::kResultOk ||
               read!=nameLength)return Steinberg::kResultFalse;
        }
        if(!session().setCaptureName(restoredName))
            return Steinberg::kResultFalse;
        session().exportTarget.choose(restoredFolder);
        if(!restoredBank.empty())loadSamples(restoredBank);
        // Project load may occur concurrently with audio. Never modify
        // activeRun_ or run_ from the state thread; process() observes
        // Run=Off and stops at its next callback.
        for(std::size_t i=0;i<values_.size();++i){
            values_[i].store(loaded[i],std::memory_order_release);
            SingleComponentEffect::setParamNormalized(
                kRunParam+static_cast<V::ParamID>(i),loaded[i]);
        }
        return Steinberg::kResultOk;
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(!active) {
            cancel();
            runGate_.disarm();
        }
        return SingleComponentEffect::setActive(active);
    }
    tresult PLUGIN_API process(V::ProcessData& data) override {
        if(data.numOutputs>0 && data.outputs && data.symbolicSampleSize==V::kSample32 &&
           data.outputs[0].channelBuffers32) {
            for(int32 ch=0;ch<data.outputs[0].numChannels;++ch) {
                auto* p=data.outputs[0].channelBuffers32[ch];
                if(p && data.numSamples>0)
                    std::memset(p,0,static_cast<std::size_t>(data.numSamples)*sizeof(float));
            }
            data.outputs[0].silenceFlags=3;
        }
        if(data.inputParameterChanges) {
            auto* changes=data.inputParameterChanges;
            for(int32 i=0;i<changes->getParameterCount();++i) {
                auto* q=changes->getParameterData(i);
                if(!q || q->getPointCount()==0) continue;
                const auto id=q->getParameterId();
                if(id<kRunParam || id>kGainParam) continue;
                V::ParamValue value=0; int32 offset=0;
                if(q->getPoint(q->getPointCount()-1,offset,value)==Steinberg::kResultTrue)
                    values_[static_cast<std::size_t>(id-kRunParam)]=std::clamp(value,0.,1.);
            }
        }
        run_=values_[0]>=0.5;
        if(!run_)renderSampler(data);
        const bool playing=data.processContext &&
            (data.processContext->state & V::ProcessContext::kPlaying);
        const bool ready=playing && data.outputEvents && data.numSamples>0;
        // One explicit OFF->ON transition per job. Restarting DAW transport
        // while Run remains ON never triggers another recording session.
        const bool startRequested=runGate_.observe(run_,ready);
        if(!ready || !run_) {
            if(activeRun_){
                if(data.outputEvents)
                    sendPendingNoteOff(data);
                cancel();
            }
            lastBlockEnd_=kDisarmed;
            return Steinberg::kResultOk;
        }
        const std::int64_t blockStart=data.processContext->projectTimeSamples;
        if(blockStart>std::numeric_limits<std::int64_t>::max()-data.numSamples)
            return Steinberg::kResultOk;
        const std::int64_t blockEnd=blockStart+data.numSamples;
        if(activeRun_ && lastBlockEnd_!=kDisarmed && lastBlockEnd_!=blockStart) {
            // Do not accidentally replay a loop onto the same WAV names.
            sendPendingNoteOff(data);
            cancel();
            runGate_.disarm();
            // Keep the visible Run parameter unchanged. Its next
            // OFF->ON transition is required to start another job.
            lastBlockEnd_=kDisarmed;
            return Steinberg::kResultOk;
        }
        if(!activeRun_) {
            if(!startRequested) return Steinberg::kResultOk;
            config_=makeConfig();
            if(!config_.valid()) return Steinberg::kResultOk;
            const auto lead=static_cast<std::int64_t>(sampleRate_/2.);
            if(blockStart>std::numeric_limits<std::int64_t>::max()-lead)
                return Steinberg::kResultOk;
            const auto anchor=blockStart+lead;
            a125::ArchivePlayback snapshot;
            for(std::size_t i=0;i<snapshot.params.size();++i)
                snapshot.params[i]=values_[static_cast<std::size_t>(
                    kNormalizeParam-kRunParam)+i].load(std::memory_order_relaxed);
            session().setCapturedPlayback(snapshot);
            session().publish(config_,static_cast<int>(sampleRate_),anchor);
            activeRun_=true;
        }
        lastBlockEnd_=blockEnd;
        const auto anchor=session().anchor.load(std::memory_order_acquire);
        if(anchor==kDisarmed || blockEnd<=anchor) return Steinberg::kResultOk;
        if(a125::jobCompleteAtBlockStart(config_,anchor,blockStart)){
            // Not until this next block: the recorder may be called after
            // the MIDI generator for a shared DAW processing block.
            session().captureEndedGeneration.store(
                session().generation.load(std::memory_order_acquire),
                std::memory_order_release);
            cancel(false);
            runGate_.disarm();
            lastBlockEnd_=kDisarmed;
            return Steinberg::kResultOk;
        }
        const auto relativeFirst=blockStart<=anchor?0:
             static_cast<std::uint64_t>(blockStart-anchor);
        const auto relativeEnd=static_cast<std::uint64_t>(blockEnd-anchor);
        const auto period=config_.period();
        // Only scan the takes intersecting this audio block, not the whole job.
        const auto first=relativeFirst/period;
        const auto last=std::min<std::uint64_t>(
            config_.takeCount(),relativeEnd/period+1);
        for(std::uint64_t i=first;i<last;++i){
            const auto take=a125::jobTake(config_,static_cast<std::size_t>(i));
            emit(data,blockStart,blockEnd,anchor,take.start,take,true);
            emit(data,blockStart,blockEnd,anchor,take.noteOff,take,false);
        }
        return Steinberg::kResultOk;
    }
private:
    void renderSampler(V::ProcessData& data) noexcept {
        if(!player_.loadedZones() || data.numSamples<=0 ||
           data.numOutputs<1 || !data.outputs ||
           data.symbolicSampleSize!=V::kSample32 ||
           !data.outputs[0].channelBuffers32 ||
           data.outputs[0].numChannels<2)return;
        auto* left=data.outputs[0].channelBuffers32[0];
        auto* right=data.outputs[0].channelBuffers32[1];
        if(!left || !right)return;
        // Per-block playback parameters: ASIO never reads files or locks.
        player_.setNormalize(values_[kNormalizeParam-kRunParam].load(
            std::memory_order_relaxed)>=.5);
        player_.setGate(values_[kGateParam-kRunParam].load(
            std::memory_order_relaxed)>=.5);
        player_.setEnvelope(
            static_cast<float>(values_[kAttackParam-kRunParam].load(
                std::memory_order_relaxed)*2.),
            static_cast<float>(values_[kDecayParam-kRunParam].load(
                std::memory_order_relaxed)*3.),
            static_cast<float>(values_[kSustainParam-kRunParam].load(
                std::memory_order_relaxed)),
            static_cast<float>(values_[kAmpReleaseParam-kRunParam].load(
                std::memory_order_relaxed)*3.));
        const auto gainValue=values_[kGainParam-kRunParam].load(
            std::memory_order_relaxed);
        const float gain=static_cast<float>(
            std::pow(10.,(-12.+24.*gainValue)/20.));
        auto* events=data.inputEvents;
        const int32 total=events?events->getEventCount():0;
        int32 eventIndex=0;
        bool sounded=false;
        for(int32 sample=0;sample<data.numSamples;++sample){
            while(eventIndex<total){
                V::Event e{};
                if(events->getEvent(eventIndex,e)!=Steinberg::kResultOk){
                    ++eventIndex;continue;
                }
                if(e.sampleOffset>sample)break;
                if(e.type==V::Event::kNoteOnEvent){
                    const int velocity=static_cast<int>(
                        std::lround(std::clamp(e.noteOn.velocity,0.f,1.f)*127.f));
                    if(velocity>0)
                        player_.noteOn(e.noteOn.pitch,velocity,sampleRate_);
                    else player_.noteOff(e.noteOn.pitch);
                }else if(e.type==V::Event::kNoteOffEvent){
                    player_.noteOff(e.noteOff.pitch);
                }
                ++eventIndex;
            }
            const auto pcm=player_.renderOne();
            left[sample]=pcm[0]*gain;right[sample]=pcm[1]*gain;
            sounded=sounded || pcm[0]!=0.f || pcm[1]!=0.f;
        }
        if(sounded)data.outputs[0].silenceFlags=0;
    }
    a125::JobConfig makeConfig() const noexcept {
        a125::ParameterValues values{};
        for(std::size_t i=0;i<values.size();++i)
            values[i]=values_[i].load(std::memory_order_relaxed);
        return a125::makeJobFromParameters(values,sampleRate_);
    }
    void cancel(bool interrupted=true) noexcept {
        if(activeRun_) {
            if(interrupted){
                const auto gen=session().generation.load(std::memory_order_acquire);
                session().interruptedGeneration.store(gen,std::memory_order_release);
                session().captureEndedGeneration.store(gen,std::memory_order_release);
            }
            session().anchor.store(kDisarmed,std::memory_order_release);
        }
        activeRun_=false;
        note_.reset();
    }
    void sendPendingNoteOff(V::ProcessData& data) noexcept {
        if(!data.outputEvents) return;
        const auto pending=note_.pending();
        if(!pending) return;
        V::Event e{};
        e.busIndex=0;
        e.sampleOffset=0;
        e.type=V::Event::kNoteOffEvent;
        e.noteOff.channel=0;
        e.noteOff.pitch=static_cast<Steinberg::int16>(pending->pitch);
        e.noteOff.velocity=0.f;
        e.noteOff.noteId=pending->noteId;
        if(data.outputEvents->addEvent(e)==Steinberg::kResultOk)
            note_.acceptedOff(pending->noteId);
    }
    void emit(V::ProcessData& data,std::int64_t first,std::int64_t end,
              std::int64_t anchor,std::uint64_t frame,
              const a125::JobTake& take,bool noteOn) noexcept {
        if(frame>static_cast<std::uint64_t>(
               std::numeric_limits<std::int64_t>::max()-std::max<std::int64_t>(anchor,0)))
            return;
        const auto time=anchor+static_cast<std::int64_t>(frame);
        if(time<first || time>=end)return;
        V::Event e{};
        e.busIndex=0;
        e.sampleOffset=static_cast<int32>(time-first);
        if(noteOn){
            e.type=V::Event::kNoteOnEvent;
            e.noteOn.channel=0;
            e.noteOn.pitch=static_cast<Steinberg::int16>(take.note);
            e.noteOn.velocity=static_cast<float>(take.velocity)/127.f;
            e.noteOn.length=-1;
            e.noteOn.noteId=static_cast<int32>(take.takeIndex+1);
        } else {
            e.type=V::Event::kNoteOffEvent;
            e.noteOff.channel=0;
            e.noteOff.pitch=static_cast<Steinberg::int16>(take.note);
            e.noteOff.velocity=0.f;
            e.noteOff.noteId=static_cast<int32>(take.takeIndex+1);
        }
        // Only events accepted by the host's output bus change our note state.
        if(data.outputEvents->addEvent(e)==Steinberg::kResultOk) {
            if(noteOn) note_.acceptedOn(take.note,static_cast<int>(take.takeIndex+1));
            else note_.acceptedOff(static_cast<int>(take.takeIndex+1));
        }
    }
    std::array<std::atomic<double>,kParamCount> values_{
        0.,60./127.,60./127.,1.,0.,0.,kDefaultHold,kDefaultTail,kDefaultGap,
        99./126.,59./126.,119./126., 0.,0.,0.,0.,1.,.15/3.,.5};
    a125::JobConfig config_{};
    a125::BuiltInSampler player_{};
    std::mutex bankMutex_;
    std::filesystem::path bankFolder_{};
    std::atomic<bool> stopBankWorker_{false};
    std::thread bankWorker_;
    bool loadFailed_=false;
    a125::ActiveNote note_{};
    a125::RunGate runGate_{};
    VSTGUI::CTextLabel* statusLabel_{nullptr};  // owned by active editor
    VSTGUI::CTextLabel* folderLabel_{nullptr};
    VSTGUI::CTextLabel* samplerLabel_{nullptr};
    VSTGUI::CTextButton* loadButton_{nullptr};
    VSTGUI::CTextButton* folderButton_{nullptr};
    VSTGUI::CTextButton* defaultButton_{nullptr};
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_{};
    double sampleRate_=44100.;
    bool run_=false,activeRun_=false;
    std::int64_t lastBlockEnd_=kDisarmed;
};

// SPSC queue: audio callback is producer, disk writer is consumer. No locks,
// file operations or heap allocations occur inside the audio callback.
struct CapturedFrame {
    std::uint64_t offset=0;
    std::uint64_t expectedFrames=0;
    float left=0,right=0;
    std::uint32_t generation=0;
    std::uint32_t take=0;
    std::uint8_t note=0,velocity=0,roundRobin=0;
    bool last=false;
    bool jobEnd=false;
};
class SpscQueue {
public:
    static constexpr std::uint32_t capacity=262144;
    bool push(const CapturedFrame& f) noexcept {
        const auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=capacity){
            dropped_.fetch_add(1,std::memory_order_relaxed);
            return false;
        }
        buffer_[w&(capacity-1)]=f;
        write_.store(w+1,std::memory_order_release);
        return true;
    }
    bool pop(CapturedFrame& f) noexcept {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        f=buffer_[r&(capacity-1)];
        read_.store(r+1,std::memory_order_release);
        return true;
    }
    bool empty() const noexcept {
        return read_.load(std::memory_order_acquire)==
               write_.load(std::memory_order_acquire);
    }
    std::uint64_t dropped() const noexcept {return dropped_.load();}
private:
    std::vector<CapturedFrame> buffer_=std::vector<CapturedFrame>(capacity);
    std::atomic<std::uint32_t> read_{0},write_{0};
    std::atomic<std::uint64_t> dropped_{0};
};

class Recorder final : public V::SingleComponentEffect {
public:
    static FUnknown* createInstance(void*) {return (V::IAudioProcessor*)new Recorder();}
    ~Recorder() override {stopWorker();}
    tresult PLUGIN_API initialize(FUnknown* context) override {
        if(SingleComponentEffect::initialize(context)!=Steinberg::kResultOk)
            return Steinberg::kResultFalse;
        addAudioInput(STR16("Stereo Input"),V::SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Output"),V::SpeakerArr::kStereo);
        return Steinberg::kResultOk;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size==V::kSample32?Steinberg::kResultTrue:Steinberg::kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(V::ProcessSetup& setup) override {
        sampleRate_=static_cast<std::uint32_t>(setup.sampleRate);
        return SingleComponentEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(active) {
            if(!worker_.joinable()){
                exit_.store(false,std::memory_order_release);
                worker_=std::thread([this]{consume();});
            }
        } else stopWorker();
        return SingleComponentEffect::setActive(active);
    }
    tresult PLUGIN_API process(V::ProcessData& data) override {
        copyAudio(data);
        if(!data.processContext || !(data.processContext->state&V::ProcessContext::kPlaying) ||
           data.numInputs==0 || !data.inputs || data.symbolicSampleSize!=V::kSample32 ||
           !data.inputs[0].channelBuffers32 || !worker_.joinable() || data.numSamples<=0)
            return Steinberg::kResultOk;
        const auto generationBefore=session().generation.load(std::memory_order_acquire);
        const auto anchor=session().anchor.load(std::memory_order_acquire);
        if(anchor==kDisarmed) return Steinberg::kResultOk;
        const auto config=session().read();
        const auto generationAfter=session().generation.load(std::memory_order_acquire);
        if(generationBefore!=generationAfter ||
           session().anchor.load(std::memory_order_acquire)!=anchor ||
           session().sampleRate.load(std::memory_order_relaxed)!=
                static_cast<int>(sampleRate_) || !config.valid())
            return Steinberg::kResultOk;
        const auto generation=generationAfter;
        const auto* left=data.inputs[0].channelBuffers32[0];
        const auto* right=data.inputs[0].numChannels>1?
           data.inputs[0].channelBuffers32[1]:left;
        if(!left || !right) return Steinberg::kResultOk;
        const auto blockStart=data.processContext->projectTimeSamples;
        for(int32 i=0;i<data.numSamples;++i){
            const auto absFrame=blockStart+i;
            if(absFrame<anchor)continue;
            const auto loc=a125::locateJobFrame(config,
                          static_cast<std::uint64_t>(absFrame-anchor));
            if(!loc) continue;
            const auto& take=loc->take;
            const CapturedFrame frame{loc->offset,config.holdFrames+config.tailFrames,
                left[i],right[i],generation,
                static_cast<std::uint32_t>(take.takeIndex),
                static_cast<std::uint8_t>(take.note),
                static_cast<std::uint8_t>(take.velocity),
                static_cast<std::uint8_t>(take.roundRobin),loc->last,
                loc->last && take.takeIndex+1==config.takeCount()};
            if(!queue_.push(frame))
                session().dropped.fetch_add(1,std::memory_order_relaxed);
            if(loc->last)
                session().observed.store(static_cast<std::uint32_t>(take.takeIndex+1),
                                         std::memory_order_release);
        }
        return Steinberg::kResultOk;
    }
private:
    void stopWorker(){
        exit_.store(true,std::memory_order_release);
        if(worker_.joinable())worker_.join();
    }
    static std::filesystem::path outputRoot(){
#ifdef _WIN32
        char* home=nullptr; std::size_t len=0;
        if(_dupenv_s(&home,&len,"USERPROFILE")==0 && home){
            const std::filesystem::path result=
                std::filesystem::path(home)/"Documents"/"125A AutoSampler";
            std::free(home);
            return result;
        }
#else
        const char* home=std::getenv("HOME");
        if(home && *home) return std::filesystem::path(home)/"125A AutoSampler";
#endif
        return std::filesystem::temp_directory_path()/"125A AutoSampler";
    }
    void consume(){
        std::vector<a125::StereoFrame> current;
        a125::TakeIntegrity integrity;
        std::uint32_t generation=0;
        std::uint32_t takeIndex=0;
        std::uint8_t note=0,velocity=0,rr=0;
        std::filesystem::path dir;
        std::ofstream report;
        bool haveRun=false;
        bool attemptedRun=false;
        a125::JobConfig recordedConfig{};
        std::string recordedName="Untitled Instrument";
        a125::ArchivePlayback recordedPlayback;
        bool configWasVerified=false;
        std::uint32_t closedGeneration=0;
        std::uint64_t runWritten=0,runRejected=0,runSilent=0,runErrors=0;
        std::uint64_t droppedAtRunStart=0;
        auto root=outputRoot();
        auto finishReport=[&] {
            if(report){
                report << "SUMMARY,exported," << runWritten << "\n"
                       << "SUMMARY,rejected," << runRejected << "\n"
                       << "SUMMARY,silent," << runSilent << "\n"
                       << "SUMMARY,exportErrors," << runErrors << "\n"
                       << "SUMMARY,droppedFrames,"
                       << (queue_.dropped()-droppedAtRunStart) << "\n";
                report.flush();
            }
        };
        auto closeRun=[&] {
            // Publish only complete captures. The ZIP is created on this
            // worker thread, never in the ASIO callback. A partial ZIP is
            // removed and a failed archive never masquerades as success.
            if(report){
                const bool complete=configWasVerified && haveRun &&
                    runWritten==recordedConfig.takeCount() &&
                    runRejected==0 && runSilent==0 && runErrors==0 &&
                    queue_.dropped()==droppedAtRunStart &&
                    session().interruptedGeneration.load(std::memory_order_acquire)!=generation;
                if(complete){
                    try {
                        const auto stem=a125::safe125AStem(recordedName);
                        // Unique run suffix prevents overwriting an earlier
                        // archive with the same user-facing instrument name.
                        const auto archive=root/(stem+" ["+
                            dir.filename().string()+"].125A");
                        a125::write125AArchive(dir,archive,recordedName,
                                               recordedPlayback);
                        report<<"ARCHIVE,125A_READY,"<<archive.filename().string()<<"\n";
                        report<<"BANK,125A_WAVS_READY\n";
                        // Reader imports the self-contained ZIP into RAM.
                        session().latestCompletedRun.choose(archive);
                        session().readyBankGeneration.store(
                            generation,std::memory_order_release);
                    }catch(const std::exception&){
                        ++runErrors;
                        session().errors.fetch_add(1,std::memory_order_relaxed);
                        report<<"ARCHIVE,FAILED\nBANK,SKIPPED_ARCHIVE_ERROR\n";
                    }
                }else report<<"BANK,SKIPPED_INCOMPLETE\n";
            }
            finishReport();
            report.close();
            if(generation!=0)
                session().writerFinishedGeneration.store(
                    generation,std::memory_order_release);
        };
        auto record=[&](const char* status,std::uint64_t frames) {
            if(report) {
                report << takeIndex << ',' << static_cast<unsigned>(note) << ','
                       << static_cast<unsigned>(velocity) << ','
                       << static_cast<unsigned>(rr) << ',' << status << ','
                       << frames << '\n';
                report.flush();
            }
        };
        auto reject=[&](const char* reason) {
            rejected_.fetch_add(1,std::memory_order_relaxed);
            ++runRejected;
            session().rejected.fetch_add(1,std::memory_order_relaxed);
            record(reason,current.size());
            current.clear();
        };
        auto openRun=[&](std::uint32_t gen) {
            // Snapshot the immutable MIDI plan and user-selected folder
            // outside the audio thread, once per recorder generation.
            recordedConfig=session().read();
            recordedName=session().captureName();
            recordedPlayback=session().readCapturedPlayback();
            configWasVerified=recordedConfig.valid() &&
                session().generation.load(std::memory_order_acquire)==gen;
            root=a125::ExportTarget::resolve(
                session().exportTarget.chosenFolder(),outputRoot());
            dir.clear();
            haveRun=false;
            attemptedRun=true;
            runWritten=runRejected=runSilent=runErrors=0;
            droppedAtRunStart=queue_.dropped();
            try {
                std::filesystem::create_directories(root);
                const auto stamp=std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                for(int n=0;n<1000 && !haveRun;++n) {
                    const auto candidate=root/("Run_"+std::to_string(stamp)+
                        "_G"+std::to_string(gen)+"_"+std::to_string(n));
                    if(std::filesystem::create_directory(candidate)){
                        dir=candidate; haveRun=true;
                    }
                }
                if(!haveRun) throw std::runtime_error("cannot create unique run folder");
                report.open(dir/"RUN-REPORT.csv",std::ios::binary|std::ios::trunc);
                if(!report) throw std::runtime_error("cannot create run report");
                report << "take,note,velocity,rr,result,frames\n";
                report.flush();
            } catch(const std::exception&) {
                exportErrors_.fetch_add(1,std::memory_order_relaxed);
                ++runErrors;
                session().errors.fetch_add(1,std::memory_order_relaxed);
                report.close();
                haveRun=false;
            }
        };
        for(;;) {
            CapturedFrame f;
            if(!queue_.pop(f)) {
                if(exit_.load(std::memory_order_acquire) && queue_.empty()) break;
                // Explicit Stop is fail-closed: never turn a partial take
                // into a WAV. Late frames from that generation are ignored.
                if(attemptedRun && generation!=closedGeneration &&
                   session().interruptedGeneration.load(std::memory_order_acquire)==generation){
                    if(integrity.abandon()) reject("ABORTED");
                    closeRun();
                    closedGeneration=generation;
                } else if(attemptedRun && generation!=closedGeneration &&
                   session().captureEndedGeneration.load(std::memory_order_acquire)==generation){
                    if(integrity.abandon()) reject("INCOMPLETE");
                    closeRun();
                    closedGeneration=generation;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if(f.generation==closedGeneration) continue;
            const auto decision=integrity.consume(
                {f.generation,f.take,f.offset,f.expectedFrames,f.last});
            if(decision.abandonedPrevious) reject("INCOMPLETE");
            if(!attemptedRun || generation!=f.generation) {
                // Close the previous generation before publishing the new
                // generation ID to the background report and GUI.
                if(attemptedRun && report) closeRun();
                generation=f.generation;
                openRun(generation);
            }
            if(decision.started){
                current.clear();
                takeIndex=f.take;
                note=f.note;
                velocity=f.velocity;
                rr=f.roundRobin;
            }
            if(decision.acceptFrame) current.push_back({f.left,f.right});
            else current.clear();
            if(decision.rejected) reject("INVALID");
            if(decision.completed){
                // Even a silent take must be reported; silence is not success.
                try{
                    if(!haveRun) throw std::runtime_error("run directory unavailable");
                    const auto wav=a125::trimAndFade(current,{});
                    if(wav.empty()){
                        silent_.fetch_add(1,std::memory_order_relaxed);
                        ++runSilent;
                        session().silent.fetch_add(1,std::memory_order_relaxed);
                        record("SILENCE",current.size());
                    } else {
                        const std::string filename="N"+std::to_string(note)+
                            "_V"+std::to_string(velocity)+"_RR"+
                            (rr<10?"0":"")+std::to_string(rr)+".wav";
                        const auto output=dir/filename;
                        if(std::filesystem::exists(output))
                            throw std::runtime_error("duplicate take output");
                        a125::writeWav24(output.string(),sampleRate_,wav);
                        exported_.fetch_add(1,std::memory_order_relaxed);
                        ++runWritten;
                        session().written.fetch_add(1,std::memory_order_relaxed);
                        record("OK",current.size());
                    }
                } catch(const std::exception&) {
                    exportErrors_.fetch_add(1,std::memory_order_relaxed);
                    ++runErrors;
                    session().errors.fetch_add(1,std::memory_order_relaxed);
                    record("EXPORT_ERROR",current.size());
                }
                current.clear();
            }
            if(f.jobEnd){
                // Final take is recorded and saved. Flush/close the report
                // immediately; no DAW Stop or plugin unload is required.
                if(integrity.abandon()) reject("INCOMPLETE");
                closeRun();
                closedGeneration=generation;
            }
        }
        if(integrity.abandon()) reject("ABORTED");
        if(attemptedRun && report) closeRun();
    }
    std::uint32_t sampleRate_=44100;
    SpscQueue queue_;
    std::atomic<bool> exit_{false};
    std::thread worker_;
    std::atomic<std::uint64_t> rejected_{0},exportErrors_{0},exported_{0},silent_{0};
};
} // namespace

BEGIN_FACTORY_DEF("125A","https://github.com/challanger2000","")
 DEF_CLASS2(INLINE_UID_FROM_FUID(kMidiID),
  Steinberg::PClassInfo::kManyInstances,kVstAudioEffectClass,
  "125A AutoSampler",0,"Instrument","0.13.0",kVstVersionString,
  MidiGenerator::createInstance)
 DEF_CLASS2(INLINE_UID_FROM_FUID(kRecorderID),
  Steinberg::PClassInfo::kManyInstances,kVstAudioEffectClass,
  "125A AutoSampler Recorder",0,"Fx","0.13.0",kVstVersionString,
  Recorder::createInstance)
END_FACTORY
