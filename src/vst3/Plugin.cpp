// 125A Auto Sampler: P002 prototype with two VST3 components.
// DAW hosts the source instrument; this bundle never hosts third-party VSTs.
#include "core/SamplingPlan.h"
#include "core/SampleProcessing.h"
#include "core/Timeline.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace V = Steinberg::Vst;
using Steinberg::FUnknown;
using Steinberg::tresult;
using Steinberg::int32;
using Steinberg::TBool;
using Steinberg::IBStream;

const Steinberg::FUID kMidiID(0xA181019A,0x3DC9472C,0xB69A0F73,0xFA25C101);
const Steinberg::FUID kRecorderID(0xA181019A,0x3DC9472C,0xB69A0F73,0xFA25C102);

constexpr std::int64_t kDisarmed = std::numeric_limits<std::int64_t>::min();
struct Session {
    std::atomic<std::int64_t> anchor{kDisarmed};
    std::atomic<std::uint32_t> generation{0};
    std::atomic<int> sampleRate{44100};
};
Session& session() noexcept { static Session s; return s; }

a125::Settings proofSettings(double sampleRate) {
    const auto sr = static_cast<std::uint64_t>(std::max(8000., sampleRate));
    a125::Settings s;
    s.firstNote=60; s.lastNote=60; s.velocities={100}; s.roundRobins=8;
    s.holdFrames=sr;
    s.tailFrames=sr*2;
    s.gapFrames=sr/10;
    return s;
}

std::vector<a125::Take> proofPlan(double sampleRate) {
    return a125::makePlan(proofSettings(sampleRate));
}

enum : V::ParamID { kRunParam = 1000 };
constexpr Steinberg::int32 kStereoChannels=2;

void copyAudio(const V::ProcessData& data) noexcept {
    if (data.numInputs < 1 || data.numOutputs < 1 || !data.inputs || !data.outputs) return;
    const auto& in = data.inputs[0];
    auto& out = data.outputs[0];
    out.silenceFlags = in.silenceFlags;
    if (data.symbolicSampleSize != V::kSample32) return;
    if (!in.channelBuffers32 || !out.channelBuffers32) return;
    for (int32 ch=0; ch<std::min(in.numChannels,out.numChannels); ++ch) {
        if (in.channelBuffers32[ch] && out.channelBuffers32[ch] &&
            in.channelBuffers32[ch]!=out.channelBuffers32[ch] && data.numSamples > 0)
            std::memcpy(out.channelBuffers32[ch],in.channelBuffers32[ch],
                        static_cast<std::size_t>(data.numSamples)*sizeof(float));
    }
}

