# START HERE – 125A AutoSampler P012

## 1. Was bis P012 erfolgreich unter Windows getestet ist

- Windows-x64-VST3 mit zwei echten Klassen: MIDI-Generator und Audio-Recorder
- VSTGUI-Editor tatsächlich über Win32-HWND geöffnet, Status-Timer gestartet und sauber geschlossen
- Steinberg-Validator, automatisierte GUI-Bundle-Prüfung und C++-Regressionstests bestanden
- **Vollständige native Integrationsprüfung:** 64 echte VST3-Note-On, 64 Note-Off und 64 hörbare Stereo-24-Bit-WAVs in drei getrennten Szenarien: 44,1 kHz/64 Samples (8 Takes), 48 kHz/1024 Samples (8 Takes), 44,1 kHz/512 Samples (48 Takes aus 2 Noten × 3 Velocities × 8 RR). Kein Frameverlust
- Realer Plugin-Stop mit springender Songposition sendet die passende Note-Off-ID
- Getrennte RUN-REPORT.csv-Einträge; korrupte und abgebrochene Takes werden nicht exportiert

[P012 Windows-Build samt VST3-Artefakt – PASS](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43539953663) – ZIP-Name **125A-AutoSampler-P012-Win64**. Nur die VST3-Datei installieren; das Testprogramm ist nicht Bestandteil des Plugins.

## P011/P012 – SFZ-Instrument automatisch mit ausgeben

Bei einem **vollständig erfolgreichen** Durchlauf entstehen automatisch die PCM24-Stereo-WAVs und die Datei `125A AutoSampler.sfz` **im selben Run-Ordner**. Die SFZ-Datei lässt sich als gemapptes Instrument in einem SFZ-kompatiblen Sampler laden. WAV-Dateien immer zusammen mit der SFZ im Ordner lassen oder gemeinsam verschieben.

SFZ ordnet gesampelte Noten inklusive Zwischentönen, Velocity-Layer und Round Robins zu. Stand P011 ist **One-Shot-Abspielung**, keine automatische Sustain-Loop-Erkennung. Bei fehlender, stummer oder beschädigter WAV sowie bei Abbruch entsteht bewusst **keine** SFZ-Bank; `RUN-REPORT.csv` enthält dann `BANK,SKIPPED_INCOMPLETE` beziehungsweise `BANK,SFZ_ERROR`. Bei Erfolg erscheint `BANK,SFZ_CREATED,125A AutoSampler.sfz`.

Die mit Windows-P012-Tests geprüfte GUI-Statuslogik zeigt `COMPLETE` nur bei fehlerfreiem Export, sonst `PARTIAL / FAILED`, `FAILED` oder `ABORTED`; ohne aktiven Recorder nach Ende des Durchlaufs `NO AUDIO RECORDER`.

Siehe [P011-SFZ-Dokumentation](docs/P011-SFZ-BANK.md) und [P012-Statusregeln](docs/P012-RUN-STATUS.md).

## P010 – Ausgabeordner wählen

Im Fenster **125A AutoSampler MIDI** auf **CHOOSE FOLDER** klicken, um per Windows-Dialog einen Zielordner auszuwählen. **DEFAULT** setzt auf `%USERPROFILE%\\Documents\\125A AutoSampler` zurück. Hinter `OUTPUT:` wird die Auswahl angezeigt. Der Ordner wird mit dem Projekt gespeichert und beim nächsten Öffnen geladen; `RUN` bleibt dabei aus.

Die Auswahl gilt jeweils **ab dem nächsten Sampling-Durchlauf**. Eine bereits laufende Aufnahme schreibt weiterhin in ihren bisherigen Ordner. Pro Durchlauf entsteht wie zuvor ein `Run_...`-Unterordner samt WAVs und `RUN-REPORT.csv`.

Der native Windows-P010-Test hat die Pfad-Persistenz und den WAV-Export per VST3-Projektzustand mit **8 von 8 WAV-Dateien erfolgreich** geprüft. Das tatsächliche Anklicken des Windows-Dialoges in Fender Studio Pro steht noch als DAW-Praxistest aus. Siehe [P010-Dokumentation](docs/P010-EXPORT-FOLDER.md).

## P008/P009 – Abschluss und manueller Abbruch

Nach der letzten gesampelten Note schließt der Recorder automatisch seinen WAV-Lauf einschließlich `RUN-REPORT.csv`. Der Transport der DAW darf dabei weiterhin laufen.

Wenn du **START/STOP vorzeitig ausschaltest**, beendet das Plugin die aktive MIDI-Note, verwirft den unvollständigen Take und meldet `ABORTED` im CSV-Bericht sowie in der GUI. Der Bericht wird geschlossen, ohne dass die Plugin-Instanzen entfernt werden müssen. Der Abbruch wurde mit den tatsächlichen Windows-VST3-Klassen und einer synthetischen Klangquelle erfolgreich getestet: 0 unvollständige WAVs.

Ein neuer Start setzt RUN **Aus → Ein** voraus. Siehe [P008](docs/P008-COMPLETION.md) und [P009](docs/P009-MANUAL-STOP.md).

## Bedienhinweis für P007

Die DAW muss bereits auf **Play** stehen, bevor RUN eingeschaltet wird. Wenn die DAW gestoppt oder das Plugin deaktiviert wird, bleibt ein eingeschalteter RUN-Schalter eventuell optisch aktiv, **löst aber keinen neuen Sampling-Job aus**. Ein neuer Job erfordert immer RUN **Aus → Ein**. Das verhindert ungewollte Mehrfachaufnahmen beim erneuten Start der DAW-Wiedergabe oder Song-Loop.

