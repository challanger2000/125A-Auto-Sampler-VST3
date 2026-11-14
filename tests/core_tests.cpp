#include "core/SamplingPlan.h"
#include "core/SampleProcessing.h"
#include "core/JobSchedule.h"
#include "core/JobParameters.h"
#include "core/CaptureIntegrity.h"
#include "core/ActiveNote.h"
#include "core/RunGate.h"
#include "core/ExportTarget.h"
#include "core/SfzBank.h"
#include "core/BuiltInSampler.h"
#include "core/InstrumentArchive.h"
#include "core/RunStatus.h"
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
    require(a125::makeJobFromParameters(p,44100.).takeCount()==1,
        "quick default is one note, one RR and one velocity");
    auto single=a125::makeJobFromParameters(p,44100.);
    require(single.velocities[0]==100,"one velocity defaults to 100");
    // Explicitly opt into the long 8-RR and dense test matrix.
    p[a125::RoundRobins]=7./15.;
    p[a125::NoteStep]=0.;
    p[a125::Hold]=(1.-.25)/7.75;
    p[a125::Release]=(2.-.25)/9.75;
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

void testExportTargetSelection() {
    a125::ExportTarget target;
    const auto fallback=std::filesystem::path("Default Output");
    require(a125::ExportTarget::resolve(target.chosenFolder(),fallback)==fallback,
            "empty selection uses default destination");
    const auto first=std::filesystem::path(u8"C:/Music Projects/Sample Output");
    target.choose(first);
    require(target.chosenFolder()==first,"custom destination survives snapshot");
    require(a125::ExportTarget::resolve(target.chosenFolder(),fallback)==first,
            "custom destination overrides fallback");
    target.choose({});
    require(target.chosenFolder().empty() &&
            a125::ExportTarget::resolve(target.chosenFolder(),fallback)==fallback,
            "blank preference restores default");
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

void testRunDisplayTruthfulness() {
    a125::RunDisplaySnapshot s;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Ready,
            "fresh plugin starts ready");
    s.generation=4;s.expected=8;s.recordingAnchorActive=true;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Recording,
            "running job must display recording");
    s.recordingAnchorActive=false;s.captureEndedGeneration=4;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::MissingRecorder,
            "missing audio Recorder must not say FINALIZING forever");
    s.observed=8;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Finalizing,
            "recorded final frame waits for writer");
    s.writerFinishedGeneration=4;s.written=8;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Complete,
            "fully exported samples and SFZ can be COMPLETE");
    s.errors=1;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Partial,
            "SFZ file error must not display COMPLETE");
    s.errors=0;s.silent=1;s.written=7;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Partial,
            "one missing velocity/RR WAV must display PARTIAL");
    s.written=0;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Failed,
            "no successful WAVs must display FAILED");
    s.interruptedGeneration=4;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Aborted,
            "manual Stop must display ABORTED, even with prior successful WAVs");
    s.writerFinishedGeneration=0;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Aborted,
            "abort UI must not wait for a missing file writer");
    s.generation=5;s.interruptedGeneration=4;s.captureEndedGeneration=4;
    require(a125::runDisplayStatus(s)==a125::RunDisplayStatus::Ready,
            "previous run must not contaminate next run's status");
}

