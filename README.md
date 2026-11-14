# 125A AutoSampler VST3 – P018: Portable Instrumente und funktionale VSTGUI

Windows-x64-VST3 für **automatisches Sampeln eines bereits in Fender Studio Pro geladenen VST-Instruments und direktes Spielen der aufgenommenen Samples im 125A selbst**.

**125A IST der Sampler.** Es wird kein SFZ-Player, kein externer Sampler und kein eigener Fremd-VST-Host benötigt. Das Quell-VST bleibt in der DAW.

## Bestandteile desselben VST3-Bundles

- **125A AutoSampler (Instrument):** Erzeugt den MIDI-Aufnahmeplan, importiert eigene `*.125A`-Instrumentarchive oder ältere WAV-Banken und spielt sie als eigenständiger Stereo-Sampler. Mit **INSTRUMENT NAME**, **LOAD INSTRUMENT**, Gain, ADSR, Normalize, One-Shot und MIDI-reaktiver Steinberg-VSTGUI-Tastatur.
- **125A AutoSampler Recorder (Effekt):** Letzter Audio-Insert hinter dem Fremd-VST; schreibt Stereo-PCM24-WAVs und erzeugt anschließend selbständig das fertige `*.125A`-Archiv. Sein kleines Logo-/Statusfenster zeigt **IDLE**, **READY** oder **RECORDING**, ohne weitere Bedienelemente.
- Noten 0–127, 1–12 Halbtonschritte, 1–3 frei einstellbare Velocity-Layer, 1–16 Round Robins, konfigurierbare Noten- und Release-Dauern.
- Zuverlässiger Aufnahmeabschluss: ungültige, lückenhafte, stumme und abgebrochene Takes werden nicht als erfolgreiche WAV-Exporte ausgegeben. Jede Aufnahme erzeugt einen eigenen `Run_...`-Ordner mit `RUN-REPORT.csv`.

## Eigenständige Sample-Wiedergabe (P013)

1. Die WAVs werden als `N60_V100_RR01.wav` usw. im Run-Ordner gespeichert.
2. Bei erfolgreicher Aufnahme übernimmt der eigene Bank-Worker die WAVs automatisch – **das Pluginfenster muss dafür nicht geöffnet sein**. **LOAD SAMPLES** erlaubt alternativ das Laden eines älteren Run-Ordners.
3. Das Instrument **125A AutoSampler** auf der DAW-Spur mit MIDI-Noten spielen: Es gibt nun Stereo-Audio aus seinem **eigenen** Sampler aus.
4. Die Bank wird nach Root-Note, Velocity und Round-Robin-Index zugeordnet. Nicht gesampelte Zwischentöne werden durch Pitch-Transposition der nächsten aufgenommenen Grundnote gespielt.
5. Der Sampler bietet bis zu 32 simultane Stimmen und spielt den aufgenommenen Ausklang einmalig ab (One-Shot). Es gibt noch keine Sustain-Loop-Erkennung.

**P014 – Bankübernahme ohne Editor:** Nach einem vollständig erfolgreichen Aufnahmeabschluss lädt ein eigener, nicht-echtzeitfähiger Bank-Worker die neue WAV-Bank in den bereits laufenden 125A, auch wenn seine GUI geschlossen bleibt. **LOAD SAMPLES** ist nur für ältere/andere Run-Ordner nötig. Die DAW-Projektdatei speichert weiterhin den **Pfad** zur Bank, nicht die WAV-Daten selbst; die WAVs daher am Speicherort belassen. [Vier echte VST3-Headless-Wiedergabetests: PASS](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43564563114).

**Kein SFZ nötig:** Die bisherige P011/P012-SFZ-Exportidee wird im produktiven Aufnahmeablauf nicht mehr verwendet. Die historischen SFZ-Dateien und -Core-Tests sind nicht Bestandteil der P013-Wiedergabekette.

## P015 – einfache One-Shot-Instrumentensteuerung

Nach dem Laden einer Bank stehen direkt im 125A-Editor zur Verfügung:

