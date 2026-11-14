#include "core/SamplingPlan.h"
#include "core/SampleProcessing.h"
#include "core/JobSchedule.h"
#include "core/JobParameters.h"
#include "core/CaptureIntegrity.h"
#include "core/ActiveNote.h"
#include "core/RunGate.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}
template<class Fn> bool throws(Fn fn) {
    try { fn(); return false; } catch(const std::exception&) { return true; }
}
void testEightRoundRobins() {
    a125::Settings cfg;
    cfg.firstNote=60; cfg.lastNote=60; cfg.roundRobins=8;
    cfg.velocities={40,80,120};
    cfg.holdFrames=11; cfg.tailFrames=7; cfg.gapFrames=5;
    const auto plan = a125::makePlan(cfg);
    require(plan.size()==24, "8 RR * 3 velocity should be 24 WAV takes");
    for (int layer=0;layer<3;++layer)
        for (int rr=1;rr<=8;++rr) {
            const auto& take=plan[static_cast<std::size_t>(layer*8+rr-1)];
            require(take.note==60 && take.velocity==cfg.velocities[layer], "wrong metadata");
            require(take.roundRobin==rr, "RR must be one-based and unique");
            require(take.noteOffFrame-take.noteOnFrame==11, "wrong hold");
            require(take.captureEndFrame-take.noteOffFrame==7, "wrong tail");
        }
}
void testSchedulerSampleOffsets() {
    a125::Settings cfg;
    cfg.holdFrames=10; cfg.tailFrames=5; cfg.gapFrames=3;
    cfg.roundRobins=2;
    a125::MidiScheduler midi(a125::makePlan(cfg));
    std::vector<std::pair<a125::MidiKind,std::uint64_t>> events;
    for(std::uint64_t pos=0;pos<40;pos+=7)
        midi.render(pos,7,[&](const a125::MidiEvent& e){
            events.emplace_back(e.kind,pos+e.sampleOffset);
        });
    require(events.size()==4, "two note-ons and two note-offs expected");
    require(events[0].first==a125::MidiKind::NoteOn && events[0].second==0, "first on");
    require(events[1].first==a125::MidiKind::NoteOff && events[1].second==10, "first off");
    require(events[2].first==a125::MidiKind::NoteOn && events[2].second==18, "second on");
    require(events[3].first==a125::MidiKind::NoteOff && events[3].second==28, "second off");
}
void testValidation() {
    a125::Settings cfg;
    cfg.roundRobins=17;
    require(throws([&]{(void)a125::makePlan(cfg);}),"RR >16 must reject");
    cfg.roundRobins=8; cfg.velocities={0};
    require(throws([&]{(void)a125::makePlan(cfg);}),"velocity zero must reject");
}
void testAllMidiVelocityRoundTrips() {
    for(int value=1;value<=127;++value){
        const auto normalized=a125::midiNormalized(value);
        require(a125::midiValue(normalized)==value,
                "all 127 MIDI velocity values must round-trip");
    }
    const auto cfg=a125::makeJobFromParameters(a125::defaultParameters,44100.);
    require(cfg.velocities[0]==100,
            "first default velocity exactly MIDI 100");
}
void testEditableVelocitiesAndRanges() {
    a125::ParameterValues p=a125::defaultParameters;
    require(a125::makeJobFromParameters(p,44100.).takeCount()==8,
        "default is one note, eight RR and one velocity");
    auto single=a125::makeJobFromParameters(p,44100.);
    require(single.velocities[0]==100,"one velocity defaults to 100");
    p[a125::FirstNote]=60./127.;
    p[a125::LastNote]=62./127.;
    p[a125::VelocityLayers]=1.;
    p[a125::Velocity1]=39./126.;
    p[a125::Velocity2]=89./126.;
    p[a125::Velocity3]=126./126.;
    auto c=a125::makeJobFromParameters(p,48000.);
    require(c.firstNote==60 && c.lastNote==62,"dynamic note range");
    require(c.velocityLayers==3 && c.roundRobins==8,"3 layers / 8 RR");
    require(c.velocities[0]==40 && c.velocities[1]==90 && c.velocities[2]==127,
        "1..127 velocity settings mapped and ascending");
    require(c.takeCount()==72,"3 notes * 3 velocities * 8 RR");
    require(a125::jobTake(c,0).velocity==40 &&
            a125::jobTake(c,8).velocity==90 &&
            a125::jobTake(c,16).velocity==127,
            "velocity mapping into emitted takes");
    require(c.holdFrames==48000 && c.tailFrames==96000 &&
            c.gapFrames==4800,"timings scale to sample rate");
    p[a125::VelocityLayers]=0.;
    c=a125::makeJobFromParameters(p,44100.);
    require(c.velocityLayers==1 && c.velocities[0]==40,
            "single layer uses explicitly configured velocity 1");
    p[a125::VelocityLayers]=.5;
    c=a125::makeJobFromParameters(p,44100.);
    require(c.velocityLayers==2 && c.velocities[0]==40 &&
            c.velocities[1]==90,"two custom velocities");
}

