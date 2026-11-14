# START HERE – 125A AutoSampler · P001–P024 Reparaturzweig (NICHT FINAL)

## Ziel: aufnehmen und mit 125A selbst spielen

**125A AutoSampler ist gleichzeitig Aufnahme-Automation und Sampler-Instrument.** Das Ziel-VST bleibt in Fender Studio Pro; unsere VST3 enthält zwei Klassen aus **einem Bundle**:

1. `125A AutoSampler` – Noten an Quell-VST senden und fertig gesampelte WAVs selbst über Stereo-Ausgang abspielen.
2. `125A AutoSampler Recorder` – Audio des Quell-VST abgreifen und zuverlässige WAV-Dateien exportieren.

Kein SFZ-Export oder fremder Sampler gehört zum notwendigen Workflow. Das interne Instrument lädt PCM24-Stereo-WAVs direkt.

**WARNUNG:** Die frühere P024-Version wurde nach unabhängiger Prüfung mit 15 Auditbefunden (5 kritisch) als **NO-GO** bewertet. Der aktuelle Branch enthält technische Reparaturen, ist aber nicht freigegeben. Insbesondere ist der vollständige unabhängige Nachaudit nicht bestanden. Die endgültige GUI P025 fehlt. [Aktueller Reparatur- und Testnachweis](docs/AUDIT-REPAIR-STATUS.md).

## Windows-Installation