void testNativeSamplerPlayback() {
    namespace fs=std::filesystem;
    const auto dir=fs::temp_directory_path()/"125a_p013_native_sampler";
    fs::remove_all(dir);
    fs::create_directories(dir);
    try {
        const std::vector<a125::StereoFrame> low(128,{.10f,.05f});
        const std::vector<a125::StereoFrame> high(128,{.40f,.20f});
        const std::vector<a125::StereoFrame> brighter(128,{.70f,.35f});
        a125::writeWav24((dir/"N60_V40_RR01.wav").string(),44100,low);
        a125::writeWav24((dir/"N60_V40_RR02.wav").string(),44100,high);
        a125::writeWav24((dir/"N60_V120_RR01.wav").string(),44100,brighter);
        require(([&]{int n=0,v=0,rr=0;return a125::parseSampleName("N127_V100_RR16.wav",n,v,rr)&&n==127&&v==100&&rr==16;}()),
                "capture file name 127/100/16");
        auto bank=a125::loadCapturedBank(dir);
        require(bank && bank->zones.size()==3,"load all three recorded WAV zones");
        a125::BuiltInSampler player;
        require(player.publish(std::move(bank)) && player.loadedZones()==3,
                "native sampler must own the captured PCM without a SFZ player");
        player.noteOn(60,40,44100.);
        auto a=player.renderOne();
        require(std::abs(a[0]-.10f)<.002f && std::abs(a[1]-.05f)<.002f,
                "first RR voice plays recorded stereo PCM");
        for(int i=0;i<200;++i)player.renderOne();
        player.noteOn(60,40,44100.);
        a=player.renderOne();
        require(std::abs(a[0]-.40f)<.002f,
                "second hit must advance the built-in RR selector");
        for(int i=0;i<200;++i)player.renderOne();
        player.noteOn(60,120,44100.);
        a=player.renderOne();
        require(std::abs(a[0]-.70f)<.002f,
                "higher velocity must select its OWN WAV layer");
        for(int i=0;i<200;++i)player.renderOne();
        player.noteOn(72,40,44100.);
        a=player.renderOne();
        require(std::abs(a[0]-.10f)<.002f,
                "nearest root key is pitched up and still audible");
        for(int i=0;i<200;++i)player.renderOne();
        // Deliberately break one RR WAV: incomplete banks are rejected.
        fs::resize_file(dir/"N60_V40_RR02.wav",30);
        require(!a125::loadCapturedBank(dir),
                "damaged WAV must NEVER be loaded as playable instrument");
    }catch(...){fs::remove_all(dir);throw;}
    fs::remove_all(dir);
}

void testP015DynamicsAndGate() {
    namespace fs=std::filesystem;
    const auto dir=fs::temp_directory_path()/"125a_p015_dynamics_gate";
    fs::remove_all(dir);
    fs::create_directories(dir);
    try {
        const std::vector<a125::StereoFrame> quiet(1024,{.2f,-.1f});
        const std::vector<a125::StereoFrame> loud(1024,{.8f,-.4f});
        a125::writeWav24((dir/"N60_V40_RR01.wav").string(),44100,quiet);
        a125::writeWav24((dir/"N60_V120_RR01.wav").string(),44100,loud);
        auto bank=a125::loadCapturedBank(dir);
        require(bank && bank->zones.size()==2,"P015 loads two velocity layers");
        const float scale=bank->normalizeGain;
        require(std::abs(.8f*scale-std::pow(10.f,-1.f/20.f))<.002f,
            "P015 bank-wide normalize aims at -1 dBFS");
        a125::BuiltInSampler player;
        require(player.publish(std::move(bank)),"P015 publish bank");
        player.setNormalize(true);
        player.noteOn(60,40,44100.);
        auto a=player.renderOne();
        require(std::abs(a[0]-.2f*scale)<.002f,
            "P015 normalized quiet layer must keep its relative level");
        for(int i=0;i<1100;++i)player.renderOne();
        player.noteOn(60,120,44100.);
        a=player.renderOne();
        require(std::abs(a[0]-.8f*scale)<.002f,
            "P015 normalized loud layer must preserve velocity dynamics");
        for(int i=0;i<1100;++i)player.renderOne();

        // One-shot ignores Note-Off, even if it happens right after attack.
        player.setNormalize(false);
        player.setGate(false);
        player.setEnvelope(0.f,0.f,1.f,.002f);
        player.noteOn(60,120,44100.);
        player.noteOff(60);
        for(int i=0;i<100;++i)a=player.renderOne();
        require(std::abs(a[0]-.8f)<.002f,
            "P015 One-Shot must continue after MIDI Note-Off");
        for(int i=0;i<1100;++i)player.renderOne();

        // Gate starts a continuous, sample-rate-correct release.
        player.setGate(true);
        player.noteOn(60,120,44100.);
        a=player.renderOne();
        require(a[0]>.79f,"P015 Gate starts at captured loudness");
        player.noteOff(60);
        for(int i=0;i<45;++i)a=player.renderOne();
        require(a[0]>.35f && a[0]<.5f,"P015 release reaches halfway");
        for(int i=0;i<80;++i)a=player.renderOne();
        require(std::abs(a[0])<.0001f,"P015 Gate release reaches silence");

        // A non-zero attack must ramp from zero, never click at full level.
        player.setEnvelope(.004f,0.f,1.f,.002f);
        player.noteOn(60,120,44100.);
        const auto first=player.renderOne();
        for(int i=0;i<176;++i)a=player.renderOne();
        require(first[0]>.0001f && first[0]<.02f,
            "P015 amp attack must ramp from a low initial level");
        require(a[0]>.79f,"P015 amp attack reaches full level");
        std::cout<<"[PASS] P015 bank-wide normalize and One-Shot/Gate ADSR\n";
    }catch(...){fs::remove_all(dir);throw;}
    fs::remove_all(dir);
}