- **NORMALIZE –1 dBFS:** Standardmäßig **aus**; **ein gemeinsamer Peak-Gain über die gesamte Bank** statt einzelner WAV-Verstärkung. Dynamikunterschiede zwischen Velocity-Layern und Round Robins bleiben erhalten. Die originalen WAV-Dateien werden dabei nicht verändert.
- **ONE SHOT / GATE:** One-Shot ist Standard und spielt die gesamte kurze Aufnahme unabhängig vom Loslassen der Taste. Gate schaltet Note-Off und die Amp-Release-Phase frei.
- **AMP ADSR:** Attack 0–2 Sekunden, Decay 0–3 Sekunden, Sustain 0–100 %, Release 0–3 Sekunden. Ohne zusätzliches Processing bleibt der aufgezeichnete Originalklang hörbar.
- **NOTE STEP:** Bereits vorhanden: 1–12 Halbtöne (etwa C1/C2/C3/C4 mit Step 12); der interne Player transponiert die Lücken.
- **Schneller Vintage-Start:** Die Werkseinstellung nimmt nur **1 Round Robin**, **1 Velocity-Layer** und pro Take **1,0 Sekunde Note + 2,0 Sekunden Ausklang + 0,1 Sekunden Pause** auf. NOTE STEP steht zunächst auf **12 Halbtönen**. Für besondere Klänge kann alles bewusst verlängert oder dichter gesampelt werden.

Ein einfacher dunkel gehaltener VSTGUI-Testbereich ist angefügt. Die 19 Parameter sind DAW-automatisierbar. Die komplette Aufnahme-WAV wird ohne Sustain-Loop gespielt. Für Vintage-One-Shots bewusst kein Loop-Editor, keine Effektkette und keine komplexe Modulationsmatrix.

**Noch ausstehend:** DAW-unabhängige automatische Wiederverknüpfung bei verschobenen Archiven, vollständig gestaltete Hardware-GUI und erneute Fender-Studio-Pro-/ASIO-Praxisabnahme.

## P016 – Normalize ohne harte Übersteuerung

**Praxishinweis:** P015 normalisierte die gesamte WAV-Bank korrekt auf −1 dBFS, aber überlagerte One-Shot-Stimmen wurden nach der Summierung intern bei ±1 hart abgeschnitten. Dadurch konnte aktiviertes NORMALIZE hörbare Verzerrungen auslösen. **P016** mischt aktive Stimmen mit gemeinsamer, rein linearer Headroom-Anpassung, erhält den Einzelnoten-Zielpegel, führt die Pegelreserve nach dem Ausklang zurück und entfernt das harte Clipping am Float-Ausgang. Die WAVs werden weiterhin **nicht** verändert.

**Regression:** Acht gleichzeitig klingende, normalisierte Stimmen; zusätzlich wurde dieselbe Belastung im echten Windows-x64-VST3 nach dem automatischen PCM-Bankimport geprüft. [P016 Windows PASS: 94/94 Steinberg-Validator, alle Core-Tests, vier echte VST3-Integrationsläufe mit 72 WAVs](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43570184894). [Aktuelles P016-Test-Bundle](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43570184894/artifacts/14104102675).

Bei der DAW-Wiedergabe muss die **Quell-VST-Spur** nach der Aufnahme stummgeschaltet bzw. ihr Monitoring ausgeschaltet sein, sonst klingen Quell-VST und 125A gegebenenfalls gleichzeitig; diese externe Summierung kann weiterhin den DAW-Ausgang übersteuern. Normalize verarbeitet ausschließlich das Audio der internen 125A-Sampler-Engine, nicht das Quell-VST.

## P017/P018 – Portable 125A-Instrumente und die ersten eigenen GUI-Bedienelemente

- **Eigenständige `*.125A`-Datei:** Der Recorder erstellt nach vollständig erfolgreichem Run im gewählten Ausgabeordner automatisch `<Instrumentenname>.125A`. Bei gleichem Namen werden bestehende Archive nicht überschrieben, sondern `<Instrumentenname> (2).125A`, `(3)` usw. angelegt.
- **Offen verwendbares ZIP:** Eine `.125A` ist ein unverschlüsseltes, ZIP32-kompatibles Archiv mit STORE-Einträgen (`instrument.json` und unveränderten einzelnen 24-Bit-Stereo-WAVs). Unter Windows `.125A` in `.zip` umbenennen und die gewünschte WAV für einen anderen Sampler extrahieren. CRC und Eintragsnamen werden beim Laden geprüft.
- **Keine Sample-Map-Editor-Seite:** Root-Noten, Velocity-Stufen und Round Robins werden aus eindeutigen Dateinamen automatisch erkannt. Der interne Player lädt die Samples direkt aus dem Archiv, ohne Drittanbieter.
- **Instrumentenname und Archivimport:** Name in der Test-VSTGUI eingeben. **LOAD INSTRUMENT** öffnet einen Windows-Dateidialog; **LOAD SAMPLES** lädt nur ältere lose WAV-Run-Ordner. Die DaW speichert derzeit den absoluten Archivpfad; bei Umzug kann das Archiv über **LOAD INSTRUMENT** neu zugeordnet werden.
- **Wiedergabe:** Gain −12 bis +12 dB, Standard 0 dB; Normalize standardmäßig **aus**. Dauerhaft sichtbare ADSR-Parameter.
- **Tastatur:** Die vorhandene Steinberg-`VSTGUI::KeyboardView` (kein JUCE) zeigt gehaltene MIDI-Noten, ermöglicht Vorschau und Auswahl einer einzelnen Aufnahmenote. Gegenwärtig 49 Tasten als einfache Testgrafik; endgültige Hardware-Optik folgt separat.
- **Recorder:** Eigenes statisches 125A-Branding plus lebende IDLE/READY/RECORDING-Anzeige, ohne Regler und ohne Leere im DAW-Fenster.

