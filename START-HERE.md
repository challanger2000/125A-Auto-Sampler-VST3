# START HERE – 125A AutoSampler P013

## Ziel: aufnehmen und mit 125A selbst spielen

**125A AutoSampler ist gleichzeitig Aufnahme-Automation und Sampler-Instrument.** Das Ziel-VST bleibt in Fender Studio Pro; unsere VST3 enthält zwei Klassen aus **einem Bundle**:

1. `125A AutoSampler` – Noten an Quell-VST senden und fertig gesampelte WAVs selbst über Stereo-Ausgang abspielen.
2. `125A AutoSampler Recorder` – Audio des Quell-VST abgreifen und zuverlässige WAV-Dateien exportieren.

Kein SFZ-Export oder fremder Sampler gehört zum notwendigen Workflow. Das interne Instrument lädt PCM24-Stereo-WAVs direkt.

## Windows-Installation

- Auf der Seite der [Windows-VST3-Tests](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml) einen **erfolgreichen** P013-Lauf wählen; unter **Artifacts** das ZIP `125A-AutoSampler-P013-Win64` herunterladen.
- Den entpackten **vollständigen** Ordner `AutosamplerVST3.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
- Fender Studio Pro neu starten und beide 125A-Klassen im Plugin-Browser suchen.

Ein grüner Windows-CI-Lauf bestätigt nur automatisierte Tests, nicht die DAW-/ASIO-Praxisabnahme.

## Aufnahme eines Fremd-VST

1. Eine Instrumentenspur A mit `125A AutoSampler` anlegen.
2. Auf einer separaten Instrumentenspur B das zu samplende Instrument laden.
3. B so einrichten, dass die MIDI-Noten vom **MIDI-Ausgang von 125A AutoSampler** übernommen werden; Monitoring aktivieren.
4. Den `125A AutoSampler Recorder` im Audio-Ausgang von B als **letzten Insert** laden.
5. In A die gewünschten Noten, Schrittweite, Velocity-Layer, Round Robins, Noten- und Release-Dauern einstellen.
6. DAW-Transport starten und erst dann den **RUN**-Schalter im 125A einschalten.
7. Auf den Abschluss und den CSV-Bericht warten. Die DAW muss dafür nicht gestoppt werden.
8. Nach Abschluss den **RUN**-Schalter auf Aus schalten.

Ein abgebrochener oder stummer Durchlauf darf nicht als erfolgreich gelten. Ein neuer Run setzt RUN-Aus→Ein voraus; ein DAW-Loop oder DAW-Play allein startet keine erneute Aufnahme.

## Die aufgenommenen Klänge im 125A spielen

1. Nach einer erfolgreichen Aufnahme wird die WAV-Bank bei geöffneter 125A-Oberfläche automatisch geladen. Bei geschlossener Oberfläche den Editor erneut öffnen. Für andere gespeicherte Runs **LOAD SAMPLES** anklicken.
2. Den konkreten Unterordner `Run_<Zeit>_G<Generation>_<Zähler>` mit seinen WAV-Dateien auswählen, etwa `N60_V100_RR01.wav`.
3. Die Statuszeile `SAMPLER: ... WAVS READY TO PLAY` prüfen.
4. RUN muss **Aus** sein. Nun MIDI-Noten an die 125A-Instrumentenspur senden: Das interne Instrument liefert Stereo-Audio.

Die interne Engine ordnet Sample-Root-Noten, Velocity und Round Robins direkt zu, transponiert auf nicht gesampelte Zwischentöne und spielt mit bis zu **32 Stimmen**. Der aufgenommene Release wird One-Shot bis zum Sampleende wiedergegeben; echte Sustain-Loops existieren noch nicht.

**Die geladene Bank bleibt nur erhalten, solange die WAV-Dateien am gespeicherten Speicherort vorhanden sind.** Das Projekt speichert den Bank-Ordnerpfad, nicht sämtliche PCM-Audiodaten. **LOAD SAMPLES** ist nur erforderlich, um einen anderen oder älteren Run-Ordner manuell auszuwählen.

## Exportformat und Kontrolle

Standard: `%USERPROFILE%\Documents\125A AutoSampler\Run_<...>\`.

- Pro Take eine `N<Note>_V<Velocity>_RR<01-16>.wav`, Stereo, PCM24.
- `RUN-REPORT.csv` dokumentiert alle Take-Ergebnisse und die Summen.
- Bei vollständigem Run mit acht Round Robins sind acht hörbare WAVs und `SUMMARY,exported,8` zu erwarten.
- Der interne Sampler lädt die WAV-Dateien direkt, **ohne** externe `.sfz`-Bank.
- **CHOOSE FOLDER** und **DEFAULT** setzen den Aufnahme-Ausgabeordner; **LOAD SAMPLES** wählt dagegen den Ordner des abzuspielenden Instruments.

Die VST3-Regression arbeitet mit einem synthetischen Testinstrument, lädt anschließend den tatsächlich aufgenommenen WAV-Run **in die 125A-VST3 selbst** und prüft den generierten Audioausgang. Das ist nicht mit einem musikalischen Test in Fender Studio Pro gleichzusetzen.

## Offene Grenzen

- Ein echtes Fremd-VST in Fender Studio Pro, dessen MIDI-Routing und die vollständige GUI-Interaktion sind noch nicht praktisch abgenommen.
- Physischer ASIO-Test steht aus.
- Pro DAW-Prozess nur ein Sampling-Generator/Recorder-Paar.
- Automatische Bankübernahme erfolgt, sobald der 125A-Editor geöffnet ist; keine Sustain-Loops.
- [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2) bleibt bis zur Endabnahme Draft.
