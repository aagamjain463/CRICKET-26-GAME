"""Emit the C++ commentary table from the single source of truth.

Usage: python3 Tools/EmitCommentaryData.py
Reads Tools/CommentaryScript.py, writes
Source/CRICKETGAME/SuperOver/C26CommentaryData.inc which C26Audio.cpp
includes. Filenames mirror GenerateDevelopmentCommentary.stable_name.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
from CommentaryScript import LIBRARY  # noqa: E402


def esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


lines = ["// AUTO-GENERATED from Tools/CommentaryScript.py — do not hand-edit.",
         "// %d original development-VO entries." % len(LIBRARY),
         "static const FC26CommentaryRowSrc GVoiceRows[] = {"]
for e in LIBRARY:
    name = e["id"].replace(".", "_")
    lines.append('    { TEXT("%s"), TEXT("%s"), TEXT("%s"), TEXT("%s"), %d, %d, %d, %.2ff, %d, %s },' % (
        esc(e["id"]), esc(e["category"]), esc(name), esc(e["text"]),
        0 if e["voice"] == "A" else 1, e["priority"], e["cooldown"],
        float(e["delay"]), e["weight"], "true" if e.get("follow") else "false"))
lines.append("};")
out = ROOT / "Source" / "CRICKETGAME" / "SuperOver" / "C26CommentaryData.inc"
out.write_text("\n".join(lines) + "\n")
print("C26_COMMENTARY_DATA entries=%d -> %s" % (len(LIBRARY), out))
