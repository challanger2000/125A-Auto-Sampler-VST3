# 125A AutoSampler VST3 – P015: One-Shot, Normalize und Amp ADSR

Windows-x64-VST3 für **automatisches Sampeln eines bereits in Fender Studio Pro geladenen VST-Instruments und direktes Spielen der aufgenommenen Samples im 125A selbst**.

**125A IST der Sampler.** Es wird kein SFZ-Player, kein externer Sampler und kein eigener Fremd-VST-Host benötigt. Das Quell-VST bleibt in der DAW.

## Bestandteile desselben VST3-Bundles

- **125A AutoSampler (Instrument):** Gibt den automatischen Note-On-/Note-Off-Plan an die DAW weiter, nimmt gesampelte WAV-Bänke per **LOAD SAMPLES** in den eigenen internen Sampler auf und spielt sie anschließend bei eingehenden MIDI-Noten als Stereo-Audio.
- **125A AutoSampler Recorder (Effekt):** Steht als letzter Audio-Insert hinter dem zu samplenden Fremd-VST und schreibt echte Stereo-PCM24-WAVs auf einem separaten Writer-Thread.
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

- **NORMALIZE –1 dBFS:** Standardmäßig an; **ein gemeinsamer Peak-Gain über die gesamte Bank** statt einzelner WAV-Verstärkung. Dynamikunterschiede zwischen Velocity-Layern und Round Robins bleiben erhalten. Die originalen WAV-Dateien werden dabei nicht verändert.
- **ONE SHOT / GATE:** One-Shot ist Standard und spielt die gesamte kurze Aufnahme unabhängig vom Loslassen der Taste. Gate schaltet Note-Off und die Amp-Release-Phase frei.
- **AMP ADSR:** Attack 0–2 Sekunden, Decay 0–3 Sekunden, Sustain 0–100 %, Release 0–3 Sekunden. Ohne zusätzliches Processing bleibt der aufgezeichnete Originalklang hörbar.
- **NOTE STEP:** Bereits vorhanden: 1–12 Halbtöne (etwa C1/C2/C3/C4 mit Step 12); der interne Player transponiert die Lücken.
- **Schneller Vintage-Start:** Die Werkseinstellung nimmt nur **1 Round Robin**, **1 Velocity-Layer** und pro Take **0,5 Sekunden Note + 0,5 Sekunden Ausklang + 0,1 Sekunden Pause** auf. NOTE STEP steht zunächst auf **12 Halbtönen**. Für besondere Klänge kann alles bewusst verlängert oder dichter gesampelt werden.

Ein einfacher dunkel gehaltener VSTGUI-Testbereich ist angefügt. Die 18 Parameter sind DAW-automatisierbar. Die komplette Aufnahme-WAV wird ohne Sustain-Loop gespielt. Für Vintage-One-Shots bewusst kein Loop-Editor, keine Effektkette und keine komplexe Modulationsmatrix.

**Noch ausstehend:** portable, unabhängig vom absoluten WAV-Ordner verschiebbare Preset-Pakete und Fender-Studio-Pro-/ASIO-Praxistest.

## P016 – Normalize ohne harte Übersteuerung

**Praxishinweis:** P015 normalisierte die gesamte WAV-Bank korrekt auf −1 dBFS, aber überlagerte One-Shot-Stimmen wurden nach der Summierung intern bei ±1 hart abgeschnitten. Dadurch konnte aktiviertes NORMALIZE hörbare Verzerrungen auslösen. **P016** mischt aktive Stimmen mit gemeinsamer, rein linearer Headroom-Anpassung, erhält den Einzelnoten-Zielpegel, führt die Pegelreserve nach dem Ausklang zurück und entfernt das harte Clipping am Float-Ausgang. Die WAVs werden weiterhin **nicht** verändert.

**Regression:** Acht gleichzeitig klingende, normalisierte Stimmen; zusätzlich wurde dieselbe Belastung im echten Windows-x64-VST3 nach dem automatischen PCM-Bankimport geprüft. [P016 Windows PASS: 94/94 Steinberg-Validator, alle Core-Tests, vier echte VST3-Integrationsläufe mit 72 WAVs](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43570184894). [Aktuelles P016-Test-Bundle](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43570184894/artifacts/14104102675).

Bei der DAW-Wiedergabe muss die **Quell-VST-Spur** nach der Aufnahme stummgeschaltet bzw. ihr Monitoring ausgeschaltet sein, sonst klingen Quell-VST und 125A gegebenenfalls gleichzeitig; diese externe Summierung kann weiterhin den DAW-Ausgang übersteuern. Normalize verarbeitet ausschließlich das Audio der internen 125A-Sampler-Engine, nicht das Quell-VST.

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
