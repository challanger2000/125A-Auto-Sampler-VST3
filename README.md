# 125A Auto Sampler VST3

Ein Windows-x64-VST3 für **Fender Studio Pro / Studio One**: vorhandene Instrumente innerhalb der DAW automatisch als WAV sampeln, einschließlich mehrerer Velocity-Layer und Round-Robin-Aufnahmen.

## Verbindliche Ziele

- **Kein eigener VST-Host.** Das zu samplende VST-Instrument läuft bereits in der DAW.
- AutoSampler erzeugt die MIDI-Noten selbst (Notenbereich, Schrittweite, Notenlänge und Velocity-Stufen).
- Für jede Note und Velocity-Stufe werden konfigurierbar **1–16 getrennte Takes (Round Robin)** aufgenommen; insbesondere 8 Takes pro Note.
- Das vom Quellinstrument erzeugte Audio wird in der DAW erfasst, inklusive konfigurierbarem Vorlauf, Note-Off und Ausklang.
- Samples werden automatisch getrimmt; kurze Fades verhindern Knackser.
- Jedes Sample wird als eigenständige WAV-Datei mit eindeutiger Noten-/Velocity-/RR-Kennung exportiert.
- Kein automatischer Import oder Mapping in andere Sampler: Das Produkt erzeugt **WAV-Dateien**.
- C++23; Windows-x64-VST3; Testautomatisierung mit dem 125A Plugin Test Lab.

## Vorgesehene Architektur

1. **MIDI-Controller (VST3-Instrument):** generiert eine genau definierte Sequenz aus Note-On, Note-Off und Pausen.
2. **Audio-Recorder (VST3-Audioeffekt):** sitzt als Insert hinter dem Quellinstrument und speichert die Audioblöcke je Take.
3. **Synchronisierung:** controllerseitige Ereigniszuordnung zum Recorder; Timing und Thread-Sicherheit müssen mit einem tatsächlichen Studio-Pro-Routingtest verifiziert werden.
4. **WAV-Export:** Verarbeitung und Dateischreiben außerhalb des Echtzeit-Audiothreads.

Die MIDI-Controller-/Recorder-Kopplung und das konkrete MIDI-Routing in Fender Studio Pro sind **zu validieren**. Diese README dokumentiert die Anforderungen, keine fertiggestellte Implementierung.

## Erster Abnahmetest

- EVM Isotope ist als Instrument in Fender Studio Pro geladen.
- 125A AutoSampler spielt C3 bei Velocity 100 achtmal an.
- Acht getrennte WAV-Dateien mit korrektem Attack, Note-Off und Release entstehen.
- Keine Überschreibung, kein zusammengeklebtes Audio und keine zusätzlichen Phantom-Takes.

## Projektstatus

Anforderungen dokumentiert; Implementierung und Praxistest ausstehend.
