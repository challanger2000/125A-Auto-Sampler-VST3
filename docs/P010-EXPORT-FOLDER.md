# P010 – Exportverzeichnis direkt im VST3 wählen

## Implementiert

- Im bestehenden dunklen MIDI-VSTGUI gibt es **CHOOSE FOLDER**. Das öffnet den nativen Windows-Ordnerdialog.
- **DEFAULT** setzt den Ausgabeordner wieder auf den bewährten Standard zurück:
  `%USERPROFILE%\Documents\125A AutoSampler\`.
- Die Auswahl erscheint im GUI-Feld **OUTPUT:**.
- Der Pfad wird zusammen mit den MIDI-/Sampling-Einstellungen im VST3-Projektzustand gespeichert (neue State-Version `0x125A1001`); vorherige P003-/P004-State-Dateien bleiben lesbar.
- Ein gewählter Pfad wird **nur im Datei-Writer-Thread zu Beginn eines neuen Runs** übernommen. Während laufender Aufnahmen ändert sich das Ziel nicht, selbst wenn der GUI-Pfad später gewechselt wird.
- Das Audio-`process()` liest **keine** Dateipfade, sperrt **keinen** Mutex und startet **keinen** Dateidialog.
- WAV-Namen und `RUN-REPORT.csv` bleiben unverändert. Für jeden Run entsteht weiterhin ein eigenes Unterverzeichnis. Es gibt kein Überschreiben vorheriger Runs.
- Wird der Windows-Ordnerdialog abgebrochen, bleibt die bisherige Auswahl erhalten.
- Ist ein gespeicherter Zielordner nicht mehr beschreibbar, wird ein Exportfehler protokolliert; es gibt **keinen** stillen Wechsel auf einen anderen Zielordner.

## Tests

Core: `ExportTarget` bestätigt ausgewählten Pfad, Rückfall auf Default und Zurücksetzen.

Echte Windows-VST3-Integration: Ein Test setzt einen Zielordner mit Leerzeichen in einem serialisierten Projektzustand, lädt ihn in die tatsächlich gebaute MIDI-VST3, speichert/liest denselben Projektzustand erneut und prüft **8 reale PCM24-WAV-Dateien** im benutzerdefinierten Ordner. Der frühere Standardordner darf dabei keine Samples enthalten. Die bisherigen 8 + 8 + 48 Standardaufnahmen und der manuelle STOP-/ABORTED-Test bleiben obligatorisch.

**P010 Windows-End-to-End-Abnahme: PASS.** [GitHub Actions Lauf #43538521360](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43538521360). Die tatsächlich gebaute VST3 erzeugte im benutzerdefinierten Ordner nach State-Load und Save/Reload **8 von 8 WAV-Dateien**, ohne Dateien im Default-Ziel. Die vorherigen 64 von 64 WAVs sowie der STOP-/ABORTED-Test blieben ebenfalls erfolgreich; ebenso Steinberg-Validator, native GUI-Anbindung, C++-Core-Tests. Windows-Artefakt: `125A-AutoSampler-P010-Win64`.

Der CI-Nachweis steht unter [P010 Windows VST3 Export Directory QA](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml).

**Grenze:** Die GUI-Ordnerauswahl ist per API implementiert, aber ihr tatsächliches Anklicken im Fender-Studio-Pro-Fenster bleibt Teil der abschließenden DAW-Praxisabnahme. Der native Test überprüft die Auswahl via Projektzustand, nicht die Mausbedienung des Windows-Ordnerdialogs.
