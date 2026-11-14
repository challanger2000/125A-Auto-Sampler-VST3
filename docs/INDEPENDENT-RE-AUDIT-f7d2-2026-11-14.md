# Unabhängige Nachprüfung f7d2 — NO-GO für P025

**Prüfstand:** `challanger2000/125A-Auto-Sampler-VST3`, Entwicklungsbranch `develop/p001-autosampler-foundation`, verbindlicher Produkt-/Test-/Workflowcommit **f7d2d4f017ac37f0ed4847879f1b1205c0ea4de5**. Datum: 14.11.2026. Keine Änderungen an Produktcode, bestehenden Tests, Ressourcen oder Workflows durch den Prüfer.

**Ergebnis:** R02, R03 und R04 bestehen die ursprünglichen lokal ausführbaren Gegenproben; der Standard-GCC14-Releasebuild mit `-O3` funktioniert. R01 ist im Quelltext korrigiert, die native Ausführung hier NOT RUN. **R05 ist für einen Bankwechsel ohne zusätzlichen Reset weiterhin FAIL.** Außerdem überlebt ein Mutant, der den RR-Reset deaktiviert, die gesamte aktuelle reguläre Coretestsuite. Der technische Audit ist wegen fehlender nativer Windows-/CI-Detailnachweise **unvollständig**, kein FULL AUDIT. Das NO-GO beruht auch auf einem reproduzierten Vertragsfehler, nicht allein auf Zugriffsgrenzen.

## Pflichtlektüre, Unabhängigkeit und Umfang

Engineering-Checkout `811f71fa72f51ecf1b9d5121c21f8b8e03302971`: START-HERE erneut vollständig geöffnet; Fetch bestätigt unverändertes Engineering-main gegenüber dem vorigen Prüfteil **derselben Unterhaltung**. Die dort tatsächlich vollständig gelesenen Originaldateien bleiben identisch: README, sämtliche 2757 Zeilen ENGINEERING-GUARDRAILS, QA/AUDIT-COMPLETENESS-CONTRACT; Standards AI-ASSISTED-ENGINEERING, SOURCE-BASELINE-GATE, REPOSITORY-WORKFLOW, PROFESSIONAL-PLUGIN-BEHAVIOR, DSP-GUIDELINES, GUI-GUIDELINES; QA RELEASE-QA, REALTIME-QA, AUDIO-FIXTURES; RESEARCH SOURCES, OPEN-QUESTIONS, REJECTED-APPROACHES; ROADMAP/V3-CANDIDATES und RELEASE-REGISTER. Es wurde kein früheres fremdes PASS als eigener Nachweis übernommen.

Projekt-README, START-HERE und Reparaturregister erneut gelesen, ebenso der gesamte vorherige lokale Bericht `INDEPENDENT-RE-AUDIT-2026-11-14.md`. Alle übrigen historischen Projektdokumente wurden im vorherigen Prüfteil vollständig gelesen; `git diff 716cdf6 f7d2 -- docs README.md START-HERE.md` weist keine Änderungen daran aus. Das unveränderte Reparaturregister verweist noch auf e30e652 und ist **kein aktueller Nachweis für f7d2**.

Die zuvor vollständig gelesenen 17 src-Dateien, beide Tests, beide UIDESCs und CMake-/CI-Dateien bilden die Basis; sämtliche acht seit 716cdf6 geänderten Dateien und ihre betroffenen Aufrufpfade wurden nachgeprüft. Aktuelles Dateihash-/Kontrollinventar: [source-inventory.csv](audit-f7d2-evidence/source-inventory.csv), [control-inventory.csv](audit-f7d2-evidence/control-inventory.csv). Die 32 interaktiven UIDESC-Elemente bleiben hinsichtlich realer Windows-Eingabe NOT RUN. Der ursprüngliche externe A01–A15-Auditkommentar war weiterhin nicht vollständig verfügbar; A-Matrix deshalb anhand Reparaturregister, geprüftem Quelltext und vorhandenen Regressionen, nicht als vollständiger Abgleich aller Originalreproduktionen.

