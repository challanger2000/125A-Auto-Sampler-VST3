# P001: Aufnahmeplan, MIDI-Zeitleiste, WAV-Export

Die erste Implementierung enthält **nur den portablen Kern** und Windows-CI-Tests.

Bereits enthalten:
- Deterministischer MIDI-Ablauf für Notenbereiche, Velocity-Layer und 1–16 Round-Robin-Takes.
- Sample-genaue Note-On/Note-Off-Offets je DAW-Audioblock ohne Heap-Allokationen.
- Offline-Silence-Trimming mit Vor-/Nachlauf und kurzen De-Click-Fades.
- Stereo-24-Bit-WAV-Ausgabe; Originalpegel bleiben erhalten.
- Regressionsprüfungen für 8 RR × 3 Velocities, Event-Offsets, ungültige Konfigurationen, Stille und WAV-Header.

Noch **nicht** enthalten:
- VST3-MIDI-Generator und Recorder als Host-kompatible Plugins.
- Verifizierte Studio-Pro-8.1.2-MIDI-/Audio-Routingstrecke.
- Die Synchronisierung zwischen zwei Plugininstanzen.
- Echtzeitsicherer Ringpuffer und Worker-Thread für WAV-Dateien.
- Preset/GUI/State-Recall; automatischer Transportstart; Integration ins 125A Plugin Test Lab.

**Nicht als fertigen AutoSampler oder getestetes VST3 ausgeben.**
