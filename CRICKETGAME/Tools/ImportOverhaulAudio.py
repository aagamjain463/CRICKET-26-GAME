"""Import the audio-overhaul sources. Run inside UnrealEditor-Cmd:

    UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript \
        -script=Tools/ImportOverhaulAudio.py -unattended -nosplash \
        -abslog=Artifacts/overhaul_audio.log

Idempotent: replace_existing=True, verified by load + count at the end.
Imports:
  ArtSource/Generated/Commentary/*.wav -> /Game/Cricket26/Audio/Commentary/
  ArtSource/Generated/Audio/{bat_mistimed,foot_plant}.wav -> /Game/Cricket26/Audio/
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
    t.filename = str(root / "ArtSource" / "Generated" / "Commentary" / row["file"])
    t.destination_path = "/Game/Cricket26/Audio/Commentary"
    t.destination_name = Path(row["file"]).stem
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
# ---- 2. supplemental Foley ----
for name in ["bat_mistimed", "foot_plant"]:
    t = u.AssetImportTask()
    t.filename = str(root / "ArtSource" / "Generated" / "Audio" / (name + ".wav"))
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
for name in ["bat_mistimed", "foot_plant"]:
    if not lib.load_asset("/Game/Cricket26/Audio/" + name):
        missing.append(name)
u.log("C26_AUDIO_VERIFY ok=%d missing=%d" % (len(tasks) - len(missing), len(missing)))
for m in missing[:10]:
    u.log_warning("C26_AUDIO_MISSING " + m)

# ---- 4. broadcast mix classes (best effort) ----
try:
    lib.make_directory("/Game/Cricket26/Audio/Mix")
    classes = {}
    for sc in ["SC_Master", "SC_Commentary", "SC_Crowd", "SC_OnField", "SC_UI", "SC_Music"]:
        path = "/Game/Cricket26/Audio/Mix/" + sc
        asset = lib.load_asset(path)
        if not asset:
            asset = tools.create_asset(sc, "/Game/Cricket26/Audio/Mix", u.SoundClass, None)
        classes[sc] = asset
    routed = 0
    for row in manifest:
        stem = Path(row["file"]).stem
        wave = lib.load_asset("/Game/Cricket26/Audio/Commentary/" + stem)
        if wave and classes.get("SC_Commentary"):
            try:
                wave.set_editor_property("sound_class_object", classes["SC_Commentary"])
                lib.save_loaded_asset(wave)
                routed += 1
            except Exception as exc:
                u.log_warning("C26_MIX_ROUTE %s %s" % (stem, exc))
                break
    u.log("C26_MIX_CLASSES routed=%d" % routed)
except Exception as exc:
    u.log_warning("C26_MIX_CLASSES unavailable: %s" % exc)

u.log("C26_AUDIO_IMPORT_COMPLETE")
