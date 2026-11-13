# START HERE — 125A AutoSampler P004

## Was bereits implementiert ist

- **125A AutoSampler MIDI**: eigener dunkler GUI-Editor mit Start/Stop und Sampling-Parametern
- **125A AutoSampler Recorder**: transparenter Stereo-Insert und WAV-Export
- Kein eigener VST-Host, kein zusätzliches Sampling-Programm, kein SFZ/DirectWave-Mapping

## Installation für die spätere DAW-Abnahme

1. Einen erfolgreichen Windows-Build unter [GitHub Actions](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions) wählen und das Windows-VST3-Artefakt herunterladen.
2. Das `.vst3`-Bundle vollständig unter `C:\Program Files\Common Files\VST3\` ablegen.
3. Fender Studio Pro neu starten und beide Plugin-Klassen laden.

## MIDI-/Audio-Routing – muss noch in Fender Studio Pro geprüft werden

1. `125A AutoSampler MIDI` auf einer eigenen Instrumentenspur laden.
2. EVM Isotope oder ein anderes Quell-VST auf einer zweiten Instrumentenspur laden.
3. Die MIDI-Ausgabe der AutoSampler-Instrumentenspur als MIDI-Eingang des Quell-VSTs wählen.
4. `125A AutoSampler Recorder` als **letztes Audio-Insert** auf dem Quellinstrument laden.
5. **DAW-Transport starten**, dann im AutoSampler-MIDI-Editor `START / STOP` einschalten.
6. Bei erfolgreicher Aufnahme entstehen einzelne WAV-Dateien unter `%USERPROFILE%\Documents\125A AutoSampler\Run_<Zeit>_G<Nummer>_<Zähler>\`.

Wichtig: Ein gewöhnliches VST3 kann den Transport der DAW nicht zuverlässig selbst starten. Daher ist beim aktuellen Ablauf zusätzlich `Play` in der DAW erforderlich. Die VST3-MIDI-Ausgangsverbindung ist in Fender Studio Pro noch nicht bestätigt und darf nicht als getestet beschrieben werden.

## Parameter

| Bedienelement | Bereich | Standard |
|---|---|---|
| Start/Stop | Aus/Ein | Aus |
| First MIDI note | 0–127 | 60 |
| Last MIDI note | 0–127 | 60 |
| Note step | 1–12 Halbtöne | 1 |
| Round Robins | 1–16 | 8 |
| Velocity layers | 1–3 | 1 |
| Velocity 1 | 1–127 | 100 |
| Velocity 2 | 1–127 | 60 |
| Velocity 3 | 1–127 | 120 |
| Note hold | 0,25–8 s | 1 s |
| Release capture | 0,25–10 s | 2 s |
| Gap | 0,05–0,5 s | 0,1 s |

Bei mehreren aktiven Velocity-Layern wird die Auswahl automatisch aufsteigend angeordnet. Nur die ersten 1–3 Velocity-Eingabewerte werden verwendet, je nach Layer-Anzahl.

## Reproduzierbare Kernprüfung

Die C++-Tests umfassen:
- Noten, Velocity-/RR-Sortierung, samplegenaue Note-On-/Note-Off-/Capture-Grenzen
- jedes MIDI-Velocity-Integer 1–127
- 2 Noten × 3 Velocity-Layer × 8 RR = **48 simulierte WAV-Dateien**, ohne doppelte Namen
- 24-Bit-Stereo-PCM-Header und vollständige Frame-Längen
- Parameter-Speichern beim Projekt-Reopen mit Run=Aus

Der offizielle Steinberg-Validator prüft beide VST3-Komponenten. Das **125A Plugin Test Lab P68/P74** bietet derzeit keine allgemeine automatische Auswahl mehrerer Prozessorklassen in einem Bundle; daraus wird kein fiktiver Lab-PASS abgeleitet.

## Noch nicht für produktiven Einsatz freigegeben

- Keine Fender-Studio-Pro-Abnahme der MIDI-Ausgabe, Audioaufnahme und eigenen VSTGUI-Oberfläche
- Noch keine frei wählbare Ausgabepfad-Option oder sichtbare Fehleranzeige für verlorene Frames
- Nur ein MIDI-Generator-/Recorder-Paar pro Host-Prozess
- Kein tatsächlicher EVM-Isotope-Klang- und 8-RR-WAV-Vergleich

Ein erfolgreicher SDK-Build beweist diese offenen Punkte nicht.
