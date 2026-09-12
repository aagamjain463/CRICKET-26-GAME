# Read-only probe of the player material chain: which masters exist, what parameters they expose,
# and which textures the per-role instances actually point at.
# Usage: UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/InspectPlayerMats.py
import unreal

MASTERS = [
    "/Game/Cricket26/Materials/M_C26_PlayerSkin",
    "/Game/Cricket26/Materials/M_C26_Cloth",
    "/Game/Cricket26/Materials/M_C26_Gear",
    "/Game/Cricket26/Materials/M_C26_Kit",
    "/Game/Cricket26/Materials/M_C26_Shell",
    "/Game/Cricket26/Materials/M_Athlete_PBR",
    "/Game/Cricket26/Materials/M_Surface",
]

TEXTURES = [
    "/Game/Cricket26/Characters/Remy_Body_Diffuse",
    "/Game/Cricket26/Characters/Remy_Body_Normal",
    "/Game/Cricket26/Characters/Remy_Top_Diffuse",
    "/Game/Cricket26/Characters/Remy_Bottom_Diffuse",
    "/Game/Cricket26/Characters/Remy_Hair_Diffuse",
    "/Game/Cricket26/Characters/Remy_Shoes_Diffuse",
    "/Game/Cricket26/Textures/T_Batter_Hero_D",
]


def scalar_names(material):
    try:
        out = []
        for p in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material):
            out.append(str(p))
        return out
    except Exception as exc:                        # noqa: BLE001
        return ["<err %s>" % exc]


def vector_names(material):
    try:
        return [str(p) for p in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)]
    except Exception as exc:                        # noqa: BLE001
        return ["<err %s>" % exc]


def texture_names(material):
    try:
        return [str(p) for p in unreal.MaterialEditingLibrary.get_texture_parameter_names(material)]
    except Exception as exc:                        # noqa: BLE001
        return ["<err %s>" % exc]


unreal.log("C26_MATS_BEGIN")
for path in MASTERS:
    m = unreal.load_asset(path)
    if m is None:
        unreal.log("C26_MAT MISSING %s" % path)
        continue
    name = path.rsplit("/", 1)[-1]
    unreal.log("C26_MAT %-22s scalars=%s" % (name, ", ".join(scalar_names(m)) or "<none>"))
    unreal.log("C26_MAT %-22s vectors=%s" % ("", ", ".join(vector_names(m)) or "<none>"))
    unreal.log("C26_MAT %-22s textures=%s" % ("", ", ".join(texture_names(m)) or "<none>"))

for path in TEXTURES:
    t = unreal.load_asset(path)
    if t is None:
        unreal.log("C26_TEX MISSING %s" % path)
        continue
    try:
        x, y = t.blueprint_get_size_x(), t.blueprint_get_size_y()
    except Exception:                               # noqa: BLE001
        x = y = -1
    unreal.log("C26_TEX %-26s %dx%d" % (path.rsplit("/", 1)[-1], x, y))

for role in ("Batter", "Bowler", "Keeper", "Umpire", "Fielder_01", "NonStriker"):
    path = "/Game/Cricket26/Materials/Players/MI_Player_%s" % role
    mi = unreal.load_asset(path)
    if mi is None:
        unreal.log("C26_MI MISSING %s" % role)
        continue
    parent = mi.get_editor_property("parent")
    unreal.log("C26_MI MI_Player_%-12s parent=%s" % (role, parent.get_name() if parent else "NONE"))
unreal.log("C26_MATS_END")