class MidiGenerator final : public V::SingleComponentEffect {
public:
    static FUnknown* createInstance(void*) { return (V::IAudioProcessor*)new MidiGenerator(); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        if (SingleComponentEffect::initialize(context) != Steinberg::kResultOk)
            return Steinberg::kResultFalse;
        addAudioOutput(STR16("Silent Output"),V::SpeakerArr::kStereo);
        addEventOutput(STR16("MIDI Out"),1);
        parameters.addParameter(STR16("Run (press Play)"),nullptr,1,0.,
            V::ParameterInfo::kCanAutomate,kRunParam);
        return Steinberg::kResultOk;
    }
    tresult PLUGIN_API setupProcessing(V::ProcessSetup& setup) override {
        plan_=proofPlan(setup.sampleRate);  // setupProcessing is non-real-time.
        sampleRate_=setup.sampleRate;
        return SingleComponentEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {
        return size==V::kSample32?Steinberg::kResultTrue:Steinberg::kResultFalse;
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if (!active) {
            cancel();
        }
        return SingleComponentEffect::setActive(active);
    }
    tresult PLUGIN_API process(V::ProcessData& data) override {
        // This VST3 is a MIDI-generating instrument with silent audio output.
        if(data.numOutputs > 0 && data.outputs && data.symbolicSampleSize==V::kSample32
           && data.outputs[0].channelBuffers32) {
            for(int32 ch=0;ch<data.outputs[0].numChannels;++ch) {
                auto* buf=data.outputs[0].channelBuffers32[ch];
                if(buf && data.numSamples>0)
                    std::memset(buf,0,static_cast<std::size_t>(data.numSamples)*sizeof(float));
            }
            data.outputs[0].silenceFlags=3;
        }
        if (data.inputParameterChanges) {
            auto* changes=data.inputParameterChanges;
            for(int32 i=0;i<changes->getParameterCount();++i) {
                auto* q=changes->getParameterData(i);
                if (!q || q->getParameterId()!=kRunParam || q->getPointCount()==0) continue;
                V::ParamValue value=0;int32 offset=0;
                if(q->getPoint(q->getPointCount()-1,offset,value)==Steinberg::kResultTrue)
                    run_=value>=0.5;
            }
        }
        const bool playing = data.processContext &&
            (data.processContext->state & V::ProcessContext::kPlaying);
        if (!playing || !run_ || !data.outputEvents) {
            if(activeRun_) {
                // Send note-off for a possibly sounding note on Stop/Run Off.
                const auto anchor=session().anchor.load(std::memory_order_acquire);
                if(anchor!=kDisarmed && data.processContext)
                    sendPendingNoteOff(data,anchor);
                cancel();
            }
            return Steinberg::kResultOk;
        }
        if(plan_.empty()) return Steinberg::kResultOk;
        const auto blockStart=data.processContext->projectTimeSamples;
        if(!activeRun_) {
            // A half-second lead gives the recorder time to observe the
            // shared session regardless of which insert the host calls first.
            const auto lead=static_cast<std::int64_t>(sampleRate_/2.);
            if(blockStart > std::numeric_limits<std::int64_t>::max()-lead)
                return Steinberg::kResultOk;
            session().sampleRate.store(static_cast<int>(sampleRate_),std::memory_order_relaxed);
            session().anchor.store(blockStart+lead,std::memory_order_release);
            session().generation.fetch_add(1,std::memory_order_release);
            activeRun_=true;
        }
        const auto anchor=session().anchor.load(std::memory_order_acquire);
        const auto blockEnd=blockStart+data.numSamples;
        for(std::size_t i=0;i<plan_.size();++i) {
            const auto& take=plan_[i];
            sendIfWithin(data,blockStart,blockEnd,anchor,
                         take.noteOnFrame,take.note,take.velocity,i,true);
            sendIfWithin(data,blockStart,blockEnd,anchor,
                         take.noteOffFrame,take.note,take.velocity,i,false);
        }
        return Steinberg::kResultOk;
    }
private:
    void cancel() noexcept {
        if(activeRun_) session().anchor.store(kDisarmed,std::memory_order_release);
        activeRun_=false;
    }
    void sendPendingNoteOff(V::ProcessData& data,std::int64_t anchor) noexcept {
        const auto at=data.processContext->projectTimeSamples;
        for(std::size_t i=0;i<plan_.size();++i) {
            const auto& t=plan_[i];
            const auto on=anchor+static_cast<std::int64_t>(t.noteOnFrame);
            const auto off=anchor+static_cast<std::int64_t>(t.noteOffFrame);
            if(at>=on && at<off) {
                V::Event e{};
                e.type=V::Event::kNoteOffEvent; e.busIndex=0;e.sampleOffset=0;
                e.noteOff.channel=0;e.noteOff.pitch=static_cast<Steinberg::int16>(t.note);
                e.noteOff.velocity=0.f;e.noteOff.noteId=static_cast<int32>(i+1);
                data.outputEvents->addEvent(e);
                break;
            }
        }
    }
    static void sendIfWithin(V::ProcessData& data,std::int64_t first,
          std::int64_t end,std::int64_t anchor,std::uint64_t relative,
          int note,int velocity,std::size_t takeIndex,bool on) noexcept {
        if(anchor==kDisarmed || relative>static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()-std::max<std::int64_t>(anchor,0))) return;
        const auto time=anchor+static_cast<std::int64_t>(relative);
        if(time<first || time>=end) return;
        V::Event event{};
        event.busIndex=0;event.sampleOffset=static_cast<int32>(time-first);
        if(on) {
            event.type=V::Event::kNoteOnEvent;
            event.noteOn.channel=0;event.noteOn.pitch=static_cast<Steinberg::int16>(note);
            event.noteOn.velocity=static_cast<float>(velocity)/127.f;
            event.noteOn.length=-1;event.noteOn.noteId=static_cast<int32>(takeIndex+1);
        } else {
            event.type=V::Event::kNoteOffEvent;
            event.noteOff.channel=0;event.noteOff.pitch=static_cast<Steinberg::int16>(note);
            event.noteOff.velocity=0.f;event.noteOff.noteId=static_cast<int32>(takeIndex+1);
        }
        data.outputEvents->addEvent(event);
    }
    std::vector<a125::Take> plan_;
    double sampleRate_=44100.;
    bool run_=false;
    bool activeRun_=false;
};