Umgebung: Linux, GNU C++ 14.2.0, CMake 4.4.5, Ninja 1.13.2. Keine lokale Windows-/MSVC-/Win32-/ASIO-Ausführung. SDK-Pin unverändert `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`. Unveränderte historische Befunde werden nicht automatisch als fortbestehende Fehler gewertet.

## Ausgeführte Prüfungen

| Prüfung | Tatsächliches Ergebnis | Nachweis |
|---|---|---|
| Standard-CMake Release, `-O3 -DNDEBUG`, `-Wall -Wextra -Werror` | **PASS**, 1/1 CTest, jetzt 30 aufgerufene Testfunktionen | [build.log](audit-f7d2-evidence/build.log), [ctest.log](audit-f7d2-evidence/ctest.log) |
| Debug, ASan + UBSan + float-cast-overflow | **PASS**, 1/1 CTest | [san-ctest.log](audit-f7d2-evidence/san-ctest.log) |
| Ursprüngliches unverändertes C++-Gegenprobenprogramm, neu gegen f7d2 kompiliert | R03/R04 sowie R05 mit explizitem Reset **PASS** | [Originalquelltext](audit-f7d2-evidence/original-core-counterexamples.cpp), [Ausgabe](audit-f7d2-evidence/original-counterexamples.log) |
| Ursprünglicher Dateiaustausch mit vier 120-MiB-Payloads | Übergröße abgewiesen; R02 **PASS** | [Script](audit-f7d2-evidence/repeat-original-race.py), [Ausgabe](audit-f7d2-evidence/original-race-result.log) |
| Ergänzende Unicodefälle / Velocity-Sortierung | **32 Unicodefälle, 3000 Sortierfälle, 0 Fehler** | [Quelle](audit-f7d2-evidence/extended-core.cpp), [Ausgabe](audit-f7d2-evidence/extended-core.log) |
| Neuer Bankkontext ohne externen Voice-Reset | **FAIL**, erste neue Note 0,7 statt 0,3 | [Quelle](audit-f7d2-evidence/bank-context.cpp), [Ausgabe](audit-f7d2-evidence/bank-context.log) |
| 11 isolierte Mutanten gegen unveränderte aktuelle Testsuite | 10 erkannt, **1 überlebt** | [Resultate](audit-f7d2-evidence/mutation-results.json), [Runner](audit-f7d2-evidence/mutations.py) |

Windows-only Zweige der Coretests (Win32-Dateinamen und konkurrierende exklusive Windows-ZIP-Publikation) wurden nicht ausgeführt. Ein 1/1 CTest-PASS umfasst sie auf Linux nicht. Die Sanitizerprüfung ist keine vollständige Race- oder Realtime-Qualifikation.

Reproduktion des regulären Builds:

```sh
export PATH=/workspace/.autosampler-tools/bin:$PATH
cmake -S /workspace/125A-Auto-Sampler-VST3 -B /workspace/autosampler-reaudit-f7d2/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_VST3=OFF
cmake --build /workspace/autosampler-reaudit-f7d2/release --parallel 4
ctest --test-dir /workspace/autosampler-reaudit-f7d2/release --output-on-failure
```

Sanitizerbuild in eigenem Verzeichnis: Debug plus `-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer` als CXX_FLAGS und `-fsanitize=address,undefined,float-cast-overflow` als EXE_LINKER_FLAGS. Alle Builds/Mutationen fanden außerhalb des Produktcheckouts statt.

## R01–R05: vollständige Nachprüftabelle