void testJobSchedule() {
    a125::JobConfig cfg;
    cfg.firstNote=48; cfg.lastNote=52; cfg.semitoneStep=2;
    cfg.roundRobins=8; cfg.velocityLayers=3;
    cfg.velocities={40,80,120};
    cfg.holdFrames=11; cfg.tailFrames=7; cfg.gapFrames=5;
    require(cfg.valid(), "job configuration must be valid");
    require(cfg.noteCount()==3, "C3 D3 E3 notes");
    require(cfg.takeCount()==72, "3 notes * 3 velocities * 8 RR");
    for(std::size_t i=0;i<cfg.takeCount();++i) {
        const auto t=a125::jobTake(cfg,i);
        const auto r=a125::locateJobFrame(cfg,t.start);
        require(r.has_value() && r->take.takeIndex==i &&
                r->take.note==t.note && r->take.velocity==t.velocity &&
                r->take.roundRobin==t.roundRobin && r->offset==0,
                "job start must resolve to its exact note/velocity/RR");
        const auto off=a125::locateJobFrame(cfg,t.noteOff);
        require(off && off->take.takeIndex==i && off->offset==cfg.holdFrames,
                "release must start exactly at note-off");
        const auto last=a125::locateJobFrame(cfg,t.captureEnd-1);
        require(last && last->last, "last frame must close this take");
        require(!a125::locateJobFrame(cfg,t.captureEnd),
                "inter-take gap must never be recorded");
    }
    require(!a125::locateJobFrame(cfg,
        static_cast<std::uint64_t>(cfg.takeCount())*cfg.period()),
        "end-of-job must not generate a phantom take");
    require(a125::jobTake(cfg,0).roundRobin==1 &&
            a125::jobTake(cfg,7).roundRobin==8 &&
            a125::jobTake(cfg,8).velocity==80 &&
            a125::jobTake(cfg,24).note==50,
            "RR / velocity / note ordering");
    cfg.firstNote=88;cfg.lastNote=80;
    require(!cfg.valid() && cfg.takeCount()==0, "invalid note range");
    cfg.firstNote=60; cfg.lastNote=60; cfg.velocityLayers=4;
    require(!cfg.valid(), "reject more than 3 velocity layers");
}
void testAutomaticJobEndBoundary() {
    a125::JobConfig c;
    c.firstNote=60;c.lastNote=61;
    c.velocityLayers=3;c.roundRobins=8;
    c.holdFrames=11;c.tailFrames=7;c.gapFrames=5;
    require(c.takeCount()==48,"finish plan contains all 48 takes");
    const auto last=a125::jobTake(c,c.takeCount()-1);
    const auto captureEnd=a125::jobCaptureEnd(c);
    require(captureEnd==last.captureEnd,
            "job end is final release sample, not trailing pause");
    const std::int64_t anchor=1000000;
    for(const auto block:std::array<std::int64_t,4>{
        anchor+static_cast<std::int64_t>(captureEnd)-4,
        anchor+static_cast<std::int64_t>(captureEnd)-1,
        anchor+static_cast<std::int64_t>(captureEnd)-64,
        anchor-1}) {
        require(!a125::jobCompleteAtBlockStart(c,anchor,block),
                "generator must not disarm before last sample reaches the recorder");
    }
    require(a125::jobCompleteAtBlockStart(
        c,anchor,anchor+static_cast<std::int64_t>(captureEnd)),
        "at the next block boundary the release capture is complete");
    require(a125::jobCompleteAtBlockStart(
        c,anchor,anchor+static_cast<std::int64_t>(captureEnd)+512),
        "late DAW callback still completes");
    // Negative song positions are permitted by the VST3 process context.
    const std::int64_t negativeAnchor=-10000;
    require(!a125::jobCompleteAtBlockStart(
        c,negativeAnchor,negativeAnchor+static_cast<std::int64_t>(captureEnd)-1),
        "negative project position must not complete early");
    require(a125::jobCompleteAtBlockStart(
        c,negativeAnchor,negativeAnchor+static_cast<std::int64_t>(captureEnd)),
        "negative DAW start supports sample-accurate completion");
    c.firstNote=99;c.lastNote=98;
    require(!a125::jobCompleteAtBlockStart(c,anchor,anchor+500000),
            "invalid plan must not auto-complete");
}