// SPSC audio queue. Read/write never hold a lock; full queue drops samples.
// A discontinuous take is rejected by the worker instead of saving corruption.
struct CapturedFrame {
    std::uint64_t offset=0;
    float left=0,right=0;
    std::uint32_t generation=0;
    std::uint16_t take=0;
    bool last=false;
};
class SpscQueue {
public:
    static constexpr std::uint32_t capacity=262144;
    bool push(const CapturedFrame& f) noexcept {
        auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=capacity) {
            dropped_.fetch_add(1,std::memory_order_relaxed);
            return false;
        }
        buffer_[w & (capacity-1)]=f;
        write_.store(w+1,std::memory_order_release);
        return true;
    }
    bool pop(CapturedFrame& f) noexcept {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire)) return false;
        f=buffer_[r & (capacity-1)];
        read_.store(r+1,std::memory_order_release);
        return true;
    }
    std::uint64_t dropped() const noexcept { return dropped_.load(); }
    bool empty() const noexcept {
        return read_.load(std::memory_order_acquire)==write_.load(std::memory_order_acquire);
    }
private:
    std::vector<CapturedFrame> buffer_=std::vector<CapturedFrame>(capacity);
    std::atomic<std::uint32_t> read_{0},write_{0};
    std::atomic<std::uint64_t> dropped_{0};
};