**Prüfstand:** [P017 vollständiger Windows PASS](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43576534622): 94/94 Steinberg-Validatorprüfungen, 72/72 WAVs, vier native PCM-Wiedergabetests, automatischer .125A-Export und Import, zwei VSTGUI-Win32-Fensterprüfungen. [Geprüftes Test-ZIP](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43576534622/artifacts/14106249655). Neuere Sicherheitsprüfungen (unabhängiger .NET-ZIP-Leser und Windows-Dateinamen) laufen zusätzlich. **Kein vollständiger Praxistest in Fender Studio Pro/ASIO.**

## Routing in Fender Studio Pro

1. Eine Instrumentenspur mit **125A AutoSampler** erstellen.
2. Auf einer anderen Instrumentenspur das zu sampelnde VST (z. B. EVM Isotope) laden.
3. Als MIDI-Eingangsquelle des Fremd-VST den **MIDI-Ausgang von 125A AutoSampler** wählen und die Spur entsprechend monitoren.
4. Im Audio-Insert der Fremd-VST-Spur **125A AutoSampler Recorder** möglichst als letzten Effekt setzen.
5. DAW-Transport auf Play stellen, dann den 125A-RUN-Schalter einschalten. Nach dem vollständigen Abschluss übernimmt der 125A die Bank automatisch und macht sie spielbar.
6. RUN wieder ausschalten, falls er noch aktiviert ist, und anschließend MIDI auf der 125A-Spur spielen.

Das genaue MIDI-Routing, die physische ASIO-Ausgabe und das Dialogverhalten in **Fender Studio Pro** müssen praktisch geprüft werden; ein synthetischer VST3-Testhost ersetzt diesen Nachweis nicht.

## Entwicklungsstand / Abnahmegrenzen

- **P001–P012:** Capture, sicherer Start/Stop, Export, Projektzustand und bisherige Windows-VST3-Integrationsfälle bestanden. Historische Prüfdokumente stehen unter [docs](docs/).
- **P013 – Windows-Abnahme PASS:** Eingebauter WAV-Sampler, MIDI-Wiedergabe, Round-Robin-/Velocity-Mapping und Projektzustand. [Erfolgreiche vollständige Windows-VST3-Abnahme mit tatsächlicher PCM-Wiedergabe](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43542007225): 72/72 WAVs und vier native MIDI-zu-Audio-Playback-Proben, Steinberg-Validator, GUI- und Core-Tests grün. Artefakt: `125A-AutoSampler-P013-Win64`.
- **P014 – Windows-Abnahme PASS:** [72/72 WAVs, vier Wiedergabeproben ohne geöffneten Editor und ohne Projekt-State-Import, Steinberg-Validator 94/94, Audio-STOP/ABORT und VSTGUI](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43564563114). Kein anderer Sampler erforderlich.
- Noch ein Generator-/Recorder-Paar je DAW-Prozess, maximal 16 nacheinander geladene Banks pro Instrumentinstanz, keine Sustain-Loops.
- **P015 Windows PASS:** [Kompletter Windows-Lauf #43567864929](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43567864929) bestätigt 94/94 Steinberg-Tests, Core-Tests einschließlich Normalize/Gate-ADSR, GUI-Validierung mit 18 Parametern und 72/72 WAVs inklusive hörbarem Headless-Playback. [Hier das getestete Windows-x64-VST3-Bundle](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43567864929/artifacts/14102478659).
- Kein fertiges Release, solange die Tests in Fender Studio Pro und mit ASIO-Hardware ausstehen. [PR #2 bleibt Draft](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2).

**Installation und Aufnahmeablauf:** [START-HERE.md](START-HERE.md).
