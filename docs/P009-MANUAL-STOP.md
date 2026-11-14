# P009 — Manuelles STOP ohne beschädigte Teilaufnahmen

## Ziel

RUN darf in Fender Studio Pro jederzeit abgeschaltet werden, auch mitten in Note-On, während des Release oder vor dem Ende eines langen Sampling-Jobs.

Eine solche Unterbrechung muss:
1. den gerade spielenden MIDI-Ton mit passender Note-Off-ID beenden,
2. den gemeinsamen Sample-Session-Anker deaktivieren,
3. einen unvollständig erfassten Take **verwerfen**, nicht als WAV speichern,
4. den laufenden `RUN-REPORT.csv` automatisch schließen, ohne das VST3 zu entladen,
5. im Editor **ABORTED** statt **COMPLETE** anzeigen,
6. spätere Audioblöcke desselben abgebrochenen Jobs ignorieren und
7. keine erneute Session ohne ausdrücklichen RUN-Aus→Ein-Wechsel erlauben.

## Umsetzung

- Eine atomare `interruptedGeneration`-Markierung signalisiert den manuellen oder hostbedingten Abbruch.
- Der getrennte Dateischreib-Thread verarbeitet seine bereits eingetroffenen Audioframes, meldet den offenen Take als `ABORTED` und schreibt die abschließenden CSV-Zähler.
- Ein `closedGeneration`-Merkmal verhindert, dass verspätete Audioblöcke derselben Session nach dem Abbruch den Bericht wieder öffnen oder doch noch WAV-Dateien schreiben.
- Normal beendete Jobs aus P008 bleiben unverändert: vollständig geschriebene WAVs werden nicht als Abbrüche eingestuft.

## Windows-Integrationstest

Ein nur für QA gebautes Windows-Testprogramm lädt **die beiden tatsächlichen VST3-Klassen**, startet MIDI-Note 60 mit synthetischem Audiomaterial und schaltet RUN nach ca. 0,75 Sekunden ab — also nach Note-On, aber vor dem erwarteten Note-Off und langen Ausklang.

**PASS-Bedingung:** Ein passendes Note-Off wird ausgelöst. Während die Recorder-Instanz noch aktiv ist, liegt ein geschlossener `RUN-REPORT.csv` mit `ABORTED`, `SUMMARY,exported,0`, `SUMMARY,rejected,1` vor. Es darf **keine** WAV-Datei entstehen. Danach darf die DAW weiterlaufen, ohne unerwünschte MIDI-Neustarts.

Parallel bleiben die P008-/P006-Matrixszenarien mit 8 + 8 + 48 vollständig geschriebenen WAVs verpflichtende Regressionstests.

Die konkrete CI-Ausführung findet sich unter [Windows-VST3-P009-Workflow](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/workflows/windows-vst3.yml).

## Nicht abgenommen

Diese native Windows-Integrationsprüfung benutzt ausschließlich eine synthetische Signalquelle. Fender Studio Pro, ein echtes Fremd-VST und ein ASIO-Hardwaretreiber bleiben als finale Praxisabnahme offen.
