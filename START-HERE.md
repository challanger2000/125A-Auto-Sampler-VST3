# START HERE — 125A AutoSampler P003 (Testversion)

## Ziel
Jedes VST-Instrument bleibt ganz normal in Fender Studio Pro geladen. Die beiden 125A-VST3-Komponenten übernehmen MIDI-Sequenz und Audioaufzeichnung. **Kein eigener Plugin-Host**, kein Mapping/Import in andere Sampler.

## Windows-Installation
1. Den aktuellen erfolgreichen [P003-Windows-Build](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions) öffnen und das Artefakt `125A-AutoSampler-P003-Win64` herunterladen.
2. Die komplette VST3-Bundle-Struktur in `C:\Program Files\Common Files\VST3\` ablegen.
3. Fender Studio Pro neu starten und `125A AutoSampler MIDI` sowie `125A AutoSampler Recorder` im Plugin-Browser prüfen.
4. **Nur in einem Testprojekt verwenden.** Diese Version ist noch nicht in Fender Studio Pro freigegeben.

## Einmaliges DAW-Routing
1. Auf einer eigenen Instrumentenspur `125A AutoSampler MIDI` laden.
2. Auf einer zweiten Instrumentenspur z. B. EVM Isotope laden.
3. MIDI-Ausgang der 125A-Spur als MIDI-Eingang des Quellinstruments wählen.
4. `125A AutoSampler Recorder` als letzten Stereo-Audio-Insert auf die Quellinstrumentenspur setzen.
5. In der generischen Parameteranzeige der MIDI-Komponente zunächst die Standardwerte verwenden; **Run = On** und die DAW-Wiedergabe starten.
6. Die WAVs erscheinen bei erfolgreichem Routing unter `%USERPROFILE%\Documents\125A AutoSampler\Run_<Zeit>_G<Nummer>_<Zähler>\`, etwa `N60_V100_RR01.wav` bis `N60_V100_RR08.wav`.

**Wichtig:** MIDI-Notennummer 60 ist eindeutig; die zugehörige Oktavbezeichnung C3/C4 hängt von der DAW-Anzeige ab.

## Parameter der 125A-MIDI-Komponente
| Parameter | Werte | Standard |
|---|---|---|
| Run (Play in DAW) | Aus / Ein | Aus |
| First MIDI note | 0–127 | 60 |
| Last MIDI note | 0–127 | 60 |
| Note step | 1–12 Halbtöne | 1 |
| Round Robins | 1–16 | 8 |
| Velocity layers | 1–3 | 1 |
| Note hold | 0,25–8 s | 1 s |
| Release capture | 0,25–10 s | 2 s |
| Gap | 0,05–0,5 s | 0,1 s |

Velocity-Layer sind derzeit fest als 100 (ein Layer), 60/110 (zwei) oder 40/80/120 (drei) definiert. Parameteränderungen während eines laufenden Jobs wirken **erst beim nächsten Start**. Run für Neustart zunächst aus- und dann wieder einschalten. Beim Projekt-Reload bleibt Run aus.

## Nachprüfbare Abnahme
- Standalone-Windows-Kerntests: MIDI-Timing, RR/Velocity-Mapping, WAV-Header, Stilleerkennung.
- Simulierter Batch-Test: 2 Noten × 3 Velocity-Layer × 8 RR = **48 separat erzeugte WAVs**, ohne doppelte Dateinamen oder verkürzte PCM-Daten.
- VST3 SDK-Windows-Build + offizieller Steinberg-Validator für beide registrierten Klassen.
- **Offen:** Test in Fender Studio Pro mit realer MIDI-Weiterleitung und Audioaufnahme.
- **Offen:** GUI, individuelle Velocity-Werte, Error-UI, Multi-Pair-Betrieb, Studio-Pro-/ASIO-Praxistest.

Das 125A Plugin Test Lab unterstützt laut P68-Dokumentation bei automatischer Erkennung aktuell nur eindeutig wählbare Einzel-Prozessorklassen. Unser VST3 enthält **zwei Klassen**. Die generische automatische P74-Abnahme wird daher nicht als erfolgreich behauptet; dafür brauchen wir einen expliziten Klassen-Auswahlschritt oder eine eigenständige Integrationsprüfung. Der offizielle Steinberg-Validator prüft bereits beide Klassen.

Die WAV-Dateien stammen bei den Regressionstests aus synthetischen Signalen, **nicht** aus dem EVM-Isotope-VST. Ein erfolgreicher Build ist keine praktische DAW-Abnahme.
