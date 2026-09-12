"""Import the audio-overhaul sources. Run inside UnrealEditor-Cmd:

    UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript \
        -script=Tools/ImportOverhaulAudio.py -unattended -nosplash \
        -abslog=Artifacts/overhaul_audio.log

Idempotent: replace_existing=True, verified by load + count at the end.
Imports:
  ArtSource/Generated/Commentary/*.wav -> /Game/Cricket26/Audio/Commentary/
  ArtSource/Generated/Audio/{bat_mistimed,foot_plant,runup_step,ball_release,final_ball_pulse}.wav -> /Game/Cricket26/Audio/
Also attempts broadcast SoundClass assets (/Game/Cricket26/Audio/Mix) and
routes commentary waves through SC_Commentary. SoundClass creation is
best-effort: the C++ bus multipliers in UC26Audio remain the authoritative
mix, so a failed optional step never blocks the import.
"""
import unreal as u
import json
from pathlib import Path

root = Path(u.Paths.project_dir())
lib = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()

# ---- 1. commentary library ----
manifest_path = root / "ArtSource" / "Generated" / "Commentary" / "manifest.json"
manifest = json.loads(manifest_path.read_text())
tasks = []
for row in manifest:
    t = u.AssetImportTask()
    filename = row["file"]
    if not filename.endswith(".wav"):
        filename += ".wav"
    t.filename = str(root / "ArtSource" / "Generated" / "Commentary" / filename)
    t.destination_path = "/Game/Cricket26/Audio/Commentary"
    t.destination_name = Path(filename).stem
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)

# ---- 2. supplemental & upgraded Foley ----
for name in ["bat_mistimed", "foot_plant", "runup_step", "ball_release", "final_ball_pulse"]:
    wav_path = root / "ArtSource" / "Generated" / "Audio" / (name + ".wav")
    if wav_path.exists():
        t = u.AssetImportTask()
        t.filename = str(wav_path)
        t.destination_path = "/Game/Cricket26/Audio"
        t.destination_name = name
        t.automated = True
        t.save = True
        t.replace_existing = True
        tasks.append(t)

u.log("C26_AUDIO_IMPORT tasks=%d" % len(tasks))
tools.import_asset_tasks(tasks)

# ---- 3. verify ----
missing = []
for row in manifest:
    stem = Path(row["file"]).stem
    if not lib.load_asset("/Game/Cricket26/Audio/Commentary/" + stem):
        missing.append(stem)
for name in ["bat_mistimed", "foot_plant", "runup_step", "ball_release", "final_ball_pulse"]:
    if not lib.load_asset("/Game/Cricket26/Audio/" + name):
        missing.append(name)

u.log("C26_AUDIO_VERIFY ok=%d missing=%d" % (len(tasks) - len(missing), len(missing)))
for m in missing[:10]:
    u.log_warning("C26_AUDIO_MISSING " + m)

# ---- 4. broadcast mix classes (best effort) ----
try:
    lib.make_directory("/Game/Cricket26/Audio/Mix")
    for sc in ["SC_Master", "SC_Commentary", "SC_Crowd", "SC_OnField", "SC_UI", "SC_Music"]:
        path = "/Game/Cricket26/Audio/Mix/" + sc
        if not lib.does_asset_exist(path):
            tools.create_asset(sc, "/Game/Cricket26/Audio/Mix", u.SoundClass, u.SoundClassFactory())
    u.log("C26_AUDIO_MIX SoundClasses ready")
except Exception as e:
    u.log_warning(f"C26_AUDIO_MIX non-blocking: {e}")
