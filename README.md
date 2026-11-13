# 125A Auto Sampler VST3

**Windows-x64-VST3 für automatisches Sampling von Instrumenten, die bereits in Fender Studio Pro laufen.**

Der AutoSampler hostet keine fremden Instrumente. Eine MIDI-Generator-Komponente steuert das vorhandene VST an; der Stereo-Recorder sitzt als Audio-Insert auf der Instrumentenspur. Das Ziel sind automatisch geschnittene, getrennte WAV-Samples, optional mit mehreren Velocity-Stufen und Round-Robin-Varianten.

## P003 – derzeitiger Stand

- **Windows VST3-Bundle mit zwei Klassen:** `125A AutoSampler MIDI` und `125A AutoSampler Recorder`.
- MIDI-Notenbereich 0–127, Notenabstand 1–12 Halbtöne, Round Robin 1–16.
- Velocity-Modi: 1 Layer (100), 2 Layer (60/110), 3 Layer (40/80/120). Diese Velocity-Werte sind in P003 noch fest vorgegeben.
- Einstellbare Notenlänge, Ausklingdauer und Pause über generische VST3-Parameter.
- Beide Komponenten verwenden dieselbe DAW-Sampleposition und denselben Aufnahmeplan; nur **ein** Generator-/Recorder-Paar unterstützt.
- Lockfreier Audio-Puffer; Stereo-24-Bit-PCM-WAV, Stilleerkennung/Trimming und Fades werden **außerhalb** des Audio-Callbacks verarbeitet.
- Jeder Aufnahmelauf erhält ein eigenes Unterverzeichnis, frühere WAVs werden nicht überschrieben.
- P003-Regression: **48 eindeutig benannte WAV-Dateien** aus einem simulierten 2-Noten-/3-Velocity-/8-RR-Durchlauf.
- Sampling-Parameter werden mit dem Projekt gespeichert, **Run** beim Projekt-Reload bewusst nicht.
- Windows-CI mit gepinntem offiziellen Steinberg-VST3-SDK und Steinberg-Validator.

**Kein fertiges Produkt:** Einen echten MIDI→EVM Isotope→Audio→WAV-Durchlauf in Fender Studio Pro haben wir bislang **nicht** nachgewiesen. Der VST3-Build/Validator und die WAV-Simulation ersetzen diesen Test nicht.

## Anleitung
[START-HERE.md](START-HERE.md) beschreibt den aktuellen DAW-Routing-Test, die Parameter und die Einschränkungen.

## Entwicklerzweig
`develop/p001-autosampler-foundation` · Entwurf [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2).
