# P006 – reale VST3-Integrationsmatrix

## Zweck
Kein eingebauter Plugin-Host im Produkt. Der nur unter `tests/vst3_integration.cpp` vorhandene QA-Host lädt ausschließlich unsere beiden tatsächlich gebauten VST3-Klassen. Ein synthetischer Oszillator liefert reproduzierbare PCM-Daten als Ersatz für das erst bei der DAW-Abnahme zu verwendende Fremd-VST.

## Drei Windows-Prüffälle

| Prüf-Fall | Samplerate | Audioblock | Note(n) | Velocity | Round Robins | Erwartete WAVs |
|---|---:|---:|---|---|---:|---:|
| P006-A | 44.100 Hz | 64 | 60 | 100 | 8 | 8 |
| P006-B | 48.000 Hz | 1024 | 60 | 100 | 8 | 8 |
| P006-C | 44.100 Hz | 512 | 60 und 61 | 40, 80, 120 | 8 | 48 |

Für alle Fälle wird die echte VST3-Ereignisliste ausgelesen. Prüfungsumfang:
- Exakte Anzahl und MIDI-Notenzuordnung der Note-On-/Note-Off-Ereignisse einschließlich eindeutiger Note-IDs.
- Die tatsächliche Recorder-Instanz erhält Stereo-Audio. Der Insert-Passthrough bleibt im Floatbereich unverändert.
- Jede WAV ist mindestens ausreichend lang, nicht stumm, stereo, 24-Bit PCM und RIFF-konform.
- Dateinamen werden gegen eine vollständige Sollmenge geprüft. **Doppelte, fehlende und unerwartete WAV-Dateien werden als Fehler behandelt.**
- RUN-REPORT.csv bestätigt die jeweilige Anzahl erfolgreicher WAVs sowie `rejected=0` und `droppedFrames=0`.
- Der echte Windows-GUI-Editor wird erstellt, an ein Win32-HWND gebunden, aktualisiert und wieder gelöst.
- Ein Note-Off nach Transport-Stopp und versetzter Songposition muss die Note-ID des zuvor erzeugten Note-On besitzen.

Jeder Prüf-Fall startet in einem eigenen Prozess und verwendet einen isolierten Zielordner. Damit werden Überbleibsel früherer Tests nicht fälschlich mitgezählt.

## Automatischer CI-Nachweis

[GitHub Actions – P006 Windows VST3 Integration Matrix](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml)

Der genaue PASS/FAIL-Status ergibt sich aus dem Workflow. Ein bloßer SDK-Build bedeutet ausdrücklich **noch keinen PASS** der Integrationsmatrix.

## Nicht abgedeckt
- Fender Studio Pro als tatsächlicher Host und seine MIDI-Zwischenspur-Routing-Konfiguration
- EVM Isotope oder ein anderes Drittanbieter-VST als reale Klangquelle
- Ein physisches ASIO-Audiointerface oder messbare Hardware-Latenzen
- Gleichzeitige Nutzung mehrerer unabhängiger Generator-/Recorder-Paare
- Vollständige GUI-Interaktion und optischer Screenshot-Vergleich aller Regler

P006 ist eine automatisierte technische Integration, keine kommerzielle Release-Abnahme.
