# P012 – Wahrheitsgemäßer Aufnahme- und Bankstatus

## Produktkorrektur

P011 erstellt nach vollständig erfolgreichen WAV-Aufnahmen automatisch eine SFZ-Bank. Ein zuvor vorhandener GUI-Fehler konnte dennoch `COMPLETE` anzeigen, obwohl WAVs fehlten, ein SFZ-Schreibfehler auftrat oder der Recorder niemals Audio empfangen hatte.

Die neue Statuslogik unterscheidet:

| Status | Verbindliche Bedeutung |
|---|---|
| READY / STOPPED | Keine Aufnahme aktiv oder noch kein Durchlauf gestartet |
| RECORDING | MIDI-/Audio-Job läuft |
| FINALIZING | Letztes Audio wurde empfangen, Writer schreibt noch |
| COMPLETE | Alle erwarteten WAVs und die SFZ-Bank erfolgreich geschrieben, keine Verluste |
| PARTIAL / FAILED | Ein Teil der WAVs erfolgreich, aber Fehler, Stille oder fehlende Samples |
| FAILED | Nichts erfolgreich ausgegeben |
| ABORTED | Benutzer oder DAW hat Run unterbrochen |
| NO AUDIO RECORDER | Lauf beendet, jedoch kein einziger Take beim Recorder angekommen |

Das Plugin behauptet **keinen Erfolg mehr**, wenn eine Bank fehlt oder der Recorder nicht richtig geroutet ist. Es verwirft alte Abbruchkennzeichen beim Anzeigen einer neuen Session.

## Umsetzung und Tests

`src/core/RunStatus.h` berechnet den Zustand deterministisch und ohne Dateizugriffe, Heap-Reservierungen oder Sperren. Der VSTGUI-Timer ruft diese Funktion mit den ohnehin vorhandenen atomaren Session-Zählern auf.

Automatisierte Core-Tests decken Erfolgs- und Fehlerzustände, fehlenden Recorder, Stille, Teil-Exporte, SFZ-Schreibfehler, Abbruch und neue Session-Generationen ab. Der Windows-Workflow prüft darüber hinaus die VST3-GUI, den offiziellen Steinberg-Validator und die vollständigen P011-SFZ/WAV-Integrationsfälle.

[Windows-P012-Workflow](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml)

**Abnahmegrenze:** Der Status wird automatisiert aus seinen Daten berechnet; ob jede Meldung in Fender Studio Pro optisch korrekt lesbar ist, bleibt Teil der abschließenden GUI-Praxisabnahme.
