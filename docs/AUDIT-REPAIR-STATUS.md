# Audit-Reparaturregister A01–A15 – 125A AutoSampler

**Datum:** 2026-11-14. **Original-Audit:** Produktstand `1a1d88cd4c8cc81245d7156761ed0146f5461e89`. **Produkt:** Windows x64 / Steinberg VST3, PR #2 Draft. **Urteil:** **NO-GO** für P025 (endgültige GUI), FULL AUDIT und kommerzielles Release, bis unabhängige Gegenprüfung und alle erforderlichen Nachweise vorliegen.

[Unabhängiger Originalaudit im PR](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2#issuecomment-6876621233). Dort sind 15 Befunde, davon 5 kritisch, mit einzelnen reproduzierten Nachweisen sowie statischen Hypothesen dokumentiert. **Keine Umdeutung von Core- oder Positivtests zu echter DAW/ASIO-Abnahme.**

## Implementierte gezielte Korrekturen / geforderte erneute Prüfung

| Audit-ID | Implementierte Gegenmaßnahme | Aktueller Nachweisumfang und offene Schranke |
|---|---|---|
| **A01 KRITISCH** | Rejected-State-Fingerprint wird beim Autosave nicht neu aus der Datei legitimiert; importierter Fingerprint aus einem geöffneten ZIP-Handle | Native VST3 prüft Fingerprint 0 und falsch-nonzero über mehrere Save/Recall-Zyklen. Verschieben und konkurrierender Datei-Austausch nur teilweise geprüft |
| **A02 KRITISCH** | Exklusiver Generator/Recorder-Lease; Session-Epoch verhindert verspätete Fremdpublikationen; kohärentes Completion-Ereignis | Zweite VST3-Instanz scheitert im technischen Host absichtlich; echte zwei DAW-Projekte mit Songwechsel und Editor-Recovery **NOT RUN**. Multi-Paar-Funktionalität **nicht unterstützt** |
| **A03 KRITISCH** | Auswahl-Epoch, Generation und Besitzer; veraltete Autoimporte dürfen manuelle/Recall-Wahl nicht überspielen | **Windows-VST3 PASS:** realer Hintergrund-Importworker vor dem ZIP-Load angehalten, explizite DAW-Bankauswahl gesetzt, Worker freigegeben; gespeicherter Bankpfad und ausgewählte PCM bleiben erhalten. GUI-Dateiauswahldialog, erzwungene Worker-Wiederaufnahme als gesonderter Nachweis und unabhängige Codex-Regression **NOT RUN** |
| **A04 MITTEL** | Alte Banks durch Off-Audio-Thread-Retirement freigeben; statt 16 Gesamtversionen Speicherbudget | **110 Core-Bankwechsel und 110 native Windows-VST3 Save/Recall-Zyklen PASS** auf dem verifizierten Produktcommit; Audio-/GUI-Race und Langzeitstress **NOT RUN** |
| **A05 KRITISCH** | Aggregiertes 512-MiB-Decoding-Limit, ZIP-Intervall-Überlappungsschutz, Catch-Grenzen um Worker; exklusiver ZIP-Schreibstagingordner | Größen-/Core- und zwei parallele Win32-ZIP-Schreiber geprüft; kontrollierte `bad_alloc`-, OOM- und volle Disk-Fehlerinjektion **NOT RUN** |
| **A06 KRITISCH** | PCM24 als `.partial`, `flush`, `close`, Längenprüfung vor Rename. Report muss vor Bank-Ready ebenfalls erfolgreich schließen | Auslösbarer später `ostream::sync`-Fehler per Core-Streambuf; echte Windows-Quota-/Volume-Faults **NOT RUN** |
| **A07 MITTEL** | Voice-Reset bei erfolgreichem Recall und Deaktivierung / Rate-Setup | Core-Replacement & State-Tests. Vielfache DAW-/ASIO-Lifecycle-Wechsel **NOT RUN** |
| **A08 MITTEL** | Normalized gain nach tatsächlicher Hüllkurvenbeitrags-Summe; sample-rateabhängige Relaxation | Gestaffelte Core-Attack-Regression, vollständige hörbare 32-Voice-/Pumping-Referenzmessung **NOT RUN** |
| **A09 MITTEL** | Kanonische Sample-Dateinamen und semantische `(note,vel,rr)`-Duplikatprüfung | Core adversariales echtes ZIP plus positive Roundtrips; Fuzzing **NOT RUN** |
| **A10 MITTEL** | Strikter vollständiger bounded JSON-v1-Parser mit exakten Zahlen, Typen, 7 Playback-Werten, doppelten Schlüsseln | Manifest-Positiv-/Negativ-Core und ZIP-Roundtrip; breit angelegtes JSON-Fuzzing **NOT RUN** |
| **A11 MITTEL** | VSTGUI Kick-Aktionen ausschließlich bei aktiver Flanke ausführen | Nativer Win32-Klick auf OCT +/− mit genau einem Oktavsprung; komplette Button-Dialog-/Keyboard-Matrix **NOT RUN** |
| **A12 MITTEL** | Bei verweigertem NoteOff Zustand über Cancel hinweg behalten und beim nächsten Callback erneut schicken | Native EventList verweigert Off im STOP-Test; Host-Deaktivierung ohne einen weiteren Callback weiterhin **NOT RUN** |
| **A13 MITTEL** | In-Block-Segmentierung für Param-Punkte mit echten sampleOffsets und korrekten MIDI-Offset-Rückgaben | Native Windows-VST3 Run OFF/ON/STOP an nichtnull Offsets; detaillierte ADSR/Gain-Rampen/-Interpolation, variierende Blockgrößen **NOT RUN** |
| **A14 MITTEL** | Voice bindet VST3 NoteID und Kanal; NoteOff ist selektiv, Host-ID -1 pitchbasiert | Core getrennte NoteIDs; nativer VST3-Audit-Negativfall nun Pflichtregression. Voice-Stealing/gestufter Declick **NOT RUN** |
| **A15 GERING** | Device-Stem wird **vor** Extension maskiert (`CON_.bass`); UTF-8-Grenzen gewahrt | Win32-Dateierstellung/Name-Tests; umfassende Windows-Locale-/Unicode-Matrix **NOT RUN** |

**Zusätzliche Architekturbegrenzungen:** Ein Paar im DAW-Prozess ist explizites P024-Limit und keine erfolgreiche Mehrinstanz-Implementierung. `.125A` bleibt ein ZIP32-Archiv mit WAVs und Metadaten. Projektstate speichert nur Pfad/ID, nicht PCM. CRC/FNV-Fingerprint ist eine Verwechslungsschutzheuristik, **keine kryptografische Authentifizierung**.

## Nachgewiesene aktuelle Windows-Korrektur (Quellstand `e30e652a5dac8251b429121a585e34ff7e4cd336`)

- [Windows VST3 #43627180524](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43627180524): **SUCCESS** auf exakt diesem Code- und Workflowstand, mit 94/94 Steinberg-Validator, Core-Regressionen, echter VST3 MIDI-Generator/Recorder/PCM24/WAV/ZIP-Integration, P023/A03 deterministischen Worker-Barrieren und nachweislich erkanntem P023-Negativmutant, unabhängigem P22-Inventar und kontrolliertem Produktionsartefakt.
- [Windows Core #43627180558](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43627180558): **SUCCESS** auf demselben SHA.
- Das ZIP-Artefakt `125A-AutoSampler-P024-Win64` enthält bei unabhängiger Archivinspektion genau **eine** `AutosamplerVST3.vst3`-Datei und die VST3-Ressourcen. **Kein** `AutosamplerAuditVST3`, keine Testmutanten. Das QA-Testbundle ist **nicht** Bestandteil dieses Produktionsartefakts.
- Das native GUI-Testprogramm hat außerdem ein eigenes **Use-after-release** im nachgelagerten Diagnose-Logging korrigiert (Parameterzahl vor `controller->release()` zwischengespeichert); dies war ein Fehler des Testhosts, **kein** nachgewiesener Produktions-GUI-Fehler.

## Gate-Matrix und wichtigste fehlende Nachweise

- **Eigene produktive Windows-Regressionen:** VS2022 x64, Steinberg-Validator, Audio/MIDI/Recorder, state/ZIP/GUI tests. Jeweils **nur** auf exakten CI-Commit und die dort ausgeführten Prüfungen beziehen; ein grüner Build ändert die ursprüngliche unabhängige NO-GO-Entscheidung nicht.
- **Unabhängiges Test Lab:** Der gepinnte P74-Host erwartet genau **eine** VST3-Audioklasse und brach bei diesem absichtlich zweiklassigen Bundle bereits in der Inventarisierung ab ([Run #43624006082](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43624006082), **P74 FAIL / NOT SUPPORTED**). Die kompatible unabhängige **P22-ABI-Inspektion** weist **beide** realen Klassen nach; [Windows #43627180524](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43627180524), Artefakt `125A-AutoSampler-P22-Independent-MultiClass-Inventory`, **P22 PASS**. Eigenständige unabhängige Funktions-, Zustands-, Audio- und FULL-Audit-Abnahme daraus **nicht** ableiten.
- **P023 erzwungener Save-before-import:** **PASS** in realem Windows-VST3 mit *test-only* Worker-Barriere **vor** ZIP-Import: spielbare Bank zum Savezeitpunkt stumm, Projektstate enthält exakt die bereits fertiggestellte `.125A` samt Fingerprint, anschließend Recall und hörbare PCM-Ausgabe. Ein separat eingeschalteter **Negativmutant**, der diesen Pending-Save-Fallback absichtlich auslässt, wurde durch das gleiche Native-Assertion-System zuverlässig **erkannt (erwarteter FAIL)**. Ebenso **A03 explizite DAW-Auswahl während pausiertem Worker PASS**. Belege: [Windows #43627180524](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43627180524). Die Barriere/Mutations-Exports werden **nur** in der separaten QA-DLL kompiliert, nicht in der produktiven DLL. Eine unabhängige Codex-Nachprüfung fehlt weiterhin.
- **P74/P75 kompletter Produkt-Funktionsvertrag / P67-Abdeckung:** **NOT COVERED** ohne explizit geprüfte P65/P67-Testpläne; P75 ist nicht automatisch aus P74 abgeleitet.
- **Realer Fender Studio Pro + Fremd-VST + echter ASIO-Treiber:** **NOT RUN**. Cloud-Windows-Runner besitzt keine physische ASIO-Hardware.
- **Unabhängiger Codex-Re-Audit auf *diesem* reparierten Branch samt adversarialer Regressionen:** **NOT RUN**.
- **Finale P025 Hardware-GUI:** **nicht entwickelt**, daher **NOT RUN**.

**Freigabekriterium:** Nachweisbare Behebung der kritischen Probleme mit gezielt negativen und positiven Regressionen auf dem aktuell gleichen Produktcommit, unabhängiger Full-Review gemäß `125A-Engineering/QA/AUDIT-COMPLETENESS-CONTRACT.md`, danach freigebene GUI und zuletzt Host/ASIO-Praxis. Fehlende oder nicht ausgeführte Punkte blockieren FULL AUDIT und Release.

**Keine Änderung von `main`, Release, Tag, kommerziellem ZIP oder Gumroad durch diesen Reparaturzweig.**
