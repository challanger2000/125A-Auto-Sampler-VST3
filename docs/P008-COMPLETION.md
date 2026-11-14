# P008 — Automatischer Abschluss und vollständiger Exportbericht

## Behobenes Problem

Bis P007 waren MIDI-Noten und WAV-Dateien zwar korrekt, aber die MIDI-Instanz blieb nach dem letzten Take intern aktiv. Zudem schrieb der Recorder die `SUMMARY`-Zeilen von `RUN-REPORT.csv` unter Umständen erst bei `setActive(false)` oder beim nächsten Durchlauf. Für einen normalen DAW-Workflow ist das kein belastbarer Abschluss.

## Implementierung

1. `JobSchedule::jobCaptureEnd()` liefert das Ende des letzten aufgenommenen Release-Fensters — **ohne die anschließende Pause**.
2. Der MIDI-Generator beendet den Job **erst bei der folgenden Blockgrenze**. Die letzte Audiofreigabe wird nicht vorzeitig durch einen bereits disarmten gemeinsamen Session-Anker abgeschnitten.
3. Ein abschließender Recorder-Frame trägt das `jobEnd`-Kennzeichen. Der Hintergrund-Writer schreibt alle WAV-Daten und schließt anschließend sofort den CSV-Bericht samt `SUMMARY`-Zählerständen.
4. Falls der letzte Frame verloren geht, kann der Writer anhand des bestätigten Ablaufendes einen unvollständigen Take als solchen melden. Er erklärt niemals eine abgebrochene Aufnahme zu einer erfolgreichen WAV-Datei.
5. Die GUI unterscheidet `RECORDING`, `FINALIZING` und `COMPLETE`. `COMPLETE` wird erst nach dem Schließen des Berichts durch den Writer angezeigt.
6. Nach dem Abschluss verhindert der P007-RunGate weitere MIDI-Ereignisse oder neue WAV-Sessions, solange kein ausdrücklich angeforderter RUN-Aus→Ein-Wechsel erfolgt.

## Prüfungen

- Core-Regression: exaktes Ende des letzten Release-Fensters, Blockgrenzen vor/nach dem Ende, negative Projektpositionen, ungültiger Notenplan.
- Echte VST3-Integration: `RUN-REPORT.csv` muss bei **noch laufender DAW und aktivem Recorder** bereits eine vollständige `SUMMARY` enthalten. Nach dem Ende sind weitere MIDI-Ereignisse verboten.
- Unveränderte End-to-End-Matrix: 44,1 kHz/64 Frames = 8 WAV, 48 kHz/1024 Frames = 8 WAV, 44,1 kHz/512 Frames = 48 WAV.
- Offizieller Steinberg-Validator und natives Win32-Editor-Smoke-Test bleiben obligatorisch.

Automatisierter Prüfworkflow: [P008 Windows VST3 Completion QA](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml).

**Abnahmegrenze:** Die native Windows-Teststrecke mit synthetischem Audiosignal ersetzt weder das tatsächliche MIDI-Routing in Fender Studio Pro noch einen ASIO-Hardwaretest oder einen Klangvergleich mit einem externen VST.
