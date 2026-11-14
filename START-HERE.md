# START HERE – 125A AutoSampler P018

## Ziel: aufnehmen und mit 125A selbst spielen

**125A AutoSampler ist gleichzeitig Aufnahme-Automation und Sampler-Instrument.** Das Ziel-VST bleibt in Fender Studio Pro; unsere VST3 enthält zwei Klassen aus **einem Bundle**:

1. `125A AutoSampler` – Noten an Quell-VST senden und fertig gesampelte WAVs selbst über Stereo-Ausgang abspielen.
2. `125A AutoSampler Recorder` – Audio des Quell-VST abgreifen und zuverlässige WAV-Dateien exportieren.

Kein SFZ-Export oder fremder Sampler gehört zum notwendigen Workflow. Das interne Instrument lädt PCM24-Stereo-WAVs direkt.

## Windows-Installation

- **Aktuell:** Windows-Testbundle [`125A-AutoSampler-P018-Win64`](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions) aus dem zuletzt erfolgreichen P018-Workflow herunterladen. P018 erweitert P016 um portable `*.125A`-Instrumente, 0-dB-Gain, einen Namen, eine native MIDI-Tastatur und das Recorder-Statusfenster. Auf der Actions-Seite ausschließlich ein **grünes** Build-Artifact verwenden.
- Den entpackten **vollständigen** Ordner `AutosamplerVST3.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
- Fender Studio Pro neu starten und beide 125A-Klassen im Plugin-Browser suchen.

Ein grüner Windows-CI-Lauf bestätigt nur automatisierte Tests, nicht die DAW-/ASIO-Praxisabnahme.

## Aufnahme eines Fremd-VST

1. Eine Instrumentenspur A mit `125A AutoSampler` anlegen.
2. Auf einer separaten Instrumentenspur B das zu samplende Instrument laden.
3. B so einrichten, dass die MIDI-Noten vom **MIDI-Ausgang von 125A AutoSampler** übernommen werden; Monitoring aktivieren.
4. Den `125A AutoSampler Recorder` im Audio-Ausgang von B als **letzten Insert** laden.
5. Im Feld **INSTRUMENT NAME** einen Namen eingeben (Standard: Untitled Instrument). Noten, Schrittweite, Velocity-Layer, Round Robins, HOLD und TAIL einstellen. Ein Mausklick auf die Tastatur wählt eine einzelne Note (FROM = TO).
6. DAW-Transport starten und erst dann den **RUN**-Schalter im 125A einschalten.
7. Auf den Abschluss und den CSV-Bericht warten. Die DAW muss dafür nicht gestoppt werden.
8. Nach Abschluss den **RUN**-Schalter auf Aus schalten.

Ein abgebrochener oder stummer Durchlauf darf nicht als erfolgreich gelten. Ein neuer Run setzt RUN-Aus→Ein voraus; ein DAW-Loop oder DAW-Play allein startet keine erneute Aufnahme.

## Die aufgenommenen Klänge im 125A spielen

1. Nach einem vollständig erfolgreichen Run erzeugt der Recorder die PCM24-WAVs und zusätzlich ein ZIP-basiertes Instrument `<Instrumentenname> [Run_...].125A` im gewählten Ausgabeordner. Der Sampler lädt die fertige `.125A` **von allein**, auch bei geschlossenem Pluginfenster.
2. Zum manuellen Laden eines kompletten Instruments auf **LOAD INSTRUMENT** klicken und die `*.125A` auswählen. **LOAD SAMPLES** bleibt für ältere Run-Ordner mit `N60_V100_RR01.wav` usw. vorhanden.
3. Bei geöffnetem Editor bestätigt `SAMPLER: ... WAVS READY TO PLAY` die geladene Bank.
4. **RUN Aus** schalten. Dann MIDI-Noten an die 125A-Instrumentenspur senden: Der 125A liefert selbst Stereo-Audio.
5. **Wichtig bei der Klangkontrolle:** Die ursprüngliche Instrumentenspur B nach dem Sampling stummschalten bzw. Monitoring deaktivieren. Wenn Quell-VST und 125A gleichzeitig hörbar bleiben, addieren sich beide Signale im DAW-Mixer und können ebenfalls hörbar übersteuern. Beim nächsten Aufnahme-Durchlauf die Quellspur wieder aktivieren.

Die interne Engine ordnet Sample-Root-Noten, Velocity und Round Robins direkt zu, transponiert auf nicht gesampelte Zwischentöne und spielt mit bis zu **32 Stimmen**. Der aufgenommene Release wird One-Shot bis zum Sampleende wiedergegeben; echte Sustain-Loops existieren noch nicht.

### P016 – Normalize-Übersteuerung behoben

Bei aktiviertem Normalize werden gleichzeitige 125A-One-Shot-Stimmen jetzt **linear mit gemeinsamem Headroom** summiert, statt ein über 0 dBFS liegendes Signal im Sampler hart abzuschneiden. Eine einzelne Stimme behält ihren Zielpegel; weitere Stimmen erhalten summensichere Reserve. Der Float-Ausgang wird nicht mehr intern abgeschnitten. Normalize beeinflusst **nur die 125A-Wiedergabe**, niemals den Recorder, den Original-Synthesizer oder dessen Insert-Signal.

### P015 – kurze Vintage-Sounds bearbeiten

- **NORMALIZE (-1 dBFS):** standardmäßig **aus**, errechnet beim Einschalten einen gemeinsamen Verstärkungsfaktor für die gesamte Bank. Dadurch bleiben die Lautstärkeunterschiede zwischen Velocity-Layern erhalten. Die Original-WAVs werden nicht verändert.
- **GATE:** standardmäßig aus – One-Shot spielt das Sample bis zum Ende, auch bei MIDI Note-Off. Eingeschaltet wird die Amp-Release-Phase durch Note-Off ausgelöst.
- **AMP ADSR:** Attack, Decay, Sustain und Release über die vier Regler im neuen Playback-Bereich. Die Grundeinstellung lässt den kurzen One-Shot nahezu unverändert.
- **NOTE STEP:** Schrittweite 12 ergibt oktavweise Aufnahme, z. B. C1/C2/C3/C4; 3 oder 6 Halbtonschritte ergeben dichtere Multisamples.
- **Schnelle Werkseinstellung:** 1 Round Robin, 1 Velocity-Layer, NOTE STEP 12, Note Hold **1,0 s**, Release-Capture **2,0 s**, Pause 0,1 s. Die lange historische 8-Round-Robin-Aufnahme bleibt nur eine einstellbare Option.

Der einfache Funktionseditor bietet Gain (0 dB neutral), die vorhandene Steinberg-VSTGUI-Tastatur und dauerhaft erreichbare ADSR-Werte. Keine Filter, kein Sample-Map-Editor, kein Fremd-Sampler. Die endgültige Hardware-GUI ist separat geplant.

**Projektwiederherstellung:** Die DAW speichert die Plugin-Parameter, den benutzerdefinierten Namen und den **Dateipfad der zuletzt geladenen `*.125A`**. Die WAVs sind innerhalb dieses Archivs transportabel, aber der gespeicherte Pfad ist derzeit absolut. Beim Rechnerwechsel die Datei mitnehmen und nötigenfalls über **LOAD INSTRUMENT** erneut wählen. Eine automatische Suche nach verschobenen Archiven ist noch nicht implementiert.

## Exportformat und Kontrolle

Standard: `%USERPROFILE%\Documents\125A AutoSampler\Run_<...>\`.

- Pro Take eine `N<Note>_V<Velocity>_RR<01-16>.wav`, Stereo, PCM24.
- `RUN-REPORT.csv` dokumentiert alle Take-Ergebnisse und die Summen.
- Bei vollständigem Run mit acht Round Robins sind acht hörbare WAVs und `SUMMARY,exported,8` zu erwarten.
- Bei jedem erfolgreichen Run entsteht auch eine eigenständige `*.125A` (standardkonformes unverschlüsseltes ZIP mit `instrument.json`, allen WAVs und Wiedergabeeinstellungen). Wer ein einzelnes Sample beispielsweise für den Emulator III benötigt, benennt `.125A` in `.zip` um und extrahiert die WAV.
- Der interne Sampler lädt das `.125A` direkt, **ohne** externe `.sfz`-Bank.
- **CHOOSE FOLDER** und **DEFAULT** setzen den Aufnahme-Ausgabeordner; **LOAD INSTRUMENT** wählt ein komplettes `.125A`-Instrument; **LOAD SAMPLES** importiert alte WAV-Run-Ordner.

**P015 Windows-PASS:** [GitHub Actions #43567864929](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43567864929) bestätigt den vollständigen Build und die simple GUI mit 18 Parametern, 94/94 Steinberg-Tests, die P015-Core-Tests für Normalize und ADSR und 72/72 WAVs mit erfolgreichem integriertem Playback. Ein vollständiger Fender-Studio-Pro-/ASIO-Praxistest steht weiterhin aus.

**P014 Windows-PASS:** Der native Test lädt nach der automatischen MIDI-/Audio-Aufnahme **keinen** Projektzustand und öffnet **keine** VSTGUI, bevor er die Wiedergabe versucht. Trotzdem liefert der bereits laufende 125A in **allen vier Szenarien** hörbares Stereo-PCM (72/72 exportierte WAVs). Zusätzlich: 94/94 Steinberg-Validator-Prüfungen, Core-Tests und Mid-Take-STOP PASS. [Testlauf](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43564563114). Das ist noch kein musikalischer Praxisnachweis in Fender Studio Pro.

**P018 Windows-Prüfung:** Für den neuen Stand läuft zusätzlich ein unabhängiger ZIP-Kompatibilitätstest mit dem .NET-Standard-ZIP-Leser. Das vorhandene Windows-Testlabor bestätigt die native Recorder-GUI (IDLE/READY/RECORDING), die Main-VSTGUI und die MIDI-/Audio-Integration. Bei einem realen Fender-Studio-Pro-DAW-Test muss insbesondere das Anklicken und MIDI-Leuchten der Tastatur noch überprüft werden.

## Offene Grenzen

- Ein echtes Fremd-VST in Fender Studio Pro, dessen MIDI-Routing und die vollständige GUI-Interaktion sind noch nicht praktisch abgenommen.
- Physischer ASIO-Test steht aus.
- Pro DAW-Prozess nur ein Sampling-Generator/Recorder-Paar.
- Automatischer Bank-Import erfolgt auf einem separaten Worker-Thread, auch bei geschlossenem Editor. Ein Instrument kann derzeit höchstens 16 verschiedene Bankversionen nacheinander übernehmen; keine Sustain-Loops.
- [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2) bleibt bis zur Endabnahme Draft.
