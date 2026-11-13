# START HERE — 125A AutoSampler P002 (Entwickler-Prototyp)

## Was hier bereits funktioniert
- Windows-x64-Build einer **VST3 mit zwei Plugin-Klassen** im selben Bundle:
  - `125A AutoSampler MIDI` — Instrument mit MIDI-Event-Ausgang
  - `125A AutoSampler Recorder` — Stereo-Audioeffekt für einen Insert
- Fester Testablauf: **MIDI-Note 60, Velocity 100, acht Round-Robin-Takes**, jeweils 1 s Note-On, 2 s Release-Capture und 100 ms Pause.
- Eigenständige 24-Bit-Stereo-WAV-Dateien, Trimming und Fades im Hintergrund-Worker.
- Keine Plugin-Hosting-Funktion; das Quellinstrument bleibt in der DAW.
- GitHub Actions Windows-2022 Build und Kern-Regressionstests: bestanden.
  - [Windows Build 43519720850](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43519720850)

## Installation (nur für kontrollierten Test)
1. Bei der oben verlinkten GitHub Action unter **Artifacts** die ZIP `125A-AutoSampler-P002-Win64` abrufen.
2. Den Inhalt so installieren, dass die vollständige `.vst3`-Bundle-Struktur unter `C:\\Program Files\\Common Files\\VST3\\` liegt.
3. Fender Studio Pro neu starten und die beiden Klassen in der Plugin-Liste prüfen.

## Vorgesehener Signalweg — noch nicht in Studio Pro verifiziert
1. Auf einer Instrumentenspur `125A AutoSampler MIDI` laden.
2. EVM Isotope oder das gewünschte Quell-VST auf einer zweiten Instrumentenspur laden.
3. Den MIDI-Ausgang der AutoSampler-Spur als MIDI-Eingang des Quellinstruments wählen.
4. Auf der Quellinstrumentenspur `125A AutoSampler Recorder` als **letztes** Audio-Insert laden.
5. In den generischen Parametern des MIDI-Generators **Run = On** aktivieren und den DAW-Transport starten.
6. Erwartet werden nach den acht Einzelaufnahmen Dateien im Standardordner:
   `%USERPROFILE%\\Documents\\125A AutoSampler\\`
   mit Namen `G1_N60_V100_RR01.wav` bis `...RR08.wav`.

## Grenzen dieser Entwicklerfassung
- **Noch keine** GUI, kein Auswahlfeld für Notenbereich, Velocity-Layer oder WAV-Zielpfad.
- Nur **ein** MIDI-/Recorder-Paar pro Prozess; die zeitliche Kopplung zwischen den beiden Plugin-Klassen benötigt einen echten Studio-Pro-Test.
- Exportfehler werden noch nicht in einer Benutzeroberfläche angezeigt.
- Kein automatischer vollständiger Abnahmetest mit dem 125A Plugin Test Lab.
- Es gibt noch keine Freigabe für den produktiven Einsatz; Sampletreue, Pegel und Release-Enden sind in der DAW noch zu prüfen.
- MIDI-Notennummer 60 ist absichtlich zusätzlich zur Oktavbezeichnung angegeben, weil DAWs die Oktavnummer unterschiedlich anzeigen.

## Abnahmekriterien der nächsten Stufe
1. Beide VST3-Plugin-Klassen werden geladen, ohne Instabilität.
2. Acht Note-On- und acht Note-Off-Ereignisse erreichen das Quellinstrument.
3. Es entstehen acht getrennte WAV-Dateien; keine Dopplung, Überlagerung, Stille oder Überschreibung.
4. Jede WAV enthält Attack, gehaltenen Klang und Ausklang.
5. Stop, Neustart, verschiedene ASIO-Blockgrößen und unterschiedliche Samplerates erzeugen keine beschädigten Dateien.
6. Erst anschließend UI, frei konfigurierbare Noten-/Velocity-Layer und weitere Funktionsausbauten.