void testJobBlockEdgeCases() {
    a125::JobConfig cfg; cfg.roundRobins=1;
    cfg.holdFrames=10;cfg.tailFrames=20;cfg.gapFrames=4;
    const auto t=a125::jobTake(cfg,0);
    for(std::uint64_t frame=0;frame<34;++frame){
        const auto where=a125::locateJobFrame(cfg,frame);
        require(bool(where)==(frame<30),"capture boundary precise");
    }
    require(t.noteOff==10 && t.captureEnd==30,"correct NoteOn/Off/End edges");
}

void testRunNeedsExplicitRearm() {
    a125::RunGate gate;
    require(!gate.observe(false,true), "Run Off cannot start");
    require(gate.observe(true,true), "manual Run On starts exactly once");
    require(!gate.observe(true,true), "steady Run On must not start twice");
    require(!gate.observe(true,false), "transport stop invalidates the job");
    require(!gate.observe(true,true), "pressing DAW Play must not restart WAV export");
    gate.disarm();
    require(!gate.observe(true,true), "reactivated VST3 must not auto-start");
    require(!gate.observe(false,true), "Run Off only rearms");
    require(gate.observe(true,true), "subsequent user OFF->ON starts a new job");
    require(!gate.observe(true,true), "job never restarts without OFF");
    require(!gate.observe(false,false), "Off while stopped rearms");
    require(!gate.observe(true,false), "Run On while transport stopped cannot start");
    require(!gate.observe(true,true), "transport Play without rearm must not start");
    require(!gate.observe(false,true), "user OFF rearms once more");
    require(gate.observe(true,true), "explicit user START works after transport resumes");
}

void testStopAndJumpNoteRecovery() {
    a125::ActiveNote tracker;
    require(!tracker.pending(), "no note before first MIDI event");
    tracker.acceptedOn(60,101);
    require(tracker.pending() && tracker.pending()->pitch==60 &&
            tracker.pending()->noteId==101, "note identity retained");
    tracker.acceptedOff(100);
    require(tracker.pending().has_value(), "wrong note ID must not clear sounding note");
    // A loop or transport jump no longer relies on playhead position.
    tracker.acceptedOff(101);
    require(!tracker.pending(), "matching Note-Off clears active voice");
    tracker.acceptedOn(74,500);
    tracker.reset();
    require(!tracker.pending(), "deactivation clears stale MIDI state");
}

void testCaptureIntegrityFailClosed() {
    a125::TakeIntegrity gate;
    for(std::uint64_t offset=0;offset<17;++offset) {
        const auto d=gate.consume({1,0,offset,17,offset==16});
        require(d.acceptFrame, "contiguous capture should be accepted");
        require(d.completed==(offset==16), "only complete take is valid");
        require(!d.rejected && !d.abandonedPrevious, "good take never rejected");
    }
    require(!gate.abandon(), "completed take has no residual state");
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,4,false});
        const auto d=g.consume({1,0,2,4,false});
        require(!d.acceptFrame && !d.completed, "missing middle frame invalidates take");
        const auto end=g.consume({1,0,3,4,true});
        require(end.rejected && !end.completed, "incomplete take cannot be WAV");
    }
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,2,false});
        const auto d=g.consume({1,1,0,2,false});
        require(d.abandonedPrevious, "missing final frame must be counted");
        const auto end=g.consume({1,1,1,2,true});
        require(end.completed, "next independent take can complete");
    }
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,3,false});
        auto d=g.consume({1,0,1,3,true});
        require(d.rejected && !d.completed, "premature last marker");
    }
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,3,false});
        auto d=g.consume({1,0,1,4,false});
        require(!d.acceptFrame, "changed capture duration rejects sample");
        d=g.consume({1,0,2,3,true});
        require(d.rejected, "changed duration cannot export");
    }
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,3,false});
        const auto d=g.consume({2,0,0,1,true});
        require(d.abandonedPrevious && d.completed,
                "session restart must not finish old capture");
    }
    {
        a125::TakeIntegrity g;
        g.consume({1,0,0,100,false});
        require(g.abandon(), "shutdown must detect unfinished capture");
        require(!g.abandon(), "do not double count interruption");
    }
    {
        a125::TakeIntegrity g;
        auto d=g.consume({1,0,0,0,true});
        require(d.rejected && !d.completed, "zero-length take is invalid");
    }
}

