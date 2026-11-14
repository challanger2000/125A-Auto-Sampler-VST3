# Audit-Reparaturregister A01–A15 – 125A AutoSampler

**Datum:** 2026-11-14. **Original-Audit:** Produktstand `1a1d88cd4c8cc81245d7156761ed0146f5461e89`. **Produkt:** Windows x64 / Steinberg VST3, PR #2 Draft. **Urteil:** **NO-GO** für P025 (endgültige GUI), FULL AUDIT und kommerzielles Release, bis unabhängige Gegenprüfung und alle erforderlichen Nachweise vorliegen.

[Unabhängiger Originalaudit im PR](https://github.com/challanger2000/125A-Auto-Sampler-VST3/pull/2#issuecomment-6876621233). Dort sind 15 Befunde, davon 5 kritisch, mit einzelnen reproduzierten Nachweisen sowie statischen Hypothesen dokumentiert. **Keine Umdeutung von Core- oder Positivtests zu echter DAW/ASIO-Abnahme.**

## Implementierte gezielte Korrekturen / geforderte erneute Prüfung

| Audit-ID | Implementierte Gegenmaßnahme | Aktueller Nachweisumfang und offene Schranke |
|---|---|---|
| **A01 KRITISCH** | Rejected-State-Fingerprint wird beim Autosave nicht neu aus der Datei legitimiert; importierter Fingerprint aus einem geöffneten ZIP-Handle | Native VST3 prüft Fingerprint 0 und falsch-nonzero über mehrere Save/Recall-Zyklen. Verschieben und konkurrierender Datei-Austausch nur teilweise geprüft |
| **A02 KRITISCH** | Exklusiver Generator/Recorder-Lease; Session-Epoch verhindert verspätete Fremdpublikationen; kohärentes Completion-Ereignis | Zweite VST3-Instanz scheitert im technischen Host absichtlich; echte zwei DAW-Projekte mit Songwechsel und Editor-Recovery **NOT RUN**. Multi-Paar-Funktionalität **nicht unterstützt** |
| **A03 KRITISCH** | Auswahl-Epoch, Generation und Besitzer; veraltete Autoimporte dürfen manuelle/Recall-Wahl nicht überspielen | Core prüft Event-Snapshot inklusive konkurrierendem Lesen; reproduzierbar blockierter nativer Bank-Worker/Save-before-import **NOT RUN** |
| **A04 MITTEL** | Alte Banks durch Off-Audio-Thread-Retirement freigeben; statt 16 Gesamtversionen Speicherbudget | 110 Core-Bankwechsel ausgeführt; Audio-/GUI-Race und Langzeitstress **NOT RUN** |
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

## Gate-Matrix und wichtigste fehlende Nachweise

- **Eigene produktive Windows-Regressionen:** VS2022 x64, Steinberg-Validator, Audio/MIDI/Recorder, state/ZIP/GUI tests. Jeweils **nur** auf exakten CI-Commit und die dort ausgeführten Prüfungen beziehen; ein grüner Build ändert die ursprüngliche unabhängige NO-GO-Entscheidung nicht.
- **Unabhängiger P74-Test-Lab-Host:** Workflow integriert auf festgepinntem Test-Lab-Source-Commit `7213fe1f81f4036f74431cb5b2d7dbd09b73ee5c`; Ergebnis anhand des aktuellen Windows-Run/Reports prüfen. `TECHNICAL_BASELINE_PASS` beweist ausschließlich die generische native Baseline, **keine** P67 vollständige Feature-Abdeckung.
- **P023 erzwungener Save-before-import** (Bank-Worker durch Barriere angehalten): **NOT RUN**, daher darf diese Aussage nicht als PASS gelten.
- **P74/P75 kompletter Produkt-Funktionsvertrag / P67-Abdeckung:** **NOT COVERED** ohne explizit geprüfte P65/P67-Testpläne; P75 ist nicht automatisch aus P74 abgeleitet.
- **Realer Fender Studio Pro + Fremd-VST + echter ASIO-Treiber:** **NOT RUN**. Cloud-Windows-Runner besitzt keine physische ASIO-Hardware.
- **Unabhängiger Codex-Re-Audit auf *diesem* reparierten Branch samt adversarialer Regressionen:** **NOT RUN**.
- **Finale P025 Hardware-GUI:** **nicht entwickelt**, daher **NOT RUN**.

**Freigabekriterium:** Nachweisbare Behebung der kritischen Probleme mit gezielt negativen und positiven Regressionen auf dem aktuell gleichen Produktcommit, unabhängiger Full-Review gemäß `125A-Engineering/QA/AUDIT-COMPLETENESS-CONTRACT.md`, danach freigebene GUI und zuletzt Host/ASIO-Praxis. Fehlende oder nicht ausgeführte Punkte blockieren FULL AUDIT und Release.

**Keine Änderung von `main`, Release, Tag, kommerziellem ZIP oder Gumroad durch diesen Reparaturzweig.**