void testP016NormalizePolyphonicHeadroom() {
    namespace fs=std::filesystem;
    const auto dir=fs::temp_directory_path()/"125a_p016_polyphonic_normalize";
    fs::remove_all(dir);
    fs::create_directories(dir);
    try {
        // A full-scale-ish sustained waveform exposes the previous hard
        // clipper immediately: 2 voices at -1 dBFS would sum to +5 dBFS.
        const std::vector<a125::StereoFrame> sample(512,{.8f,-.8f});
        a125::writeWav24((dir/"N60_V100_RR01.wav").string(),44100,sample);
        auto bank=a125::loadCapturedBank(dir);
        require(bank && bank->zones.size()==1,"P016 load mono-root stereo WAV");
        a125::BuiltInSampler player;
        require(player.publish(std::move(bank)),"P016 publish WAV bank");
        player.setNormalize(true);
        player.noteOn(60,100,44100.);
        auto a=player.renderOne();
        const float target=std::pow(10.f,-1.f/20.f);
        require(std::abs(a[0]-target)<.002f,
            "P016 single normalized sample must retain -1 dBFS target");
        // Overlap 8 one-shot notes without Note-Off; no hard clip or
        // intermodulation from a saturating output stage is acceptable.
        for(int i=0;i<7;++i)player.noteOn(60,100,44100.);
        for(int frame=0;frame<200;++frame) {
            a=player.renderOne();
            require(std::isfinite(a[0]) && std::isfinite(a[1]) &&
                    std::abs(a[0])<=target+.002f &&
                    std::abs(a[1])<=target+.002f,
                "P016 normalized 8-voice sum must not overload or clip");
        }
        require(std::abs(a[0]-target)<.003f,
            "P016 normalized 8-voice sum must retain musical level");
        // New note after silence must recover the level of a single note.
        for(int i=0;i<600;++i)player.renderOne();
        player.noteOn(60,100,44100.);
        a=player.renderOne();
        require(std::abs(a[0]-target)<.003f,
            "P016 normalize headroom must reset after note tails end");
        // Without Normalize, the sampler must not destroy float peaks
        // by hard-clipping: the DAW may apply a downstream linear gain.
        for(int i=0;i<600;++i)player.renderOne();
        player.setNormalize(false);
        player.noteOn(60,100,44100.);
        player.noteOn(60,100,44100.);
        a=player.renderOne();
        require(a[0]>1.55f && a[1]<-1.55f,
            "P016 float playback must preserve unclipped multi-voice peaks");
        std::cout<<"[PASS] P016 Normalize: safe 8-voice sum, 1-voice recovery, unclipped float bus\\n";
    }catch(...){fs::remove_all(dir);throw;}
    fs::remove_all(dir);
}

