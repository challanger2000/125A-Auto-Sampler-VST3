# 125A Auto Sampler VST3

Windows-x64-VST3 für automatisches Sampling von Instrumenten in **Fender Studio Pro**. Das Quell-VST wird **ausschließlich von der DAW gehostet**, nicht vom AutoSampler.

## P004 – Entwicklerstand

**Zwei Plugin-Klassen im selben VST3-Bundle:**
- `125A AutoSampler MIDI`: autonomer MIDI-Notengenerator mit eigenem dunklem VSTGUI-Bedienfeld
- `125A AutoSampler Recorder`: Audio-Insert für WAV-Aufnahmen

Einstellbar sind: Notenbereich, Notenabstand (Halbtöne), 1–16 Round Robins, 1–3 Velocity-Layer mit **individuell einstellbaren MIDI-Velocities 1–127**, Notenlänge, Release-Aufnahmezeit und Pausenlänge.

Der Recorder erzeugt getrennte **Stereo-24-Bit-WAV-Dateien**, erkennt Stille, trimmt die Samples und fügt Fades hinzu. WAV-Export erfolgt auf einem Hintergrundthread; die Aufnahme-Daten kommen aus einem lockfreien Ringpuffer.

Ein Aufnahmelauf erhält einen eigenen Ordner, damit keine alten Samples überschrieben werden. Bei Projekt-Reopen bleiben Parameter erhalten, aber **Run startet nicht automatisch**.

## Teststatus

- Windows-x64-VST3-Build mit offiziellem Steinberg-SDK.
- Steinberg-Validator prüft beide Plugin-Klassen.
- C++-Tests prüfen Zeitplan, Note-On/Off, Release-Ende, alle 127 Velocity-Werte und den simulierten Export von **48 eindeutigen WAV-Dateien** (2 Noten × 3 Velocity × 8 RR).
- Die erste dunkle VSTGUI-Oberfläche ist im Quellcode. **Ihre korrekte Darstellung in Fender Studio Pro ist noch nicht praktisch geprüft.**
- **Offen:** MIDI-Routing und echte Aufnahme eines Quell-VSTs in Fender Studio Pro; eigener Recorder-Status, Plugin-Test-Lab-Multi-Class-Test und abschließende UI-/ASIO-Praxisabnahme.

Ausführlicher Ablauf: [START-HERE.md](START-HERE.md). Entwicklungs-PR: [#2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2).

**P004 ist ein Entwicklungsprototyp, keine vollständige Release-Freigabe.**
