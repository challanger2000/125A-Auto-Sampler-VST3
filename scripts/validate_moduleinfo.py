#!/usr/bin/env python3
"""Validate SDK-generated VST3 metadata and normalize trailing commas.

Some moduleinfotool builds emit optional trailing commas. The official
moduleinfo.json must be strict JSON for hosts which parse it strictly.
The normalization only removes commas outside quoted strings that directly
precede ] or }. It never changes MIDI/audio DSP or any binary.
"""
import json
import pathlib
import sys


def normalize(text: str) -> str:
    result = []
    quoted = False
    escape = False
    for index, char in enumerate(text):
        if quoted:
            result.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
            result.append(char)
        elif char == ",":
            next_char = next((c for c in text[index + 1:] if not c.isspace()), "")
            if next_char not in "]}":
                result.append(char)
        else:
            result.append(char)
    return "".join(result)


def main() -> None:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build")
    files = list(root.rglob("moduleinfo.json"))
    if not files:
        raise SystemExit("FAIL: no VST3 moduleinfo.json was produced")
    valid = False
    for path in files:
        source = path.read_text(encoding="utf-8-sig")
        doc = json.loads(normalize(source))
        if doc.get("Name") != "A125DigitalBass":
            continue
        classes = doc.get("Classes", [])
        if not any("Instrument" in c.get("Sub Categories", []) for c in classes):
            raise SystemExit("FAIL: missing VST3 Instrument category")
        path.write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8")
        json.loads(path.read_text(encoding="utf-8"))
        print("PASS: strict JSON / Instrument category:", path)
        valid = True
    if not valid:
        raise SystemExit("FAIL: 125A module metadata not found")


if __name__ == "__main__":
    main()
