# 125A Auto Sampler VST3

**Windows-x64-VST3 für vollautomatisches Multisampling vorhandener Instrumente in Fender Studio Pro.**

Das Quellinstrument läuft unverändert in der DAW: **kein eigener VST-Host**.

## Geplanter Funktionsumfang
- Selbstständige MIDI-Ansteuerung über einstellbaren Notenbereich und Notenlänge.
- Mehrere Velocity-Layer und konfigurierbare 1–16 Round-Robin-Wiederholungen pro Note.
- Automatische Audioaufnahme inklusive Note-Off/Release, Stilleerkennung, Schnitt und Fades.
- Getrennte, sinnvoll benannte WAV-Dateien zur freien Verwendung; **kein Sample-Mapping in andere Sampler**.

## Entwicklungsstatus
**P002 — Windows-VST3-Prototyp erfolgreich kompiliert. Noch keine DAW-Abnahme.**

Die VST3 enthält zwei Klassen (MIDI-Generator und Recorder). Der Prototyp ist zunächst auf MIDI-Note 60, Velocity 100 und acht Round-Robin-Aufnahmen fest eingestellt. Er benutzt einen lockfreien Audio-Ringpuffer und schreibt die WAV-Dateien außerhalb des Echtzeit-Audiothreads.

Die UI, die frei wählbaren Noten-/Velocity-Einstellungen, die robuste Fehleranzeige und der Fender-Studio-Pro-Praxistest sind noch offen.

**[START-HERE.md](START-HERE.md)** enthält Installation, ersten Testablauf, Einschränkungen und Abnahmekriterien.

Windows-Build: [GitHub Actions, Lauf 43519720850](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43519720850).