- **Entwicklertests, kein Release:** Historischer [P023-Lauf #43583118459](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43583118459) bestand die damalige Matrix, konnte aber den tatsächlichen Save-before-import-Zeitpunkt **nicht nachweisen**. Neue Windows-Testartefakte im Draft-PR sind ausschließlich für kontrollierte technische Tests bestimmt. Nicht als kommerziell oder vollständig abgenommen ausgeben.
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

1. Nach einem vollständig erfolgreichen Run erzeugt der Recorder die PCM24-WAVs und zusätzlich ein ZIP-basiertes Instrument `<Instrumentenname>.125A` (bei erneuter Aufnahme automatisch `<Instrumentenname> (2).125A` usw.) im gewählten Ausgabeordner. Der Sampler lädt die fertige `.125A` **von allein**, auch bei geschlossenem Pluginfenster.
2. Zum manuellen Laden eines kompletten Instruments auf **LOAD INSTRUMENT** klicken und die `*.125A` auswählen. **LOAD SAMPLES** bleibt für ältere Run-Ordner mit `N60_V100_RR01.wav` usw. vorhanden.
3. Bei geöffnetem Editor bestätigt `SAMPLER: ... WAVS READY TO PLAY` die geladene Bank.
4. **RUN Aus** schalten. Dann MIDI-Noten an die 125A-Instrumentenspur senden: Der 125A liefert selbst Stereo-Audio.
5. **Wichtig bei der Klangkontrolle:** Die ursprüngliche Instrumentenspur B nach dem Sampling stummschalten bzw. Monitoring deaktivieren. Wenn Quell-VST und 125A gleichzeitig hörbar bleiben, addieren sich beide Signale im DAW-Mixer und können ebenfalls hörbar übersteuern. Beim nächsten Aufnahme-Durchlauf die Quellspur wieder aktivieren.

Die interne Engine ordnet Sample-Root-Noten, Velocity und Round Robins direkt zu, transponiert auf nicht gesampelte Zwischentöne und spielt mit bis zu **32 Stimmen**. Der aufgenommene Release wird One-Shot bis zum Sampleende wiedergegeben; echte Sustain-Loops existieren noch nicht.

### P016 – Normalize-Übersteuerung behoben

Bei aktiviertem Normalize werden gleichzeitige 125A-One-Shot-Stimmen jetzt **linear mit gemeinsamem Headroom** summiert, statt ein über 0 dBFS liegendes Signal im Sampler hart abzuschneiden. Eine einzelne Stimme behält ihren Zielpegel; weitere Stimmen erhalten summensichere Reserve. Der Float-Ausgang wird nicht mehr intern abgeschnitten. Normalize beeinflusst **nur die 125A-Wiedergabe**, niemals den Recorder, den Original-Synthesizer oder dessen Insert-Signal.

### P015 – kurze Vintage-Sounds bearbeiten

- **NORMALIZE (-1 dBFS):** standardmäßig **aus**, errechnet beim Einschalten einen gemeinsamen Verstärkungsfaktor für die gesamte Bank. Dadurch bleiben die Lautstärkeunterschiede zwischen Velocity-Layern erhalten. Die Original-WAVs werden nicht verändert.
- **ONE SHOT:** Standardmäßig **AN**. Das Sample spielt nach Note-Off bis zum Ende weiter. Bei **AUS** löst Note-Off die Amp-Release-Phase aus. Ältere DAW-Projekte mit GATE werden beim Laden automatisch migriert.
- **AMP ADSR:** Attack, Decay, Sustain und Release über die vier Regler im neuen Playback-Bereich. Die Grundeinstellung lässt den kurzen One-Shot nahezu unverändert.
- **NOTE STEP:** Schrittweite 12 ergibt oktavweise Aufnahme, z. B. C1/C2/C3/C4; 3 oder 6 Halbtonschritte ergeben dichtere Multisamples.
- **Schnelle Werkseinstellung:** 1 Round Robin, 1 Velocity-Layer, NOTE STEP 12, Note Hold **1,0 s**, Release-Capture **2,0 s**, Pause 0,1 s. Die lange historische 8-Round-Robin-Aufnahme bleibt nur eine einstellbare Option.

Die derzeitige **funktionale Test-VSTGUI (nicht die endgültige P025-Oberfläche)** ist in drei Bereiche gegliedert: **SAMPLING**, **SAMPLE DISPLAY** (echte PCM-Wellenform der auf der Tastatur ausgewählten bzw. zuletzt per MIDI gespielten Root-Note) und **PLAYBACK** (vier silberne ADSR-Drehregler, Gain, Normalize, One Shot). Darunter ist Steinbergs MIDI-Tastatur mit goldenen Markierungen aufgenommener Grundtöne und **OCT − / OCT +**. Keine Filter, kein Sample-Map-Editor, kein Fremd-Sampler.

**Projektwiederherstellung:** Die DAW speichert Plugin-Parameter, Instrumentennamen, den Archivpfad und seit P022 eine **64-Bit-Prüfkennung aus sämtlichen WAV-Einträgen und dem Manifest**. Existiert der ursprüngliche Pfad nicht mehr, sucht das Plugin nur in bekannten 125A-Ordnern (gewählter Ausgabeordner, `Dokumente/125A AutoSampler` und `Dokumente`) nach einem Archiv mit **passender Gesamtkennung**. Eine gleichnamige Bank mit anderen WAV-Daten wird abgewiesen. Das Speichern direkt nach dem Aufnahmelauf vor abgeschlossenem RAM-Import ist konstruktiv vorgesehen, der zeitlich erzwungene native Interleaving-Nachweis bleibt jedoch **NOT RUN**. Bei anderen Umzügen **LOAD INSTRUMENT** wählen. Eine unbeschränkte Dateisuche oder ein im DAW-Projekt eingebettetes Sample-Archiv ist nicht implementiert. **Für einen Rechnerwechsel die .125A-Datei zusätzlich zum DAW-Projekt sichern und mitnehmen.** Projektzustände ab P022 ohne gültige PCM-Prüfkennung laden aus Sicherheitsgründen keine gleichnamige .125A automatisch, sondern verlangen eine erneute explizite Auswahl über LOAD INSTRUMENT.

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

**P018 Windows-PASS:** Der neue Stand besteht den unabhängigen ZIP-Kompatibilitätstest mit dem .NET-Standard-ZIP-Leser; ein einzelnes PCM24-Sample wird aus der `.125A` tatsächlich extrahiert. P019 hat zudem den doppelten Archivabschluss beim Plugin-Teardown beseitigt (exakt ein Archiv pro Aufnahme). Das vorhandene Windows-Testlabor bestätigt die native Recorder-GUI (IDLE/READY/RECORDING), die Main-VSTGUI und die MIDI-/Audio-Integration. Bei einem realen Fender-Studio-Pro-DAW-Test muss insbesondere das Anklicken und MIDI-Leuchten der Tastatur noch überprüft werden.

## Offene Grenzen

- Ein echtes Fremd-VST in Fender Studio Pro, dessen MIDI-Routing und die vollständige GUI-Interaktion sind noch nicht praktisch abgenommen.
- Physischer ASIO-Test steht aus.
- Pro DAW-Prozess nur ein Sampling-Generator/Recorder-Paar.
- Automatischer Bank-Import läuft außerhalb des Audiothreads, auch bei geschlossenem Editor. Die starre 16-Versionen-Grenze wurde durch Bank-Retirement/Speicherbudget ersetzt; umfangreiche Ressourcen-/Nebenläufigkeitsabnahme fehlt. Keine Sustain-Loops.
- [PR #2](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2) bleibt bis zur Endabnahme Draft.
