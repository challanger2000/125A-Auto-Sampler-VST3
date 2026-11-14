# P013 – 125A AutoSampler ist selbst das Instrument

## Korrektur der Produktarchitektur

Der Nutzer benötigt **keinen SFZ-Player**. Die P011-SFZ-Exportrichtung war ein unnötiger Umweg; der automatische SFZ-Export gehört nicht mehr zum Aufnahmeablauf. Die 125A-VST3 liest ihre WAVs selbst.

- **125A AutoSampler** (Instrument, gleicher VST3-Klassen-Identifier wie zuvor): erzeugt den MIDI-Sampling-Plan, besitzt einen MIDI-Eingang für normales Spielen und liefert dessen Klänge über einen echten Stereo-Audioausgang.
- **125A AutoSampler Recorder** (Effekt im selben Bundle): bleibt am Audio-Ausgang des externen Quell-VSTs, um die Capture-WAVs zu erstellen. Ein fremder Sampler ist nicht beteiligt.
- **LOAD SAMPLES:** Ein gespeicherter 125A-Run-Ordner wird direkt als eigene WAV-Bank geladen. Nach erfolgreicher Aufnahme wird der neue Bankordner bei geöffnetem 125A-Editor automatisch geladen; beim nächsten Öffnen wird eine zuvor abgeschlossene Aufnahme nachgeladen. Der DAW-Projektzustand speichert den Bankpfad, nicht die PCM-Daten selbst.

## Implementierte Sample-Engine

`src/core/BuiltInSampler.h` dekodiert die vom Recorder geschriebenen Stereo-PCM24-WAVs, liest Note/Velocity/RR aus `N60_V100_RR01.wav` und verwaltet bis zu 32 aktive Stimmen. MIDI wählt die nächste aufgenommene Root-Note, die passende Velocity-Schicht und zyklische Round-Robin-Samples. Fehlende Zwischentöne werden mit linearer Interpolation der Tonhöhe angepasst; die WAVs werden als One-Shots inklusive aufgenommenem Release gespielt.

Audiocallbacks lesen keine WAV-Dateien, erzeugen keine Ordner und allozieren keinen Heap. Laden und Projekt-Wiederherstellung erfolgen außerhalb des Echtzeit-Renderings. Beschädigte, leere oder unvollständige WAV-Bänke werden verweigert.

## Belegte Windows-Abnahme

**[GitHub Actions – P013 Windows-VST3-Lauf #43542007225: SUCCESS](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43542007225)**

Geprüfter Code-Commit: `cd43da6cbfdeb6ffd4d71d102fc8b10f2098a80a`.

- Windows-x64-VST3-Bundle, VSTGUI-Ordner-/Bedienelemente, offizieller Steinberg-Validator, Core-Regressionsprüfungen: **PASS**.
- Die echten VST3-Komponenten erzeugen **72/72** hörbare Stereo-PCM24-WAVs (8 + 8 + 48 + 8) unter unterschiedlichen Samplerates und Buffergrößen: **PASS**.
- Der native Windows-Test-Host lädt exakt diese WAVs anschließend in die eigentliche **125A AutoSampler**-Instrumentenklasse, speichert/lädt deren Projektzustand und sendet ihr MIDI-Note-On. Die Klasse rendert **hörbare Stereo-PCM-Daten** in sämtlichen vier Tests (63, 1018, 507 und 253 nichtstumme Frames pro Testblock): **PASS**.
- Round-Robin-/Velocity-Auswahl, Pitch-Transpose und beschädigte-Bank-Ablehnung in den Core-Tests: **PASS**.
- STOP während eines Takes: richtiges MIDI-Note-Off, keine kaputte Teil-WAV: **PASS**.
- Artefakt im Actions-Lauf: `125A-AutoSampler-P013-Win64`.

## Nicht als fertig ausgegeben

Ein realer End-to-End-Durchlauf mit einem Drittanbieter-VST in **Fender Studio Pro** und physischem ASIO-Gerät ist weiterhin nicht durchgeführt. Der integrierte Sampler spielt derzeit One-Shots und besitzt keine Sustain-Loop-Engine; die einfache lineare Tonhöheninterpolation ist noch nicht als endgültige Qualitätsstufe beurteilt. Der vollständige GUI-Klicktest und die automatische Bankübernahme über die Editor-Timer-Logik in der Ziel-DAW sind noch nicht praktisch geprüft. Nur ein MIDI-Generator-/Recorder-Paar je Host-Prozess wird unterstützt.

**P013 ist eine erfolgreiche technische Windows-Abnahme, keine Release-Freigabe.**
