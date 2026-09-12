# Read-only probe of every player skeletal mesh in the project.
# Reports triangle count, bone count, material slots and bounds so the hero bodies can be compared
# against the Mixamo-derived SK_Cricketer_Match that the live match currently spawns.
# Usage: UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/InspectPlayers.py
import unreal

PATHS = [
    "/Game/Cricket26/Characters/SK_Cricketer",
    "/Game/Cricket26/Characters/SK_Cricketer_Match",
    "/Game/Cricket26/Characters/SK_Cricketer_KitBase",
    "/Game/Cricket26/Characters/SK_Cricketer_KitCricket",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBatter",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBowler",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroKeeper",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroUmpire",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder01",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder02",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder03",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder04",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder05",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder06",
]

unreal.log("C26_PLAYERS_BEGIN")
for path in PATHS:
    mesh = unreal.load_asset(path)
    if mesh is None:
        unreal.log("C26_PLAYER MISSING %s" % path)
        continue
    name = path.rsplit("/", 1)[-1]
    try:
        tris = mesh.get_num_triangles(0)
    except Exception as exc:                       # noqa: BLE001 - probe, report and continue
        tris = -1
    try:
        verts = mesh.get_num_vertices(0)
    except Exception as exc:                       # noqa: BLE001
        verts = -1
    skel = mesh.get_editor_property("skeleton")
    skel_name = skel.get_name() if skel else "NONE"
    try:
        bones = mesh.get_ref_skeleton().get_num()
    except Exception:                              # noqa: BLE001
        bones = -1
    try:
        lod_count = mesh.get_lod_num()
    except Exception:                              # noqa: BLE001
        lod_count = -1
    mats = []
    try:
        for m in mesh.get_editor_property("materials"):
            mats.append(m.material_interface.get_name() if m.material_interface else "None")
    except Exception:                              # noqa: BLE001
        mats = ["<unreadable>"]
    try:
        box = mesh.get_bounds()
        ext = box.box_extent
        bounds = "%.1f x %.1f x %.1f" % (ext.x, ext.y, ext.z)
        height = ext.z * 2.0
    except Exception:                              # noqa: BLE001
        bounds, height = "<unreadable>", -1.0
    unreal.log("C26_PLAYER %-34s tris=%-7d verts=%-7d bones=%-4d lods=%-2d height=%.1fcm bounds=%s"
               % (name, tris, verts, bones, lod_count, height, bounds))
    unreal.log("C26_PLAYER     skeleton=%s materials=%s" % (skel_name, ", ".join(mats)))
unreal.log("C26_PLAYERS_END")
