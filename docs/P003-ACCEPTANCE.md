# P003 – konkrete Nachweise und Grenzen

## Bestehende native Tests
- `JobSchedule.h`: deterministische Note/Velocity/RR-Dekodierung in O(1), MIDI-Note-On/Note-Off/CaptureEnd auf Sampleframes.
- Regression `testJobSchedule`: 3 Noten × 3 Velocity × 8 RR = 72 korrekte Metadatenzuordnungen einschließlich Start/Release/Gap.
- Regression `testAutomaticBatch48Wavs`: 2 Noten × 3 Velocity × 8 RR = 48 WAV-Dateien und 24-Bit-PCM-Header/Längenvergleich, keine Kollision.
- Windows-VST3-Build und offizieller Steinberg-Validator prüfen die beiden Plugin-Klassen.

## Nicht aus diesen Tests ableitbar
- Studio Pro routet MIDI aus Generator und Audio ins Recorder-Insert tatsächlich zur richtigen Zeit.
- Echtzeitcallback-Sicherheit mit unterlaufenden Queues bei großen Sessions.
- Beliebige VST-Software erzeugt pro RR-Take wirklich neue Variationen.
- Eigene GUI, vollständige DAW-Transportsteuerung, AU/macOS/Linux.
- ASIO-Hardware, vollständige Hersteller-Host-Konformität oder Release-Freigabe.

## Nächster Funktionstest
In Fender Studio Pro einen eigenen Test-Song mit EVM Isotope:
- MIDI-Note 60, 100 Velocity, 8 RR; NoteOn/Off-Zähler und 8 exportierte WAVs.
- Jeder Take inklusive Attack/Note-Off/Release und eindeutiger Dateiname.
- Nachprüfen 44,1 und 48 kHz, Puffergrößen 64/256/1024, Stop/Start, Song-Loop und Project-Reload.
- Anschließend mindestens 2 Noten × 3 Velocity × 8 RR mit korrekten Level-Unterschieden.
- Auch die 125A Plugin Test Lab Multi-Class-Einschränkung ausdrücklich dokumentieren.

Kein Abnahme-Label ohne diese Nachweise.