void testP017PortableArchive() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/"125a_p017_portable_archive";
    fs::remove_all(root);
    fs::create_directories(root/"takes");
    try {
        std::vector<a125::StereoFrame> wave(512,{.65f,-.4f});
        a125::writeWav24((root/"takes"/"N60_V100_RR01.wav").string(),44100,wave);
        a125::writeWav24((root/"takes"/"N72_V100_RR01.wav").string(),44100,wave);
        const auto archive=root/"Vintage Bass.125A";
        a125::ArchivePlayback settings;
        settings.params={1.,0.,.25,.5,.75,.1,.8};
        a125::write125AArchive(root/"takes",archive,"Vintage \"Bass\"",settings);
        require(fs::exists(archive),"P017 archive file is missing");
        require(throws([&]{a125::write125AArchive(root/"takes",archive,"Duplicate");}),
            "P017 must not overwrite a portable instrument archive");
        std::string name;
        a125::ArchivePlayback restoredSettings;
        auto bank=a125::load125AArchive(archive,&name,&restoredSettings);
        require(bank && bank->zones.size()==2 && name=="Vintage \"Bass\"",
            "P017 ZIP must round-trip escaped instrument name and sample count");
        require(std::abs(restoredSettings.params[2]-.25)<.00001 &&
                std::abs(restoredSettings.params[4]-.75)<.00001 &&
                std::abs(restoredSettings.params[6]-.8)<.00001,
            "P017 self-contained archive must preserve playback settings");
        require(a125::safe125AStem("Vintage/Bad:*?Name")=="Vintage_Bad___Name",
            "P017 archive filenames must sanitize Windows reserved characters");
        require(bank->zones[0].root==60 && bank->zones[1].root==72 &&
                bank->zones[0].pcm.size()==wave.size(),
            "P017 source WAVs must retain root mapping and frame count");
        require(std::abs(bank->zones[0].pcm[13][0]-.65f)<.00001f,
            "P017 archive PCM must be bit-preserving through ZIP STORE");
        a125::BuiltInSampler player;
        require(player.publish(std::move(bank)),"P017 archive bank publication");
        player.noteOn(60,100,44100.);
        auto frame=player.renderOne();
        require(std::abs(frame[0]-.65f)<.00001f,
            "P017 loaded archive must directly play as 125A instrument");
        // A single corrupt PCM byte is caught by the ZIP CRC, rather than
        // being loaded as a changed sample.
        {
            std::fstream io(archive,std::ios::binary|std::ios::in|std::ios::out);
            require(static_cast<bool>(io),"P017 cannot open ZIP for fault injection");
            io.seekg(45);char c=0;io.read(&c,1);
            require(static_cast<bool>(io),"P017 cannot read ZIP fault target");
            io.seekp(45);c^=1;io.write(&c,1);
            require(static_cast<bool>(io),"P017 cannot corrupt ZIP payload");
        }
        require(!a125::load125AArchive(archive),
            "P017 tampered archive must fail closed on manifest CRC mismatch");
        std::cout<<"[PASS] P017 .125A ZIP STORE manifest, WAV mapping, playback, CRC and no-overwrite\n";
    }catch(...){fs::remove_all(root);throw;}
    fs::remove_all(root);
}

