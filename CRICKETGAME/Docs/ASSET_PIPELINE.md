# Asset pipeline

Reuse `/Game/Cricket26` conventions; preserve original source FBX and saved scenes.

## Hero equipment (Milestone 2)

Authored through Blender MCP, source under `ArtSource/Blender/`:

| Script | Produces |
|---|---|
| `c26_build.py` | Shared helpers: `Builder` (loft/quad/UV accumulation), `tube`, `material`, `studio` inspection renders, `export_fbx` |
| `Equipment/build_bat.py` | `SM_C26_Bat_Hero` |
| `Equipment/build_headwear.py` | `SM_C26_Helmet_Hero`, `SM_C26_HelmetGrille_Hero`, `SM_C26_Cap_Hero` |
| `Equipment/build_guards.py` | `SM_C26_Pad_L/R`, `SM_C26_Glove_L/R` |
| `Equipment/build_shoe.py` | `SM_C26_Shoe_L/R` |
| `Equipment/build_all.py` | Builds all ten, reports triangle counts, exports FBX, saves `C26_Equipment_v001.blend` |

Run inside the connected Blender:

    import build_all; build_all.run(export=True, save=True)

Then import and verify:

    UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ImportEquipment.py

### Units — read this before authoring anything new

Author in centimetres and let `c26_build.CM = 0.01` store metres. Measured
against a real import, Unreal brings these FBX in at **x100** regardless of the
`UnitScaleFactor` the exporter writes, which is why the existing stadium assets
are authored in metres too. Authoring in raw centimetres landed every asset a
hundred times too large, and `Tools/ImportEquipment.py` caught it: that script
asserts the imported centimetre size of every mesh against a table and fails the
import rather than letting a wrongly scaled bat reach the pitch. Keep that
assertion. The character rig is the one asset that legitimately sits at ~378
units, because it was authored at 100x.

### Local frames

Each mesh is modelled in the frame `AC26Athlete::PlaceKit` already poses it in,
so imported geometry drops onto the existing solve with no new transforms:

- bat — origin at the top of the handle, blade down -Z, hitting face +X
- helmet / grille / cap — origin at the skull pivot, +X out of the face, +Z up
- pad — origin mid-shin, +Z up the shin, +X out of the front of the leg
- glove — origin at the palm, +Z wrist to fingertip, +X back of the hand
- shoe — origin at ground level under the ankle, toes +X, +Z up

Material slots are bound **by name**, never by index (`AC26Athlete::Dress`):
Unreal drops slots that no triangle references, so Blender's slot order and
Unreal's do not match.

## Character

Source `ArtSource/Blender/Characters/C26_KitBase_v001.blend` (never overwrite).
Working copy `C26_KitBase_v002.blend`. `build_kit.py` and `Tools/ImportKit.py`
exist but the derive-garments-from-the-body approach does not work on this base:
`Body` carries no torso or thigh geometry, and `Bottoms` stops at the knee.
Authoring new skinned garments with transferred weights is the open task.

Two traps that cost real time in this file, both guarded in `build_kit.py`:
`matrix_world` is stale immediately after `bpy.ops.wm.open_mainfile` and reports
the wrong space, and the file carries three scenes with the rig in only one, so
`bpy.context.scene.objects` silently omits it.

## Blocked

Premium authored or captured cricket motion: right-handed front-foot straight
drive and a fast bowling run-up to release. Acquire licensed motion or record
with consent; clean in Cascadeur; retarget; integrate deterministic
release/contact markers. Do not relabel procedural motion as authored.
No Meshy usage. No paid generation.
