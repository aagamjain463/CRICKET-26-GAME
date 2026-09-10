"""Generate ORIGINAL development commentary VO locally with macOS `say`.

Usage:
    python3 Tools/GenerateDevelopmentCommentary.py [--force] [--limit N]

Input : Tools/CommentaryScript.py (LIBRARY list, 118 original lines)
Output: ArtSource/Generated/Commentary/<Stable_Name>.wav (24 kHz 16-bit mono)
        ArtSource/Generated/Commentary/manifest.json

Voices (enumerated via `say -v '?'`, both present on this Mac):
    A (play-by-play) : Daniel (en_GB) @ 178 wpm
    B (analyst)       : Samantha (en_US) @ 172 wpm

Post: peak-normalize to 0.89, 4 ms edge fades, DC removal. No external
service, no network, no copyrighted material. Rerunnable and deterministic:
existing files are skipped unless --force is given.

The game itself NEVER calls `say`; these WAVs are imported as SoundWaves.
"""
import subprocess
import sys
import wave
import struct
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
from CommentaryScript import LIBRARY

DEST = ROOT / "ArtSource" / "Generated" / "Commentary"
DEST.mkdir(parents=True, exist_ok=True)

VOICES = {"A": ("Daniel", "178"), "B": ("Samantha", "172")}


def stable_name(entry_id: str) -> str:
    return entry_id.replace(".", "_")


def synthesize(text: str, voice: str, rate: str, out: Path) -> None:
    cmd = ["say", "-v", voice, "-r", rate,
           "--file-format=WAVE", "--data-format=LEI16@24000",
           "-o", str(out), text]
    subprocess.run(cmd, check=True)


def normalize(path: Path, peak_target: float = 0.89) -> None:
    with wave.open(str(path), "rb") as w:
        params = w.getparams()
        frames = w.readframes(w.getnframes())
    assert params.nchannels == 1 and params.sampwidth == 2, params
    n = len(frames) // 2
    samples = list(struct.unpack("<%dh" % n, frames))
    # DC removal (single-pole high-pass approx) kills rumble cheaply.
    mean = sum(samples) / max(1, len(samples))
    samples = [s - mean for s in samples]
    peak = max(abs(s) for s in samples) or 1.0
    gain = (peak_target * 32767.0) / peak
    # Cap gain so a whisper-quiet render cannot be blasted into noise.
    gain = min(gain, 6.0)
    samples = [max(-32767, min(32767, round(s * gain))) for s in samples]
    # 4 ms raised-cosine edge fades against clicks.
    fade = int(params.framerate * 0.004)
    for i in range(min(fade, len(samples))):
        k = 0.5 - 0.5 * __import__("math").cos(3.14159265 * i / max(1, fade))
        samples[i] = round(samples[i] * k)
        samples[-1 - i] = round(samples[-1 - i] * k)
    with wave.open(str(path), "wb") as w:
        w.setparams(params)
        w.writeframes(struct.pack("<%dh" % n, *samples))


def main() -> None:
    force = "--force" in sys.argv
    limit = None
    for a in sys.argv[1:]:
        if a.startswith("--limit"):
            limit = int(a.split("=")[1] if "=" in a else sys.argv[sys.argv.index(a) + 1])
    entries = LIBRARY[:limit] if limit else LIBRARY
    manifest = []
    done, skipped = 0, 0
    for e in entries:
        name = stable_name(e["id"])
        out = DEST / (name + ".wav")
        voice, rate = VOICES[e["voice"]]
        if out.exists() and not force:
            skipped += 1
        else:
            synthesize(e["text"], voice, rate, out)
            normalize(out)
            done += 1
        with wave.open(str(out), "rb") as w:
            dur = w.getnframes() / float(w.getframerate())
        manifest.append({"id": e["id"], "file": out.name, "voice": e["voice"],
                         "category": e["category"], "priority": e["priority"],
                         "cooldown": e["cooldown"], "delay": e["delay"],
                         "weight": e["weight"], "follow": e.get("follow", False),
                         "text": e["text"], "duration": round(dur, 2)})
    with open(DEST / "manifest.json", "w") as f:
        json.dump(manifest, f, indent=1)
    print(f"C26_COMMENTARY generated={done} reused={skipped} total={len(manifest)} -> {DEST}")


if __name__ == "__main__":
    main()