| ID | Fundstelle / geprüfte Implementierung | Reproduktion und Soll | Tatsächlich / Bewertung |
|---|---|---|---|
| R01 | `Plugin.cpp:945–958`, `:1647–1654`: eigene setProcessing-Overrides, Instrument infiniteTail, Recorder noTail | Beide Klassen true/false/true ohne process dazwischen; Tail passend zur OneShot-/Passthroughfunktion. Neuer Nativehost `vst3_integration.cpp:214–223` fordert dies | Quellkorrektur nachvollziehbar, keine Allokation/Locks/Join im Handler. **NOT RUN** für die erforderliche native Ausführung; CI-Detailbelege fehlen. Kein nativer PASS behauptet |
| R02 | `BuiltInSampler.h:76–104`, `:131–156`: Restbudget aus demselben geöffneten Handle vor PCMbytes-/PCM-Allokation; tatsächliche dekodierte Bytes aufsummieren | Ursprünglicher Austausch unmittelbar vor Open, ursprüngliche 512-MiB-Konstante, vier tatsächlich große gültige WAVs. Übergröße ablehnen | `accepted=0 decoded=0 limit=536870912`; unverändert große Dateien ebenfalls abgewiesen. **PASS** für ursprünglichen Defekt und getestete Budgetgrenzen |
| R03 | `BuiltInSampler.h:443–457`: Voice-Reset setzt polyMixGain=1 | Unverändertes Originalprogramm: 32 Stimmen, neue Bank/Voice-Reset, neue Note ohne stille Zwischenprobe; frische Referenz 0,25 | `recall_first=0.25 fresh_first=0.25`. **PASS** für expliziten Recall-Reset; keine pauschale Freigabe aller Bank-/Lifecyclepfade |
| R04 | `ManifestJson.h:29`, `:120–145`: Stringabschluss prüft vollständiges UTF-8, Escapes separat dekodiert | Original Byte FF ablehnen; ergänzend Overlongs, verwaiste Continuations, Truncation, Surrogate, >U+10FFFF, internationale gültige Zeichen, gültiges Surrogatpaar | `invalid_utf8_accepted=0`; 15 positive/17 negative Unicodefälle PASS. **PASS** im geprüften Parserumfang |
| R05 | `BuiltInSampler.h:325–329`, `:443–457`: lazy RR-Epoch beim Voice-Reset; `publish:198–223` ändert den RR-Kontext nicht; `Plugin.cpp:632` veröffentlicht ohne Reset | Verbindlicher Auftrag: neuer Projekt-/Bankkontext beginnt RR01. Originalprobe mit requestVoiceReset sowie Ergänzung publish ohne weiteren Reset; normaler RR-Wechsel muss erhalten bleiben | Originalprobe mit explizitem Reset 0,25=Referenz, normale RR01/RR02-Probe PASS. **FAIL** insgesamt: Bankwechsel ohne Reset spielt RR02; außerdem überlebt deaktivierter Epochwechsel die regulären Tests |

**GCC14-Warnung:** `JobParameters.h:43–56` verwendet ein festes Vergleichsnetz für maximal drei aktivierte Layer. Die Standard-`-O3`-Konfiguration baut ohne Warnungsunterdrückung. Die unabhängige Ergänzung vergleicht bei 1/2/3 Layern je 1000 deterministischen Eingaben gegen separat sortierte Vektoren; deaktivierte Velocityfelder bleiben unverändert. **PASS**.

### Originalprogramme wurden erhalten

Das C++-Original wurde byteidentisch wiederverwendet, nicht durch einen neuen Test ersetzt; Hash in [provenance.json](audit-f7d2-evidence/provenance.json). Der Dateiaustauschrunner erhielt ausschließlich einen anderen **Ausgabepfad**, damit historische Belege nicht überschrieben werden; Fixture, Hookposition, Decoder und Budgetkonstante blieben unverändert. Die Hookinstrumentierung betrifft nur eine isolierte Headerkopie direkt vor dem tatsächlichen ifstream-open.

Wichtig zum Exitcode: Der ursprüngliche R02-Runner liefert jetzt **1**, weil sein damaliges Erfolgskriterium „übergroße Bank wurde angenommen“ nicht mehr zutrifft. Seine Ausgabe zeigt die korrekte Ablehnung. Das ist kein fehlgeschlagener Produktfix und kein Testaufbaufehler. Er beweist nicht, dass zu keinem Zeitpunkt irgendein temporärer Speicher allokiert wurde: Die ersten zulässigen WAVs können geladen werden, der die Grenze überschreitende Decoder wird abgewiesen.

## Verbliebener R05-Vertragsfehler — MITTEL

Reproduzierbare Eingabe in `bank-context.cpp` mit unveränderten Produktheadern:

1. Bank A mit zwei RR-Zonen 0,1/0,2 veröffentlichen, Gate mit Release=0 setzen.
2. Eine Note auslösen: RR01=0,1. NoteOff beendet sie vollständig; kein alter Ton bleibt übrig.
3. Bank B mit Zonen 0,3/0,7 veröffentlichen. Wie in loadSamples wird **kein** zusätzlicher requestVoiceReset-Aufruf eingefügt.
4. Neue Note auslösen. Erwartet RR01=0,3, beobachtet **RR02=0,7**; Probe endet mit Exit 1.
5. Kontrollprobe mit explizitem requestVoiceReset ergibt wieder 0,3.

Produktverbindung: GUI-LOAD-INSTRUMENT und LOAD-SAMPLES rufen `loadSamples` auf. Dort folgt auf player_.publish nur Metadaten-/Parameterübernahme; ein RR-Kontextwechsel wird nicht angefordert. Auch der automatische Import verwendet diesen Publishpfad. setState setzt später explizit zurück und besteht deshalb die ursprüngliche Recallprobe. Dies ist ein Corelauf plus direkte Quellzuordnung zum Produktpfad, **kein selbst ausgeführter Windows-GUI-Test**.

**Reparaturempfehlung:** Bankpublikationen müssen eine neue Identität für die RR-Auswahl zukünftiger NoteOns liefern. Diese ist im Audiothread atomar/kohärent zu übernehmen. Die alten ausklingenden Stimmen dürfen entsprechend der dokumentierten Swap-Policy weiterlaufen; nicht unbedacht alle Stimmen abwürgen oder audioeigene RR-Felder vom Controlthread verändern. Teste nach **ungerader** Zahl alter Treffer einen tatsächlich anderen Bankinhalt, 2/3/16 RRs, direkte GUI-/Autoimportpfade und expliziten Recall; Ausgangs-PCM prüfen.

### Nachgewiesene Regressionstestlücke: RR-Reset-Mutant überlebt

Im isolierten Header wurde ausschließlich `if(++rrEpoch_==0)` durch `if(false)` ersetzt. Gain-Reset und Voicebereinigung bleiben aktiv, aber es gibt **keine neue RR-Epoch**. Das ist ein definierter funktionaler Fehler. Die **gesamte aktuelle unveränderte Coretestsuite** kompiliert unter `-O3 -DNDEBUG` und beendet sich dennoch mit **Exit 0**.

Ursache: Der neue R05-Test `tests/core_tests.cpp:1057–1068` spielt genau zwei Treffer in einer Bank mit zwei RRs vor dem Reset. Ohne jeden Reset ist der dritte Treffer ohnehin RR01. Auch die 32-Stimmen-Sequenz der R03-Fixture lässt den RR-Zähler auf einer geraden Position zurück. Die Assertions können fehlenden RR-Reset daher nicht erkennen.

Unabhängige Kontrollprobe: Das **ursprüngliche** unveränderte Gegenprobenprogramm gegen diesen Mutanten liefert `rr_recall=0.75 rr_fresh=0.25` — [Ausgabe](audit-f7d2-evidence/rr-mutant-original-probe.log). Der Fehler ist beobachtbar; die grüne reguläre Suite ist unzureichend. Dieser Nachweis benötigt keinen Windows-Runner.

## A01–A15: vollständige erneute Prüftabelle

Status bezeichnet den gesamten hier genannten Vertrag. Teilweise bestandene Corefälle sind bei unvollständigem Gesamtvertrag ausdrücklich getrennt. Ein offener Fehler wird nicht durch eine grüne Nachbarprobe geschlossen.

