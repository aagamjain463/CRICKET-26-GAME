# Read-only: the exact material SLOT NAMES on the mesh the live match spawns, and on the hero
# bodies. C26Athlete::Configure assigns materials by slot name, so these names are the contract.
# Usage: UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/InspectSlots.py
import unreal

MESHES = [
    "/Game/Cricket26/Characters/SK_Cricketer_Match",
    "/Game/Cricket26/Characters/SK_Cricketer",
    "/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBatter",
]

for path in MESHES:
    mesh = unreal.load_asset(path)
    name = path.rsplit("/", 1)[-1]
    if mesh is None:
        unreal.log("C26_SLOT MISSING %s" % name)
        continue
    mats = mesh.get_editor_property("materials")
    unreal.log("C26_SLOT %s slots=%d" % (name, len(mats)))
    for i, slot in enumerate(mats):
        iface = slot.material_interface
        unreal.log("C26_SLOT    [%d] name='%s' material=%s"
                   % (i, slot.material_slot_name, iface.get_name() if iface else "None"))
    try:
        skel = mesh.get_editor_property("skeleton")
        unreal.log("C26_SLOT    skeleton=%s" % (skel.get_name() if skel else "NONE"))
    except Exception as exc:                        # noqa: BLE001
        unreal.log("C26_SLOT    skeleton=<err %s>" % exc)
unreal.log("C26_SLOT_END")