class Recorder final : public V::SingleComponentEffect {
public:
    static FUnknown* createInstance(void*) { return (V::IAudioProcessor*)new Recorder(); }
    Recorder()=default;
    ~Recorder() override { stopWorker(); }

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
        plan_=proofPlan(setup.sampleRate);
        sampleRate_=static_cast<std::uint32_t>(setup.sampleRate);
        return SingleComponentEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API setActive(TBool active) override {
        if(active) {
            if(!worker_.joinable()) {
                exit_.store(false,std::memory_order_release);
                worker_=std::thread([this]{ consume(); });
            }
        } else stopWorker();
        return SingleComponentEffect::setActive(active);
    }
    tresult PLUGIN_API process(V::ProcessData& data) override {
        copyAudio(data);
        if(!data.processContext || !(data.processContext->state & V::ProcessContext::kPlaying)
           || data.numInputs==0 || !data.inputs || !data.inputs[0].channelBuffers32
           || data.symbolicSampleSize!=V::kSample32 || !worker_.joinable()
           || plan_.empty() || data.numSamples<=0) return Steinberg::kResultOk;
        const auto anchor=session().anchor.load(std::memory_order_acquire);
        if(anchor==kDisarmed) return Steinberg::kResultOk;
        const auto generation=session().generation.load(std::memory_order_acquire);
        const auto* const left=data.inputs[0].channelBuffers32[0];
        const auto* const right=data.inputs[0].numChannels>1?
                                   data.inputs[0].channelBuffers32[1]:left;
        if(!left || !right) return Steinberg::kResultOk;
        const auto blockStart=data.processContext->projectTimeSamples;
        for(int32 i=0;i<data.numSamples;++i) {
            const auto absFrame=blockStart+i;
            if(absFrame<anchor) continue;
            const auto relative=static_cast<std::uint64_t>(absFrame-anchor);
            const auto location=a125::locateCaptureFrame(plan_,relative);
            if(!location) continue;
            const CapturedFrame frame{location->takeFrame,left[i],right[i],
                  generation,static_cast<std::uint16_t>(location->takeIndex),location->lastFrame};
            queue_.push(frame);
        }
        return Steinberg::kResultOk;
    }
private:
    void stopWorker() {
        exit_.store(true,std::memory_order_release);
        if(worker_.joinable()) worker_.join();
    }
    static std::filesystem::path outputRoot() {
        const char* home=std::getenv("USERPROFILE");
        return (home && *home)?std::filesystem::path(home)/"Documents"/"125A AutoSampler":
               std::filesystem::temp_directory_path()/"125A AutoSampler";
    }
    void consume() {
        std::vector<a125::StereoFrame> current;
        std::uint32_t lastGeneration=0;
        int currentTake=-1;
        bool broken=false;
        std::uint64_t nextOffset=0;
        const auto dir=outputRoot();
        bool dirReady=false;
        // Background worker never interacts with the host or its UI.
        for(;;) {
            CapturedFrame f;
            if(!queue_.pop(f)) {
                if(exit_.load(std::memory_order_acquire) && queue_.empty()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if(currentTake!=static_cast<int>(f.take) || lastGeneration!=f.generation) {
                current.clear();
                currentTake=static_cast<int>(f.take);
                lastGeneration=f.generation;
                nextOffset=0;
                broken=false;
            }
            if(f.offset!=nextOffset) broken=true;
            nextOffset=f.offset+1;
            if(!broken) current.push_back({f.left,f.right});
            if(f.last) {
                if(!broken && !current.empty() && f.take<plan_.size()) {
                    try {
                        if(!dirReady){ std::filesystem::create_directories(dir);dirReady=true; }
                        const auto& t=plan_[f.take];
                        const auto name="G"+std::to_string(f.generation)
                             +"_N"+std::to_string(t.note)
                             +"_V"+std::to_string(t.velocity)
                             +"_RR"+(t.roundRobin<10?"0":"")+std::to_string(t.roundRobin)+".wav";
                        const auto processed=a125::trimAndFade(current,{});
                        if(!processed.empty())
                            a125::writeWav24((dir/name).string(),sampleRate_,processed);
                    } catch(const std::exception&) {
                        // TODO: expose export errors to editor UI and validator.
                    }
                }
                current.clear();currentTake=-1;
            }
        }
    }
    std::vector<a125::Take> plan_;
    std::uint32_t sampleRate_=44100;
    SpscQueue queue_;
    std::atomic<bool> exit_{false};
    std::thread worker_;
};
} // namespace

BEGIN_FACTORY_DEF("125A","https://github.com/challanger2000","")
 DEF_CLASS2(INLINE_UID_FROM_FUID(kMidiID),
  Steinberg::PClassInfo::kManyInstances,Steinberg::kVstAudioEffectClass,
  "125A AutoSampler MIDI",0,"Instrument","0.2.0",Steinberg::Vst::kVstVersionString,
  MidiGenerator::createInstance)
 DEF_CLASS2(INLINE_UID_FROM_FUID(kRecorderID),
  Steinberg::PClassInfo::kManyInstances,Steinberg::kVstAudioEffectClass,
  "125A AutoSampler Recorder",0,"Fx","0.2.0",Steinberg::Vst::kVstVersionString,
  Recorder::createInstance)
END_FACTORY