| ID | Fundstelle / Implementierung | Reproduktion, Soll und tatsächliches Ergebnis | Status / verbleibende Prüfung |
|---|---|---|---|
| A01 | Plugin.cpp getState/setState/loadSamples; InstrumentArchive inspect/readEntry/Fingerprints/resolve | Reguläre ZIP-/Relink-/CRC-Corefälle erneut PASS; native 0-/falsch-nonzero-Stateproben nur gelesen. Soll: abgelehnte Bank bleibt stumm und erhält keine neue Autorisierung | **INCONCLUSIVE** insgesamt: exakte native Originalfälle, Dateiaustausch während Restore/Save und ABI-OOM nicht unabhängig ausgeführt |
| A02 | Session::acquire/releaseGenerator/Recorder, ownerEpoch; CompletedBankPublication | Snapshot-Corestress erneut PASS; Leases verweigern statisch zweites Paar. Soll: keine projektfremde Bank/verspätete Publikation | **INCONCLUSIVE**: native doppelte Instanz/Sequenz NOT RUN; Recorder-/Generator-Teardown mit angehaltenem alten Writer fehlt |
| A03 | loadSamples Auswahl-Epoch/Generationen; monitorBanks; setState | Fortschrittsbarriere jetzt mit Ack **nach** tatsächlichem loadSamples-Return; Drop-Replay-Mutant und passende Fehlermeldung in YAML. Soll: neuere Auswahl bleibt erhalten | **INCONCLUSIVE**: Testdesign verbessert; nativer Positiv-/Negativlauf nicht selbst ausgeführt, Detailprotokoll gesperrt. Weiteres State-Risiko unten |
| A04 | BuiltInSampler publish/reclaimRetired, ReaderScope/voiceBanks | 110 Wechsel erneut PASS; 16er-Slot-Mutant an passender Assertion erkannt. Soll: weitere Wechsel funktionieren ohne UAF und Callback-Destruktion | **INCONCLUSIVE** insgesamt: regulärer Coreumfang PASS, keine vollständige konkurrierende Audio-/GUI-Reclamation-/Weak-Memoryqualifikation |
| A05 | WAV-Decoderbudget, ZIP-Intervallgrenzen, Worker-Catches | Original R02 nun PASS; deaktivierter Decoderbudgetcheck wird an „insufficient budget before allocation“ erkannt. Soll: begrenzte Allokationen, kein ABI-Exceptionescape | **INCONCLUSIVE** insgesamt: reparierter Budgetfall PASS; native bad_alloc, Threadstartfehler, State-ABI-Exceptions und volle Disk NOT RUN |
| A06 | SampleProcessing writeWav24Stream/writeWav24; ZIP private Staging; Recorder closeRun | WAV-Größe/No-overwrite/Flusherror erneut PASS; Flushignorier-Mutant erkannt. Soll: keine fertige Bank aus unvollständiger Datei oder fehlgeschlagenem Report | **INCONCLUSIVE**: Close-/Rename-/Report-/Cleanupfehler und tatsächliche Windows-Quota-Faults nicht ausgeführt |
| A07 | clearVoicesIfRequested und setState/setActive/setupProcessing | Original R03 PASS; Voice-Reset-deaktiviert-Mutant erkannt. Soll: neuer Kontext ohne alte transienten Zustände | **FAIL** für Bank-RR-Kontext gemäß R05; expliziter Recall-Gainreset PASS; Windows-Lifecycle NOT RUN |
| A08 | renderOne Envelope-Summe/polyMixGain, Reset | 8-Voice und staggered Attack erneut PASS; alter VoiceCount- und Gainreset-Mutant erkannt. Soll: korrekte Normalize-Summe/Recall ohne alte Absenkung | **PASS** für diese Coreverträge; keine Behauptung einer vollständigen musikalischen Pumping-/32-Voice-/DAW-Abnahme |
| A09 | parseSampleName, ZIP-Namen/Semantik/Gruppen | Reguläre gültige Grenzen, Aliase und ZIP-Roundtrip erneut PASS. Soll: keine mehrdeutigen RR-Namen oder lückenhaften Gruppen | **PASS** für ausgeführte Parser-/Aliasfälle; breites ZIP-Fuzzing NOT RUN |
| A10 | ManifestCursor/parse125AManifest | Version10/Count20/Duplicate/Trailing/Format erneut PASS; 32 Unicodefälle PASS; Version- und UTF-8-Mutant erkannt | **PASS** im dokumentierten Parserumfang; keine vollständige Fuzzingbehauptung |
| A11 | valueChanged aktive Flanke, verifyView, UIDESC, HardwareWidgets | Statische Routen unverändert; native OCT±-/Dialog-/Keyboardprüfungen nicht ausführbar | **NOT RUN** für reale Eingaben, Default-Reset, Wheel, Dialoge und Multi-Editor |
| A12 | ActiveNote, sendPendingNoteOff, cancel, process | Tracker-Core erneut PASS. Native STOP-Fixture verweigert einen Off und erwartet folgenden. Soll: passendes NoteOff bleibt bis Hostannahme ausstehend | **INCONCLUSIVE**: nativer Lauf NOT RUN; Nullframe-Stop/mehrfache Abweisung/Deaktivierung ohne nächsten Callback offen |
| A13 | process ParamCursor/processSegment/renderSampler | Native Run-Offsetprobe statisch unverändert; soll alle Punkte auf Sampleoffset anwenden | **NOT RUN** nativ; Gain-/ADSR-Rampen, Held-Voice-Automation, Partitionen, Nullframes und Sättigung nicht durch Run-Test nachgewiesen |
| A14 | BuiltInSampler noteOn/noteOff NoteID+Kanal | Identische Pitches, verschiedene IDs Core PASS; Pitch-only-Mutant erkannt | **PASS** für ID-Selektionsprobe; komplette Kanal/-1/Steal/Declick-Nativmatrix NOT RUN |
| A15 | safe125AStem, UTF-8-Grenzen, Device-Stem vor Extension | Stringfälle erneut PASS; unmaskierter Gerätename-Mutant erkannt | **PASS** für Strings; tatsächliche Win32-/Locale-Matrix NOT RUN |