void testAutomaticBatch48Wavs() {
    a125::JobConfig cfg;
    cfg.firstNote=60;cfg.lastNote=61;cfg.semitoneStep=1;
    cfg.velocityLayers=3;cfg.velocities={40,80,120};cfg.roundRobins=8;
    cfg.holdFrames=80;cfg.tailFrames=120;cfg.gapFrames=15;
    const auto root=std::filesystem::temp_directory_path()/"125a_p003_batch_regression";
    std::filesystem::remove_all(root);
    std::filesystem::create_directory(root);
    std::size_t files=0;
    try {
        std::vector<a125::StereoFrame> current;
        std::size_t active=0;
        for(std::uint64_t frame=0;
            frame<static_cast<std::uint64_t>(cfg.takeCount())*cfg.period();++frame) {
            const auto capture=a125::locateJobFrame(cfg,frame);
            if(!capture)continue;
            const auto& take=capture->take;
            if(capture->offset==0) {
                current.clear();active=take.takeIndex;
            }
            require(active==take.takeIndex,"do not overlap RR takes");
            const float level=static_cast<float>(take.velocity)/127.f;
            const float release=capture->offset<cfg.holdFrames?1.f:
                1.f-static_cast<float>(capture->offset-cfg.holdFrames)/
                      static_cast<float>(cfg.tailFrames);
            const float sample=level*release;
            current.push_back({sample,-sample*.5f});
            if(capture->last) {
                const std::string filename="N"+std::to_string(take.note)+
                  "_V"+std::to_string(take.velocity)+"_RR"+
                  (take.roundRobin<10?"0":"")+std::to_string(take.roundRobin)+".wav";
                const auto processed=a125::trimAndFade(current,{0.0001f,0,0,4});
                require(!processed.empty(),"non-silent take should export");
                const auto path=root/filename;
                require(!std::filesystem::exists(path),"no duplicate RR filenames");
                a125::writeWav24(path.string(),44100,processed);
                require(std::filesystem::file_size(path)==44+processed.size()*6,
                        "truncated 24-bit WAV frame count");
                ++files;
            }
        }
        require(files==48,"2 MIDI notes * 3 velocity layers * 8 RR = 48 WAVs");
        std::size_t count=0;
        for(const auto& entry:std::filesystem::directory_iterator(root))
            if(entry.path().extension()==".wav")++count;
        require(count==48,"48 unique WAVs physically present");
    } catch(...) {
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::remove_all(root);
}

void testWavAndTrim() {
    std::vector<a125::StereoFrame> samples(32,{0.f,0.f});
    for(int i=8;i<24;++i) samples[static_cast<std::size_t>(i)]={0.5f,-0.25f};
    const auto processed=a125::trimAndFade(samples,{0.01f,2,3,2});
    require(processed.size()==21,"expected 2 preroll, 16 sound, 3 postroll");
    require(processed.front()[0]==0.f && processed.back()[0]==0.f,"no clicks at endpoints");
    const auto file=std::filesystem::temp_directory_path()/"125a_core_test.wav";
    a125::writeWav24(file.string(),44100,processed);
    std::ifstream in(file,std::ios::binary);
    const std::vector<char> data((std::istreambuf_iterator<char>(in)),{});
    require(data.size()==44+processed.size()*6,"invalid WAV PCM length");
    require(std::string(data.data(),4)=="RIFF","missing RIFF");
    require(std::string(data.data()+8,4)=="WAVE","missing WAVE");
    require(std::string(data.data()+36,4)=="data","missing data");
    in.close(); // Windows prevents deleting a file held by an open reader.
    std::filesystem::remove(file);
    require(a125::trimAndFade(std::vector<a125::StereoFrame>(16,{0.f,0.f}),{}).empty(),
            "silence should not create a phantom WAV");
}
} // namespace
int main() {
    try {
        testEightRoundRobins();
        testSchedulerSampleOffsets();
        testValidation();
        testAllMidiVelocityRoundTrips();
        testEditableVelocitiesAndRanges();
        testJobSchedule();
        testJobBlockEdgeCases();
        testAutomaticJobEndBoundary();
        testRunNeedsExplicitRearm();
        testStopAndJumpNoteRecovery();
        testCaptureIntegrityFailClosed();
        testAutomaticBatch48Wavs();
        testWavAndTrim();
        std::cout << "P001 core tests PASSED\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "P001 core tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
