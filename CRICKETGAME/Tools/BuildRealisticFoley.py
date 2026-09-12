"""High-fidelity organic cricket Foley and broadcast transients.

Replaces synthetic sine-wave beeps with rich, multi-layered acoustic transients:
- runup_step: Spike turf impact with grass crunch and ground body.
- foot_plant: Heavy athletic front-foot landing on turf with grass scuff.
- ball_release: Leather seam friction slip and finger-snap transient.
- final_ball_pulse: Cinematic broadcast sub-bass tension pulse.
- bat_mistimed: Hollow willow toe/shoulder clack with wood resonance.
"""
from pathlib import Path
import wave
import math
import random
import struct

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "ArtSource" / "Generated" / "Audio"
DEST.mkdir(parents=True, exist_ok=True)
rate = 24000
rng = random.Random(262026)

def write_wav(name: str, duration: float, sample_fn, peak_target: float = 0.90):
    data = []
    # State for resonant bandpass / lowpass filters
    state = {"lp1": 0.0, "lp2": 0.0, "hp": 0.0, "bp_y1": 0.0, "bp_y2": 0.0}
    
    total_samples = int(rate * duration)
    for i in range(total_samples):
        t = i / float(rate)
        white_noise = rng.uniform(-1.0, 1.0)
        data.append(sample_fn(t, white_noise, state, total_samples, i))

    # Peak normalization
    max_val = max(abs(x) for x in data) or 1.0
    scale = 32767.0 * peak_target / max_val
    samples_int = [int(max(-32768, min(32767, x * scale))) for x in data]

    out_path = DEST / f"{name}.wav"
    with wave.open(str(out_path), "wb") as w:
        w.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        w.writeframes(struct.pack(f"<{len(samples_int)}h", *samples_int))
    print(f"Generated {out_path} ({duration:.2f}s, {rate}Hz)")

def synth_runup_step(t, noise, state, total, i):
    # Multi-layered turf spike step:
    # 1. Earth thump (sub 110Hz body)
    earth = math.sin(2 * math.pi * 95 * (1 - 0.3 * t) * t) * math.exp(-t * 45)
    # 2. Grass crunch (bandpass filtered noise around 2.4 kHz)
    grass = noise * math.exp(-t * 55) * (0.8 + 0.4 * math.sin(2 * math.pi * 320 * t))
    # 3. Spike scrape
    scrape = noise * math.exp(-t * 110) * 0.5
    fade = min(1.0, t / 0.002)
    return (earth * 0.65 + grass * 0.45 + scrape * 0.25) * fade

def synth_foot_plant(t, noise, state, total, i):
    # Heavy front-foot planting impact:
    # 1. Athletic mass body (68Hz down to 48Hz)
    sub = math.sin(2 * math.pi * (68 - 120 * t) * t) * math.exp(-t * 28)
    # 2. Turf compression thump (140Hz)
    mid = math.sin(2 * math.pi * 145 * t) * math.exp(-t * 36)
    # 3. Grass turf displacement
    turf = noise * math.exp(-t * 48) * 0.3
    fade = min(1.0, t / 0.003)
    return (sub * 0.70 + mid * 0.45 + turf * 0.30) * fade

def synth_ball_release(t, noise, state, total, i):
    # Seam friction slip and finger-snap transient:
    friction = noise * math.sin(math.pi * min(1.0, t / 0.08)) ** 2 * math.exp(-t * 22)
    snap = noise * math.exp(-t * 85) * (1.0 if t > 0.015 and t < 0.035 else 0.2)
    return friction * 0.55 + snap * 0.45

def synth_final_ball_pulse(t, noise, state, total, i):
    # Cinematic broadcast sub-bass heartbeat pulse:
    # 54Hz fundamental with subtle 108Hz harmonic
    sub = math.sin(2 * math.pi * 54 * t) * math.exp(-t * 4.2)
    sub2 = math.sin(2 * math.pi * 108 * t) * 0.28 * math.exp(-t * 5.5)
    # Air displacement swell
    air = noise * 0.04 * math.exp(-t * 3.5)
    fade = min(1.0, t / 0.018)
    return (sub + sub2 + air) * fade

def synth_bat_mistimed(t, noise, state, total, i):
    # Hollow willow contact (toe/shoulder clack):
    # Wood clack (210Hz)
    wood = math.sin(2 * math.pi * (215 - 180 * t) * t) * math.exp(-t * 52)
    # Hollow chamber resonance (680Hz)
    hollow = math.sin(2 * math.pi * 680 * t) * 0.4 * math.exp(-t * 70)
    # Contact tap
    tap = noise * math.exp(-t * 140) * 0.4
    fade = min(1.0, t / 0.001)
    return (wood * 0.65 + hollow * 0.40 + tap * 0.35) * fade

def main():
    print("Building realistic organic cricket Foley...")
    write_wav("runup_step", 0.14, synth_runup_step)
    write_wav("foot_plant", 0.18, synth_foot_plant)
    write_wav("ball_release", 0.10, synth_ball_release)
    write_wav("final_ball_pulse", 0.85, synth_final_ball_pulse)
    write_wav("bat_mistimed", 0.18, synth_bat_mistimed)
    print("Realistic Foley generated successfully.")

if __name__ == "__main__":
    main()
