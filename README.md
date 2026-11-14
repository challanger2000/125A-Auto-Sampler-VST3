# 125A Auto Sampler VST3

**Windows-x64-VST3 zum automatischen Sampeln bereits in Fender Studio Pro geladener Instrumente.**

**Kein eigener VST-Host im Produkt.** Das Quellinstrument bleibt in der DAW. Zwei 125A-Plugin-Klassen bilden das System:

- **125A AutoSampler MIDI:** generiert Note-On/Off, Notenbereiche, Velocity-Stufen und Round Robins; eigene anthrazitfarbene VSTGUI-Oberfläche mit Start/Stop und Live-Zählern.
- **125A AutoSampler Recorder:** letzter Audio-Insert auf dem Quellinstrument, sammelt dessen echte Audiosamples und schreibt separate WAVs auf einem Hintergrundthread.

## P010 – wählbarer WAV-Ausgabeordner

Die MIDI-GUI besitzt jetzt **CHOOSE FOLDER** (Windows-Ordnerdialog) und **DEFAULT** (Rückkehr zum Standardverzeichnis). Der aktuell ausgewählte Ordner wird angezeigt und mit dem DAW-Projekt gespeichert. Der Recorder übernimmt die Auswahl zu Beginn jedes neuen Runs auf seinem Datei-Worker; der Audiothread bleibt frei von Dateioperationen und Sperren. Der Default bleibt `%USERPROFILE%\\Documents\\125A AutoSampler`.

Ein automatisierter Windows-Test lädt den Projektzustand mit einem benutzerdefinierten Zielordner, testet Save/Reload und prüft die daraus erzeugten acht echten VST3-WAV-Dateien. Details: [P010-EXPORT-FOLDER](docs/P010-EXPORT-FOLDER.md). **Die GUI-Mausbedienung in Fender Studio Pro ist noch nicht praktisch abgenommen.**

## P008/P009 – automatischer Abschluss und sicherer Abbruch

**P008 (PASS):** Der normale Sampling-Durchlauf wird nach dem letzten Ausklang automatisch abgeschlossen. Der Hintergrund-Recorder schreibt und schließt `RUN-REPORT.csv` bei laufender DAW, ohne dass das Plugin deaktiviert werden muss. Die GUI zeigt `RECORDING`, `FINALIZING` und `COMPLETE`.

**P009 (PASS):** Ein manuelles STOP während einer aktiven Note erzeugt das passende MIDI-Note-Off, markiert den angebrochenen Take `ABORTED`, schließt ebenfalls den CSV-Bericht ohne Plugin-Deaktivierung und erzeugt **keine** unvollständige WAV-Datei. Ein erneuter Durchlauf setzt RUN-Aus→Ein voraus.

**Native Windows-Nachweise:** 64/64 reguläre WAV-Dateien aus drei Samplerate-/Blockgrößen-Szenarien sowie der eigenständige STOP-/ABORTED-Regressionstest. Offizieller Steinberg-Validator, VSTGUI-HWND-Test und C++-Tests bestanden.

- [Aktueller P009-Windows-VST3-Build](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43535357254)
- [P008 – Automatischer Abschluss](docs/P008-COMPLETION.md)
- [P009 – Manuelles Stoppen](docs/P009-MANUAL-STOP.md)

## P007 – sicherer Neustart

Ein neuer Sampling-Durchlauf startet **nur nach einem bewusst ausgeführten RUN-Aus→RUN-Ein-Wechsel** bei laufendem DAW-Transport. Ein DAW-Stopp, Loop/Songpositionssprung oder Plugin-Deaktivierung kann keine neue WAV-Session auslösen, nur weil der Run-Schalter noch auf Ein steht. Nach einem Stopp muss der Benutzer RUN zunächst ausschalten und erneut einschalten. Die wiederholte DAW-Wiedergabe allein erzeugt keine neuen Exporte.

P007 enthält dafür einen Core-Zustandsautomaten und einen Regressionstest, der die **tatsächlich gebaute VST3** deaktiviert/reaktiviert und auf unerlaubte MIDI-Ausgabe prüft. Abnahme: aktueller Windows-P007-GitHub-Actions-Lauf.

## P006 – nachweislich implementiert

- MIDI-Noten 0–127, Schrittweite 1–12 Halbtöne
- 1–3 frei einstellbare Velocity-Layer (Werte 1–127), 1–16 Round Robins
- Notenlänge 0,25–8 s, Release-Capture 0,25–10 s, Pausen 0,05–0,5 s
- 24-Bit-Stereo-WAV, Stille-Trim, kurze Fades, keine Normalisierung
- Eindeutiger Ordner pro Sampling-Durchlauf, automatisch benannte WAVs
- **Fail-closed Audioaufnahme:** verlorene, lückenhafte, abgebrochene oder nicht vollständige Takes werden NICHT als gültige WAVs ausgegeben
- **RUN-REPORT.csv** je Lauf mit Einzelresultaten und Zusammenfassung
- Live-Zähler im Editor: Takes, fertige WAVs, Rejects, Silence, Exportfehler und verlorene Audioframes
- Projekt-Speichern/Laden; Start wird beim Projekt-Reload bewusst deaktiviert
- Note-Off mit korrekter Note-ID bei Stop und Song-Positionssprüngen

## P005–P006 – unabhängig geprüfte Technik

Ein privater, **nicht ausgelieferter Windows-Test-Host** lädt beide **tatsächlichen VST3-Klassen** und speist ein synthetisches Instrumentensignal ein.

**Ergebnis:** acht echte VST3-Note-On, acht Note-Off, acht voneinander getrennte hörbare Stereo-24-Bit-WAVs, 0 verworfene Takes und 0 verlorene Frames. Zusätzlich wurden der Stop-Test mit springender Songposition sowie das echte Öffnen/Schließen der VSTGUI-Oberfläche auf Windows erfolgreich ausgeführt. Der offizielle Steinberg-Validator, die GUI-Paketprüfung und sämtliche Core-Regressionen sind erfolgreich.

**P006 zusätzlicher Nachweis:** drei tatsächlich ausgeführte Integrationsszenarien mit den beiden geladenen VST3-Klassen, sämtlich bestanden. **44,1 kHz / 64 Samples: 8 WAVs**, **48 kHz / 1024 Samples: 8 WAVs**, **44,1 kHz / 512 Samples: 48 WAVs** (2 Noten × 3 Velocity-Layer × 8 RR), insgesamt **64 von 64**. Sämtliche WAV-Dateinamen wurden gegen die vollständige Sollmenge geprüft; 0 verlorene Audioframes.

- [Erfolgreicher Windows-P006-Build samt VST3-Artefakt](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43530522001)
- [START-HERE: Installations- und Prüfanleitung](START-HERE.md)
- [P006: echte VST3-Testmatrix und Nachweise](docs/P006-TEST-MATRIX.md)
- [P005: historischer Nachweis](docs/P005-VALIDATION.md)
- [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2)

**Noch nicht als fertiges Produkt freigegeben.** Die vollständige Weiterleitung MIDI → Fremd-VST → Recorder innerhalb **Fender Studio Pro** sowie ASIO-Hardware und die finale Bedienung im echten Song sind noch nicht abgenommen. Der synthetische Windows-Test ist kein Klangtest mit EVM Isotope. P010 bleibt ein Entwicklungsstand.