### Weiter offenes State-/Auswahlrisiko (HOCH, native Reproduktion INCONCLUSIVE)

Die neue Prüfung `Plugin.cpp:617–618` verwirft veraltete **explizite** Loads mit Rückgabe false. Dies schützt den unmittelbaren Publishpfad. Der aufrufende `setState` behandelt false in `:1157–1170` jedoch unterschiedslos als Ladefehler und ruft `player_.clearCurrent()` auf, setzt bankFolder zurück auf den alten angeforderten Pfad und schreibt danach alte Parameter.

Konkreter aus dem Quelltext abgeleiteter Ablauf: Restore A wartet im Decoder; neuer expliziter Load B erhöht die Epoch und veröffentlicht B; A erkennt beim Commit die neue Epoch und gibt false zurück; sein äußerer setState-Fehlerzweig löscht B. Das neue „stale explicit“-Guard allein ist daher kein vollständiger transaktionaler Recallvertrag. **Nicht im echten VST3 ausgeführt**, deshalb kein behaupteter gemessener PCM-/DAW-Fehler. Erforderlich: Unterscheidung „fehlgeschlagen“ vs. „durch neueren Intent ersetzt“ und erneute Commit-Arbitrierung des gesamten Restorezustands, mit deterministisch pausiertem nativen Restore. Auch die spät nach Streamlesen erfasste Recallpriorität ist dabei zu prüfen.

Unveränderte weitere Grenzen: Metadatenzuweisungen nach player_.publish können allokieren; State-/Initialisierungs-/Workerstart-ABI-Grenzen sind nicht systematisch unter OOM ausgeführt. Einzelne atomare Felder beweisen keinen kohärenten Gesamtzustand. Diese Punkte werden als offene Prüfungen geführt, nicht als neue reproduzierte Crashes ausgegeben.

## P023/A03-Fortschritt, Negativkontrollen und DLL-Trennung

**Quellbefund zur Reparatur:** A125_AuditHoldImport(1) setzt Entered und Returned zurück. Der Worker meldet Entered vor der Barriere; nach Freigabe ruft er loadSamples auf und erhöht Returned **erst nach dessen Rückkehr** (`Plugin.cpp:723–728`). Synchronous setState-Loads erhöhen den Counter nicht. Der Test wartet begrenzt auf Returned>0 (`vst3_integration.cpp:960–971`) und prüft danach persistierten Pfad und charakteristische PCM. Das frühere bloße 350-ms-Warten wurde ersetzt. Dies belegt im vorgesehenen Test den Funktionsreturn, nicht notwendigerweise erneutes Dekodieren: ein korrekt als veraltet verworfener Import darf früh zurückkehren.

Der Auditmutant priority-no-replay setzt DropImportReplay. Der Worker bleibt aktiv, lässt den loadSamples-Aufruf und dessen Counter aus; genau die neue Assertion muss nach Timeout scheitern. YAML verlangt nichtnull Exit **und** `A03 worker failed to resume import after release of barrier`. Der ursprüngliche P023-Pending-Mutant bleibt zusätzlich enthalten. **Designprüfung PASS; eigene native Ausführung beider Positiv-/Negativkontrollen NOT RUN.**

