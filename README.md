# 125A Digital Bass VST3

## P001 — digital bass prototype

Purpose: a small, playable **Windows x64 VST3 bass synthesizer** designed
for gritty synth-pop/EBM basses with velocity-controlled tone.

**P001 is not an EVM Isotope emulator.** The provided Wave Dancer 2 recording,
original VST2 DLL and DAT are reference material; the synthesis formulas in
P001 are provisional and independently written. No claim of identical sound.

### Implemented

- Monophonic MIDI note-on/off with sample-offset dispatch.
- Digital phase-warp oscillator, oscillator cross-modulation and sub oscillator.
- Resonant 2-pole low-pass, filter contour, velocity-to-timbre.
- 9 DAW-automatable parameters: Shape, Cross Mod, Cutoff, Resonance,
  Filter Env, Velocity Color, Decay, Release, Output.
- Stereo output (same audio on both channels), float32 and float64.
- Host generic parameter editor; state save/restore.
- Zero dynamic allocation in the audio processing callback.
- Independent C++23 DSP unit test; Windows-2022 VST3 build on GitHub Actions.

### Build from source

GitHub Actions checks out Steinberg VST3 SDK 3.8.1 at commit
3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 recursively into
third_party/vst3sdk, then executes:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

Build artifacts will be visible under Actions after a successful run.
Without the SDK, CMake builds the independent DSP tests only.

### Deliberate P001 limitations

- No custom GUI and no wavetable bank/step sequencer.
- Block-final-point automation (not yet sample-accurate).
- No anti-aliasing/oversampling yet.
- Monophonic last-note retrigger without note priority or legato stack.
- Reference-timbre and Studio One/ASIO acceptance tests are not yet done.

### Follow-on technical work

1. Decode the exact Wave Dancer 2 parameter-to-DSP connections in EVM Isotope.
2. Use matched MIDI test notes to compare envelope and spectral changes.
3. Improve only the oscillator/filter modules required by those measurements.
4. Add a simple final GUI once the audio engine is satisfactory.

Do not publish the user-provided reference files in this public repository.
