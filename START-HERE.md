# START HERE – 125A AutoSampler P015

## Ziel: aufnehmen und mit 125A selbst spielen

**125A AutoSampler ist gleichzeitig Aufnahme-Automation und Sampler-Instrument.** Das Ziel-VST bleibt in Fender Studio Pro; unsere VST3 enthält zwei Klassen aus **einem Bundle**:

1. `125A AutoSampler` – Noten an Quell-VST senden und fertig gesampelte WAVs selbst über Stereo-Ausgang abspielen.
2. `125A AutoSampler Recorder` – Audio des Quell-VST abgreifen und zuverlässige WAV-Dateien exportieren.

Kein SFZ-Export oder fremder Sampler gehört zum notwendigen Workflow. Das interne Instrument lädt PCM24-Stereo-WAVs direkt.

## Windows-Installation

- Auf dem [erfolgreichen P015-Windows-Testlauf](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43567864929) unter **Artifacts** das ZIP `125A-AutoSampler-P015-Win64` herunterladen (P015 mit kurzen Werkseinstellungen, Normalize, One-Shot/Gate, Amp ADSR und einfacher GUI).
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

1. Nach einem vollständig erfolgreichen Run lädt der 125A die gerade aufgenommenen WAVs **von allein**, auch bei geschlossenem Pluginfenster.
2. Zum manuellen Laden einer anderen Bank auf **LOAD SAMPLES** klicken und deren konkreten `Run_<Zeit>_G<Generation>_<Zähler>`-Ordner wählen (z. B. mit `N60_V100_RR01.wav`).
3. Bei geöffnetem Editor bestätigt `SAMPLER: ... WAVS READY TO PLAY` die geladene Bank.
4. **RUN Aus** schalten. Dann MIDI-Noten an die 125A-Instrumentenspur senden: Der 125A liefert selbst Stereo-Audio.

Die interne Engine ordnet Sample-Root-Noten, Velocity und Round Robins direkt zu, transponiert auf nicht gesampelte Zwischentöne und spielt mit bis zu **32 Stimmen**. Der aufgenommene Release wird One-Shot bis zum Sampleende wiedergegeben; echte Sustain-Loops existieren noch nicht.

### P015 – kurze Vintage-Sounds bearbeiten

- **NORMALIZE (-1 dBFS):** standardmäßig aktiv, errechnet einen gemeinsamen Verstärkungsfaktor für die gesamte Bank. Dadurch bleiben die Lautstärkeunterschiede zwischen Velocity-Layern erhalten. Die Original-WAVs werden nicht verändert.
- **GATE:** standardmäßig aus – One-Shot spielt das Sample bis zum Ende, auch bei MIDI Note-Off. Eingeschaltet wird die Amp-Release-Phase durch Note-Off ausgelöst.
- **AMP ADSR:** Attack, Decay, Sustain und Release über die vier Regler im neuen Playback-Bereich. Die Grundeinstellung lässt den kurzen One-Shot nahezu unverändert.
- **NOTE STEP:** Schrittweite 12 ergibt oktavweise Aufnahme, z. B. C1/C2/C3/C4; 3 oder 6 Halbtonschritte ergeben dichtere Multisamples.
- **Schnelle Werkseinstellung:** 1 Round Robin, 1 Velocity-Layer, NOTE STEP 12, Note Hold 0,5 s, Release-Capture 0,5 s, Pause 0,1 s. Die lange historische 8-Round-Robin-Aufnahme bleibt nur eine einstellbare Option.

Für den ersten DAW-Test ist die bisherige VSTGUI um eine kompakte Playback-Sektion erweitert. Filter, portable Komplettpresets und eventuell Vintage-DAC-Charaktere gehören **nicht** zum bereits getesteten P015-Funktionsumfang.

**Die geladene Bank bleibt nur erhalten, solange die WAV-Dateien am gespeicherten Speicherort vorhanden sind.** Das Projekt speichert den Bank-Ordnerpfad, nicht sämtliche PCM-Audiodaten. **LOAD SAMPLES** ist nur erforderlich, um einen anderen oder älteren Run-Ordner manuell auszuwählen.

## Exportformat und Kontrolle

Standard: `%USERPROFILE%\Documents\125A AutoSampler\Run_<...>\`.

- Pro Take eine `N<Note>_V<Velocity>_RR<01-16>.wav`, Stereo, PCM24.
- `RUN-REPORT.csv` dokumentiert alle Take-Ergebnisse und die Summen.
- Bei vollständigem Run mit acht Round Robins sind acht hörbare WAVs und `SUMMARY,exported,8` zu erwarten.
- Der interne Sampler lädt die WAV-Dateien direkt, **ohne** externe `.sfz`-Bank.
- **CHOOSE FOLDER** und **DEFAULT** setzen den Aufnahme-Ausgabeordner; **LOAD SAMPLES** wählt dagegen den Ordner des abzuspielenden Instruments.

**P015 Windows-PASS:** [GitHub Actions #43567864929](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43567864929) bestätigt den vollständigen Build und die simple GUI mit 18 Parametern, 94/94 Steinberg-Tests, die P015-Core-Tests für Normalize und ADSR und 72/72 WAVs mit erfolgreichem integriertem Playback. Ein vollständiger Fender-Studio-Pro-/ASIO-Praxistest steht weiterhin aus.

**P014 Windows-PASS:** Der native Test lädt nach der automatischen MIDI-/Audio-Aufnahme **keinen** Projektzustand und öffnet **keine** VSTGUI, bevor er die Wiedergabe versucht. Trotzdem liefert der bereits laufende 125A in **allen vier Szenarien** hörbares Stereo-PCM (72/72 exportierte WAVs). Zusätzlich: 94/94 Steinberg-Validator-Prüfungen, Core-Tests und Mid-Take-STOP PASS. [Testlauf](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43564563114). Das ist noch kein musikalischer Praxisnachweis in Fender Studio Pro.

## Offene Grenzen

- Ein echtes Fremd-VST in Fender Studio Pro, dessen MIDI-Routing und die vollständige GUI-Interaktion sind noch nicht praktisch abgenommen.
- Physischer ASIO-Test steht aus.
- Pro DAW-Prozess nur ein Sampling-Generator/Recorder-Paar.
- Automatischer Bank-Import erfolgt auf einem separaten Worker-Thread, auch bei geschlossenem Editor. Ein Instrument kann derzeit höchstens 16 verschiedene Bankversionen nacheinander übernehmen; keine Sustain-Loops.
- [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2) bleibt bis zur Endabnahme Draft.
