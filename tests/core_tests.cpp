#include "core/SamplingPlan.h"
#include "core/SampleProcessing.h"
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
        testWavAndTrim();
        std::cout << "P001 core tests PASSED\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "P001 core tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