## 2. Installation für spätere Fender-Studio-Pro-Abnahme

1. Unter der genannten GitHub-Action bei **Artifacts** das Windows-ZIP herunterladen.
2. Den vollständigen VST3-Bundle-Ordner nach `C:\Program Files\Common Files\VST3\` kopieren.
3. Fender Studio Pro neu starten und beide Plugin-Klassen im Plugin-Browser suchen.

## 3. Vorgesehener Routing-Aufbau in Fender Studio Pro

1. **Spur A:** Instrument `125A AutoSampler MIDI` laden.
2. **Spur B:** das zu samplende VST-Instrument laden (zum Beispiel EVM Isotope).
3. Bei **Spur B** im **MIDI-Eingang** `125A AutoSampler MIDI` als Quelle auswählen; falls angeboten **MIDI Output 1 / Kanal 1** wählen. Die Spur überwachen/Monitor aktivieren.
4. Auf dem Audio-Ausgang von **Spur B** `125A AutoSampler Recorder` als **letzten Insert-Effekt** laden. Den Recorder nicht auf Spur A einfügen: dort ist das Audio des Quellinstruments nicht vorhanden.
5. **DAW-Transport auf Play** und im AutoSampler den großen **START / STOP**-Schalter aktivieren.
6. Nach den Aufnahmen die individuellen WAV-Dateien und die Datei **RUN-REPORT.csv** im Zielordner prüfen.

Eine vergleichbare MIDI-Ausgangsweiterleitung ist für Scaler 2 in Studio One dokumentiert: https://help.pluginboutique.com/hc/en-us/articles/6232575716372-External-Instrument-Routing-Guide-Studio-One . **Dass Fender Studio Pro genau unsere 125A-VST3 als Eingangsquelle anbietet, muss noch praktisch bestätigt werden.**

## 4. Standard-Testlauf

Voreinstellung:
- Erste/letzte **MIDI-Note 60** (Oktavname kann je nach DAW C3 oder C4 sein)
- Velocity 100
- 8 Round-Robin-Wiederholungen
- 1 s Note-On, 2 s Ausklang, 0,1 s Pause
- 44,1 kHz oder 48 kHz, Stereo 24 Bit bei der Ausgabe

Erwartetes Ausgabeverzeichnis:

`%USERPROFILE%\Documents\125A AutoSampler\Run_<Zeit>_G<Generation>_<Zähler>\`

Bei erfolgreichem Test befinden sich acht Dateien `N60_V100_RR01.wav` bis `N60_V100_RR08.wav` und die gemappte Datei `125A AutoSampler.sfz` im Verzeichnis. **RUN-REPORT.csv** muss acht `OK`-Zeilen sowie `SUMMARY,exported,8`, `SUMMARY,rejected,0` und `SUMMARY,droppedFrames,0` enthalten.

**Bei fehlendem MIDI-Routing oder Stille ist das kein PASS.** Der Recorder meldet `SILENCE`, `INCOMPLETE`, `ABORTED`, `INVALID` oder `EXPORT_ERROR`, statt fehlerhafte Takes als funktionierende WAV-Dateien auszugeben. Diese Resultate erscheinen im Bericht; die GUI zeigt dazu Zähler.

## 5. Bedienbereiche

| Bedienelement | Bereich | Standard |
|---|---|---|
| Start / Stop | Aus / Ein | Aus |
| First / Last MIDI Note | 0–127 | 60 / 60 |
| Note Step | 1–12 Halbtöne | 1 |
| Round Robins | 1–16 | 8 |
| Velocity Layers | 1–3 | 1 |
| Velocity 1, 2, 3 | jeweils 1–127 | 100, 60, 120 |
| Note Hold | 0,25–8 s | 1 s |
| Release Capture | 0,25–10 s | 2 s |
| Gap | 0,05–0,5 s | 0,1 s |

Bei mehreren aktiven Velocity-Layern werden ihre Werte aufsteigend angeordnet. **Ein statischer Synthesizer liefert nicht automatisch unterschiedliche Round-Robin-Klangvarianten:** die acht WAVs können hörbar identisch sein.

## 6. Bekannte Grenzen

- Bisher nur **ein** Generator-/Recorder-Paar je Host-Prozess
- Benutzerauswahl per GUI-Ordnerdialog implementiert, aber in Fender Studio Pro noch nicht praktisch getestet
- Eigener DAW-Transportstart aus dem Plugin ist nicht garantiert; Play muss in Fender Studio Pro laufen
- Die neue SFZ-Bank wurde noch nicht mit einem tatsächlichen externen Sampler musikalisch abgespielt
- Keine bestätigte Fender-Studio-Pro-End-to-End-Abnahme mit EVM Isotope
- Kein physischer ASIO-Hardwaretest; dieser folgt erst bei der abschließenden DAW-Praxisabnahme
- 125A Plugin Test Lab P68/P74 behandelt Mehrklassen-VST3 noch nicht automatisch; der P006-Windows-Integrationstest ist eine separate, produktbezogene echte VST3-Prüfung, **keine Behauptung über einen Lab-P74-PASS**
- PR #2 bleibt bis zur Produkt-Abnahme ein **Draft**, kein Release

Diese Einschränkungen gelten trotz bestandener Steinberg- und P006-Windows-Tests.

Ausführliche Prüfmatrix: [docs/P006-TEST-MATRIX.md](docs/P006-TEST-MATRIX.md).
