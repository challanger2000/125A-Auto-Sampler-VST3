# P011 – Automatisch spielbare SFZ-Bank aus den WAV-Aufnahmen

## Was P011 liefert

Nach jedem **vollständig erfolgreichen** Sampling-Durchlauf enthält der bestehende Run-Ordner nun:

- alle einzelnen `N<Note>_V<Velocity>_RR<Nummer>.wav` (Stereo, 24-bit PCM)
- `125A AutoSampler.sfz` als keyboard- und velocity-gemapptes Instrument
- `RUN-REPORT.csv` mit dem Bankstatus `BANK,SFZ_CREATED,125A AutoSampler.sfz`

Die SFZ-Datei ist eine **portable Instrument-Definition**, keine VST3-Presetdatei. Ein vorhandener SFZ-kompatibler Sampler kann die Datei laden, solange die WAV-Dateien daneben liegen.

## Automatisches Mapping

- **Root-Key:** `pitch_keycenter` ist immer die ursprüngliche gesampelte MIDI-Note.
- **Notenbereiche:** `lokey/hikey` füllen das gewählte erste bis letzte MIDI-Notenintervall. Wenn nicht chromatisch gesampelt wurde, gehören Zwischentöne zur jeweils nächsten Originalnote. Bei exakter Gleichheit wird die tiefere Originalnote verwendet.
- **Velocity:** 1–3 verschiedene Werte werden sortiert. Die Grenzen `lovel/hivel` werden an den Mittelpunkten geteilt, mit lückenloser und überlappungsfreier Abdeckung von MIDI-Velocity 1 bis 127.
- **Round Robins:** SFZ-1-Opcodes `seq_length` und `seq_position` bilden die bis zu 16 tatsächlich aufgenommenen Wiederholungen ab.
- **Spielweise:** `loop_mode=one_shot`, damit die vollständige Aufnahme einschließlich des Ausklangs abgespielt wird. P011 erkennt **keine Sustain-Loops**; lange Pad-Noten werden nicht unbegrenzt gehalten.
- **Dateien:** `sample=` verweist relativ auf die zugehörigen WAV-Dateien im gleichen Ordner. Der Run-Ordner bleibt verschiebbar.

## Fehlerbehandlung

**Fail-closed:** Wenn ein Take fehlt, stumm, unvollständig oder beim Schreiben fehlerhaft ist, wird **keine** spielbare SFZ behauptet oder erzeugt. Gleiches gilt für doppelte Velocity-Werte, die keine eindeutigen Layer ergeben. Der CSV-Bericht enthält `BANK,SKIPPED_INCOMPLETE`. Bei einem eigentlichen SFZ-Dateifehler wird `BANK,SFZ_ERROR` protokolliert, der Exportfehlerzähler erhöht und keine halbe SFZ veröffentlicht.

Die Datei wird im **WAV-Hintergrundthread** in einem temporären Pfad erzeugt und danach atomar umbenannt. Die ASIO-Audiocallbacks erledigen keinerlei Dateizugriffe oder SFZ-Textgenerierung.

## Abnahmekriterien

- Core-Tests: 48 Regionen für 2 Noten × 3 Velocity-Layer × 8 RR; exakte Velocity-Grenzen; Noten-Midpoint-Mapping; Abweisung doppelter Velocity-Werte; kein SFZ wenn nur eine der 48 WAVs fehlt; nach vollständigem Export genau eine unveränderte SFZ-Datei.
- Native Windows-VST3-Integration: zusätzlich zur bisherigen 8/8 + 8/8 + 48/48 WAV-Matrix und dem P010-Exportordner werden die vollständigen `sample=`-Referenzen, `seq_length/seq_position`, `pitch_keycenter` und das CSV-Bankergebnis der realen Recorder-DLL geprüft. Manuelles STOP darf keine SFZ erzeugen.
- Steinberg-VST3-Validator, native Win32-GUI und C++-Regressionen bleiben verbindlich.

[Windows-P011-Workflow](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml)

## Weiter offene Produktarbeit

Die SFZ-Bank ist kein direkter Import in jedes beliebige Samplerformat (zum Beispiel kein automatisch erzeugtes DirectWave-, Kontakt- oder SF2-Preset). Auch die Wiedergabe mit einem **externen SFZ-Sampler** und die DAW-Routing-Praxis mit einem echten Fremd-VST wurden noch nicht geprüft. Der native Test überprüft die Dateikonsistenz und SFZ-v1-Struktur. Eine kommerzielle Release-Freigabe folgt daraus nicht.
