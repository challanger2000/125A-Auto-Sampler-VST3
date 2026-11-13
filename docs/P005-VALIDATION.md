# P005 – belegte Windows-VST3-Abnahme

## Belegter vollständiger Build

[GitHub Actions: P005 Windows VST3 Integration QA, Lauf #43528728186](https://github.com/challanger2000/125A-Auto-Sampler-VST3/actions/runs/43528728186)

Der Workflow verwendet einen gepinnten offiziellen Steinberg-VST3-SDK-Checkout, Windows-2022-x64-Runner und C++23 für Produkt und Test-Harness.

## Positive, unabhängige Prüfschritte

1. **Windows-Build:** MIDI-Generator und Recorder sind zwei ladbare VST3-Klassen aus derselben echten Bundle-DLL.
2. **Steinberg-Validator:** beide Klassen ohne Fehler geprüft.
3. **VSTGUI-Ressourcen:** echte Bundle-Datei `autosampler.uidesc`, zwölf VST3-Parameterkennungen und Start/Stop geprüft.
4. **Echter nativer Win32-Editor:** Die DLL wird geladen, der Editor gibt plausible Abmessungen zurück, HWND wird erfolgreich attached, Status-Timer läuft und das Fenster wird sauber detached.
5. **Echte MIDI-Prozessor-Kette:** acht Note-On- und acht Note-Off-Ereignisse, korrekte Note 60/Velocity 100. Output-Events der geladenen VST3-Klasse werden als MIDI-Stimulus verwendet.
6. **Echter Recorder-Ein-/Ausgang:** Ein synthetisches Oszillator-Signal wird in die tatsächlich geladene Recorder-VST3 geleitet und am Stereo-Output bit-/floatgetreu durchgereicht.
7. **Eigentlicher Datei-Export:** der Recorder schreibt **acht** separate 24-Bit-Stereo-WAVs; WAV-Header, Mindestdauer, nichtstumme Sampledaten und kompletter RUN-REPORT.csv werden unabhängig geprüft. Keine verlorenen Frames, keine verworfenen Takes.
8. **Real getesteter Stop:** der MIDI-Generator wird erneut aktiviert; nach akzeptiertem Note-On wird bei gestoppter Wiedergabe und absichtlich versetzter Songposition exakt der passende Note-Off mit gleicher `noteId` ausgegeben.
9. **Core-Regressionen:** 48 eindeutige simulierte WAVs bei 2 Noten × 3 Velocity × 8 RR, alle 127 Velocity-Werte, Note-/Release-/Gap-Grenzen, Stille-Trim und Fehlerfälle für verlorene Frames, fehlende Take-Enden und Song-Neustarts.

Der native Host ist **nur** unter `tests/vst3_integration.cpp` für die technische QA implementiert. Er wird nicht in das VST3-Bundle gepackt und kann keine Fremd-VSTs für den Endbenutzer hosten. Er erzeugt ein Testsignal, das ein hypothetisches, von der DAW geladenes Instrument repräsentiert.

## Abnahmegrenzen – FAIL-CLOSED

- **EVM Isotope** oder ein anderes fremdes VST wurde in P005 nicht geladen.
- **Fender Studio Pro:** keine reale Projektspur, keine bestätigte Routing-Auswahl und keine Praxisaufnahme; weiter **NOT RUN**.
- **Physisches ASIO-Gerät:** kein Test mit Hardwaretreiber; weiter **NOT RUN**.
- **GUI-Bedienung:** HWND Attach/Detach und Zeitgeber geprüft, aber noch keine vollständige pixelbasierte Darstellung, Klickmatrix und Bedienbarkeit aller Regler; weiter **NOT COVERED**.
- **Mehrere Recorder-Paare, frei wählbarer Exportpfad und dynamische Namenskonvention:** noch nicht implementiert.
- **125A Plugin Test Lab P74:** für dieses Mehrklassen-Bundle nicht automatisch ausführbar, kein P74-PASS behauptet.
- **Release:** ausdrücklich **NOT APPROVED**. Alle restlichen Produktfälle müssen real geprüft werden.

Diese Nachweise belegen wesentlich mehr als einen Kompilierungserfolg, ersetzen aber keine Abnahme in der Ziel-DAW.
