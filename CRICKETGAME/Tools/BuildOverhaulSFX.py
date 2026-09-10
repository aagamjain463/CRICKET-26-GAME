"""Original supplemental cricket Foley for the audio overhaul.

Deterministic synthesis; no external service, samples, or recordings.
Adds the two transients the library was missing:
  bat_mistimed : dull, choked contact (lowpassed burst, weak high snap)
  foot_plant   : front-foot plant thump (soft low body, no explosion)
Writes into ArtSource/Generated/Audio alongside the existing Foley.
Rerunnable: outputs are deterministic given the fixed seed.
"""
from pathlib import Path
import wave
import math
import random
import struct

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "ArtSource" / "Generated" / "Audio"
DEST.mkdir(parents=True, exist_ok=True)
rng = random.Random(261026)
rate = 24000


def write(name, duration, sample):
    data = []
    filtered = 0.0
    for i in range(int(rate * duration)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        filtered = filtered * 0.90 + noise * 0.10  # darker than existing Foley
        data.append(sample(t, noise, filtered))
    peak = max(abs(x) for x in data) or 1
    with wave.open(str(DEST / (name + ".wav")), "wb") as w:
        w.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        w.writeframes(b"".join(struct.pack("<h", round(x / peak * 20000)) for x in data))


def mistimed(t, n, f):
    # Choked bat: low knock with a stunted crack that dies fast.
    body = 0.85 * math.sin(2 * math.pi * (170 * t - 260 * t * t))
    snap = 0.30 * n * math.exp(-t * 160)
    return (body * math.exp(-t * 42) + snap + 0.25 * f * math.exp(-t * 60)) * min(1, t / 0.002)


def plant(t, n, f):
    # Plant: weight through the forefoot on turf. Felt, not heard, beyond metres.
    thud = 0.9 * math.sin(2 * math.pi * (88 * t - 60 * t * t))
    scuff = 0.22 * f
    return (thud * math.exp(-t * 30) + scuff * math.exp(-t * 55)) * min(1, t / 0.003)


write("bat_mistimed", 0.16, mistimed)
write("foot_plant", 0.16, plant)
print("Generated 2 original overhaul Foley sources:", DEST)