CMake definiert A125_ENABLE_AUDIT_HOOKS ausschließlich für AutosamplerAuditVST3; der Produktbuild enthält weder Barrier-/Returned-/Drop-Replay-/Pending-Mutant-Exports noch deren Kontrollvariablen. Der Upload kopiert weiterhin nur AutosamplerVST3 in ship-only. Das ist eine nachvollziehbare **statische** Trennung. Die fertige DLL und ZIP wurden nicht unabhängig inspiziert; kein binärer Export-/Artefakt-PASS.

Der unabhängige .NET-ZIP-Verifier untersucht jetzt ausschließlich den Recorder-Ausgabeordner; das verhindert, dass die absichtlich benachbarte manuelle A03-Bank fälschlich als zweites Recorderarchiv gezählt wird. Die anschließende Prüfung von genau einem Archiv/Manifest/acht WAVs blieb bestehen. Eine solche Fixturepfadkorrektur ist kein Nachweis einer Audio-Produktkorrektur.

## Windows-CI und Zugriff: beobachtet, nicht selbst ausgeführt

- [VST3-Run 43636037233](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43636037233): Websummary **Success**, exakter verlinkter Commit f7d2d4f…; Job 132572936946.
- [Core-Run 43636037377](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43636037377): Websummary **Success**, derselbe Commit; Job 132572937273.
- Die tatsächliche VST3-Jobseite ist erreichbar und zeigt ausdrücklich **„Sign in to view logs“**. Die vorhandene `gh api`-Abfrage liefert **Forbidden**. Es liegt kein nutzbarer API-Detailzugriff vor. Dies ist eine Umgebungs-/Zugriffsgrenze, kein Produktfehler.
- Vollständige Logs, konkrete Negativlauftranskripte, 94/94 Validator und heruntergeladene Produkt-/Audit-Artefaktbytes sind hier **INCONCLUSIVE**. Keine selbst ausgeführten Windows-Tests behauptet.
- Gepinntes Test Lab bleibt hinsichtlich P74 auf eine Audioklasse begrenzt; der P22-Pfad ist Inventar und keine semantische Produktabnahme. Weder ein P74- noch ein vollständiger Test-Lab-PASS wird abgeleitet.

## Abschlussentscheidung

**NO-GO für P025.** Die RR01-Regel gilt jetzt ausdrücklich auch für den Bankkontext; der manuelle/automatische Publishpfad verletzt sie weiterhin. Der zugehörige neue Regressionstest erkennt nicht einmal einen ausgeschalteten RR-Reset. Zusätzlich ist der gesamte konkurrierende Restore-Commit noch nicht belastbar nachgewiesen, und die erforderlichen Windows-Detailbelege konnten nicht geprüft werden.

Die erfolgreich nachgewiesenen Reparaturen werden ausdrücklich anerkannt: R02 ursprünglicher Dateiaustausch, R03 ursprüngliche Normalize-Recallprobe, R04 ursprüngliche und erweiterte Unicodefälle sowie GCC14 `-O3` **PASS**. R01-Overrides und P023/A03-Workerfortschrittsdesign sind statisch verbessert, jedoch nativ **NOT RUN**.

Vor erneuter technischer Freigabe: RR-Kontext an Bankwechsel binden; Odd-Count-Negativkontrolle ergänzen und bestehen; Restore-vs-neuere-Auswahl einschließlich Fehlerpfad nativ erzwingen; die exakten CI-Logs/Artefakte einschließlich beider Auditmutanten und produktiver Exporttrennung unabhängig auswerten. A01–A15-Offenpunkte dürfen nicht pauschal aus dem Core-PASS geschlossen werden.

**Dies fordert keinen vorgezogenen finalen Fender-Studio-Pro-/ASIO-Test:** Dieser bleibt entsprechend dem Auftrag erst nach der fertigen GUI erforderlich. Eine spätere P025-Freigabe wäre weiterhin keine kommerzielle Release-Freigabe.