void testSfzKeyboardBank() {
    a125::JobConfig config;
    config.firstNote=60;
    config.lastNote=61;
    config.velocityLayers=3;
    config.velocities={40,80,120};
    config.roundRobins=8;
    config.holdFrames=128;
    config.tailFrames=64;
    config.gapFrames=16;
    const auto text=a125::sfzBankText(config);
    require(text.has_value(),"valid multi-layer RR plan must create SFZ mapping");
    require(std::count(text->begin(),text->end(),'<')==54,
            "two notes x three velocity groups plus 48 SFZ regions");
    require(text->find("<group> lokey=60 hikey=60 pitch_keycenter=60 lovel=1 hivel=60 seq_length=8")!=std::string::npos,
            "velocity layer 40 covers 1-60");
    require(text->find("<group> lokey=60 hikey=60 pitch_keycenter=60 lovel=61 hivel=100 seq_length=8")!=std::string::npos,
            "velocity layer 80 covers 61-100");
    require(text->find("<group> lokey=61 hikey=61 pitch_keycenter=61 lovel=101 hivel=127 seq_length=8")!=std::string::npos,
            "velocity layer 120 covers 101-127");
    require(text->find("<region> seq_position=8 sample=N61_V120_RR08.wav")!=std::string::npos,
            "last SFZ RR sample name does not match recorder");
    for(std::size_t i=0;i<config.takeCount();++i)
        require(text->find("sample="+a125::sampleFileName(a125::jobTake(config,i)))!=
                std::string::npos,"every generated WAV must be in SFZ");
    {
        a125::JobConfig spaced=config;
        spaced.firstNote=60;
        spaced.lastNote=69;
        spaced.semitoneStep=4;
        spaced.velocityLayers=1;
        spaced.roundRobins=1;
        auto mapping=a125::sfzBankText(spaced);
        require(mapping.has_value() &&
                mapping->find("lokey=60 hikey=62 pitch_keycenter=60")!=std::string::npos &&
                mapping->find("lokey=63 hikey=66 pitch_keycenter=64")!=std::string::npos &&
                mapping->find("lokey=67 hikey=69 pitch_keycenter=68")!=std::string::npos,
                "non-chromatic note spacing must split playable ranges at midpoints");
    }
    {
        a125::JobConfig duplicate=config;
        duplicate.velocities={40,40,120};
        require(!a125::sfzBankText(duplicate),
                "duplicate velocities must not create overlapping SFZ regions");
        duplicate.firstNote=127;
        duplicate.lastNote=126;
        require(!a125::sfzBankText(duplicate),"invalid MIDI mapping must fail closed");
    }
    const auto root=std::filesystem::temp_directory_path()/"125a_p011_sfz_export";
    std::filesystem::remove_all(root);
    std::filesystem::create_directory(root);
    try {
        require(!a125::writeCompleteSfzBank(config,root),
                "missing WAV files must not publish a playable SFZ bank");
        require(!std::filesystem::exists(root/"125A AutoSampler.sfz"),
                "incomplete capture must not create SFZ");
        const std::vector<a125::StereoFrame> samples(100,{.25f,-.15f});
        for(std::size_t i=0;i<config.takeCount();++i){
            const auto path=root/a125::sampleFileName(a125::jobTake(config,i));
            a125::writeWav24(path.string(),44100,samples);
        }
        require(a125::writeCompleteSfzBank(config,root),
                "48 complete physical WAVs must produce a full playable SFZ");
        const auto sfzPath=root/"125A AutoSampler.sfz";
        std::ifstream file(sfzPath,std::ios::binary);
        const std::string generated((std::istreambuf_iterator<char>(file)),{});
        file.close();
        require(generated==*text,"on-disk SFZ mapping differs from offline model");
        require(!std::filesystem::exists(root/"125A AutoSampler.sfz.tmp"),
                "atomic SFZ publication must not leave a temporary file");
        std::filesystem::remove(sfzPath);
        std::filesystem::remove(root/a125::sampleFileName(a125::jobTake(config,47)));
        require(!a125::writeCompleteSfzBank(config,root) &&
                !std::filesystem::exists(sfzPath),
                "removing just one RR take must prevent SFZ bank creation");
    }catch(...){
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::remove_all(root);
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
        testExportTargetSelection();
        testRunNeedsExplicitRearm();
        testStopAndJumpNoteRecovery();
        testCaptureIntegrityFailClosed();
        testRunDisplayTruthfulness();
        testNativeSamplerPlayback();
        testP015DynamicsAndGate();
        testP016NormalizePolyphonicHeadroom();
        testP017PortableArchive();
        testSfzKeyboardBank();
        testAutomaticBatch48Wavs();
        testWavAndTrim();
        std::cout << "P001 core tests PASSED\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "P001 core tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
