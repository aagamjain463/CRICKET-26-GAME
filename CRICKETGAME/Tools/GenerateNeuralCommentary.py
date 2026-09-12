"""Generate high-quality broadcast commentary VO locally with neural Piper TTS.

Usage:
    python3 Tools/GenerateNeuralCommentary.py [--force]

Voices:
    A (play-by-play) : Alan (en_GB) neural medium model - punchy, athletic British cadence
    B (analyst)       : Bryce (en_US) neural medium model - calm, measured, tactical cadence

Post: CoreAudio mastering, peak normalization to -1.0 dBFS (0.89), 15ms lead / 40ms tail fades.
"""
import sys
import os
import wave
import struct
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
from CommentaryScript import LIBRARY

DEST = ROOT / "ArtSource" / "Generated" / "Commentary"
DEST.mkdir(parents=True, exist_ok=True)
TEMP_DIR = ROOT / "Saved" / "TempAudio"
TEMP_DIR.mkdir(parents=True, exist_ok=True)

MODEL_DIR = ROOT / "Saved" / "VoiceModels"
MODEL_A = MODEL_DIR / "en_GB-alan-medium.onnx"
MODEL_B = MODEL_DIR / "en_US-bryce-medium.onnx"

import piper

def stable_name(entry_id: str) -> str:
    return entry_id.replace(".", "_")

def post_process_and_convert(raw_wav: Path, final_wav: Path, peak_target: float = 0.89) -> None:
    # Read raw 22050Hz audio
    with wave.open(str(raw_wav), "rb") as w:
        nchannels = w.getnchannels()
        sampwidth = w.getsampwidth()
        framerate = w.getframerate()
        nframes = w.getnframes()
        data = w.readframes(nframes)

    samples = list(struct.unpack(f"<{nframes}h", data))
    if not samples:
        return

    # 1. DC offset removal
    mean = sum(samples) / len(samples)
    samples = [s - mean for s in samples]

    # 2. Peak normalize
    max_amp = max(abs(s) for s in samples) or 1
    gain = (32767.0 * peak_target) / max_amp
    samples = [int(max(-32768, min(32767, s * gain))) for s in samples]

    # 3. Soft edge fades (15ms head, 40ms tail)
    fade_in = int(framerate * 0.015)
    fade_out = int(framerate * 0.040)
    for i in range(min(fade_in, len(samples))):
        factor = i / float(fade_in)
        samples[i] = int(samples[i] * factor)
    for i in range(min(fade_out, len(samples))):
        idx = len(samples) - 1 - i
        factor = i / float(fade_out)
        samples[idx] = int(samples[idx] * factor)

    # Write processed temp wav
    proc_wav = raw_wav.with_suffix(".proc.wav")
    with wave.open(str(proc_wav), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(framerate)
        w.writeframes(struct.pack(f"<{len(samples)}h", *samples))

    # Convert to 24000Hz 16-bit Mono broadcast standard using afconvert
    cmd = ["afconvert", "-f", "WAVE", "-c", "1", "-d", "LEI16@24000", str(proc_wav), str(final_wav)]
    subprocess.run(cmd, check=True)

    if proc_wav.exists():
        proc_wav.unlink()
    if raw_wav.exists():
        raw_wav.unlink()

def main():
    force = "--force" in sys.argv
    print(f"Loading Piper neural voice models...")
    voice_a = piper.PiperVoice.load(str(MODEL_A))
    voice_b = piper.PiperVoice.load(str(MODEL_B))
    print("Models loaded successfully.")

    generated_count = 0
    manifest = []

    for i, entry in enumerate(LIBRARY):
        name = stable_name(entry["id"])
        final_wav = DEST / f"{name}.wav"
        temp_wav = TEMP_DIR / f"{name}_raw.wav"

        voice_choice = entry.get("voice", "A")
        voice = voice_a if voice_choice == "A" else voice_b
        voice_label = "Alan (UK Lead)" if voice_choice == "A" else "Bryce (US Analyst)"

        if final_wav.exists() and not force:
            print(f"[{i+1}/{len(LIBRARY)}] Exists: {name}.wav")
        else:
            print(f"[{i+1}/{len(LIBRARY)}] Synthesizing {name} with {voice_label}...")
            sr = voice.config.sample_rate
            with wave.open(str(temp_wav), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(sr)
                for chunk in voice.synthesize(entry["text"]):
                    w.writeframes(chunk.audio_int16_bytes)

            post_process_and_convert(temp_wav, final_wav)
            generated_count += 1

        # Check duration
        duration = 0.0
        with wave.open(str(final_wav), "rb") as w:
            duration = w.getnframes() / float(w.getframerate())

        manifest.append({
            "id": entry["id"],
            "file": name,
            "category": entry["category"],
            "voice": voice_choice,
            "duration": round(duration, 3),
            "text": entry["text"],
            "priority": entry["priority"]
        })

    with open(DEST / "manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"\nCompleted! Generated/verified {len(LIBRARY)} clips ({generated_count} newly synthesized).")
    print(f"Manifest saved to {DEST / 'manifest.json'}")

if __name__ == "__main__":
    main()
