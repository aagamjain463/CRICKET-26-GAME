"""
MetaHumanForceNoBake.py

Purpose
-------
Turn OFF `bBakeMaterials` on the MetaHuman editor pipeline that the character build
uses, so that `UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials()` is never
reached. That call is the exact crash site on this machine:

    UTG_AsyncExportTask::TG_AsyncExportTask(...)
    <- UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials(...)
    <- UMetaHumanDefaultEditorPipelineBase::ProcessBakedMaterials(...)
    <- UMetaHumanDefaultEditorPipelineBase::BuildCollection(...)
    <- UMetaHumanCollection::Build(...)
    <- FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter(...)

Reachability chain (all Python-exposed UPROPERTYs):
    UMetaHumanDefaultPipeline::EditorPipeline        UPROPERTY(EditAnywhere, NoClear, Instanced)
    UMetaHumanDefaultEditorPipelineBase::bBakeMaterials  UPROPERTY(EditAnywhere, Category="Materials")

`GetMutableEditorPipeline()` / `GetSelectedPipeline()` are plain C++ virtuals (not
UFUNCTION), so they are NOT callable from Python -- but the `EditorPipeline` member
they return IS a readable UPROPERTY, which is what this script uses instead.

Run:
  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=<this file>
"""

import unreal

LOG = "C26_NOBAKE"


def log(msg):
    unreal.log("%s %s" % (LOG, msg))


# ---------------------------------------------------------------------------
# Name-variant resolution: UE Python sometimes exposes `bFoo` as `bFoo`, `foo`,
# or `b_foo`. Try each and report which one the binding actually accepts.
# ---------------------------------------------------------------------------
def try_get(obj, name_variants):
    for n in name_variants:
        try:
            v = obj.get_editor_property(n)
            return n, v, None
        except Exception as e:
            last = e
    return None, None, last


def try_set(obj, name_variants, value):
    for n in name_variants:
        try:
            obj.set_editor_property(n, value)
            return n, None
        except Exception as e:
            last = e
    return None, last


BAKE_NAMES = ["bBakeMaterials", "b_bake_materials", "BakeMaterials", "bake_materials"]
EP_NAMES = ["EditorPipeline", "editor_pipeline"]


# ---------------------------------------------------------------------------
# Candidate pipeline classes: the CDO of each carries the Instanced EditorPipeline
# subobject. Class paths come from DefaultMetaHumanCharacter.ini plus the C++
# SetDefaultEditorPipeline() implementations.
# ---------------------------------------------------------------------------
CLASS_PATHS = [
    # Project-settings default collection pipelines (BP assets -> generated classes)
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultPipeline.BP_DefaultPipeline_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline.BP_DefaultLegacyPipeline_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_High.BP_DefaultLegacyPipeline_High_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Medium.BP_DefaultLegacyPipeline_Medium_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Low.BP_DefaultLegacyPipeline_Low_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_High.BP_DefaultUEFNPipeline_High_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Medium.BP_DefaultUEFNPipeline_Medium_C",
    "/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Low.BP_DefaultUEFNPipeline_Low_C",
]

# Native pipeline classes (their CDOs are always live)
NATIVE_CLASSES = [
    "MetaHumanDefaultPipeline",
    "MetaHumanDefaultPipelineBase",
    "MetaHumanDefaultPipelineLegacy",
    "MetaHumanDefaultPipelineUEFN",
]

# Native editor-pipeline classes: setting these CDOs covers instances created later
# via NewObject<>() because NewObject copies property values from the CDO.
EDITOR_PIPELINE_CLASSES = [
    "MetaHumanDefaultEditorPipeline",
    "MetaHumanDefaultEditorPipelineLegacy",
    "MetaHumanDefaultEditorPipelineUEFN",
    "MetaHumanCollectionEditorPipeline",
]


def resolve_class(path):
    """Load a generated UClass from an object path."""
    obj = unreal.load_object(None, path)
    if obj is None:
        return None
    # load_object may hand back the CDO itself for a _C path; normalise to a class.
    if isinstance(obj, unreal.Class):
        return obj
    try:
        return obj.get_class()
    except Exception:
        return None


def cdo_of(cls):
    try:
        return unreal.get_default_object(cls)
    except Exception:
        return None


def force_no_bake_on_cdo(cdo, label):
    """Find the Instanced EditorPipeline on `cdo` and set bBakeMaterials=False."""
    name, ep, err = try_get(cdo, EP_NAMES)
    if ep is None:
        log("  %-46s no EditorPipeline (%s)" % (label, err))
        return False

    bname, before, berr = try_get(ep, BAKE_NAMES)
    if bname is None:
        log("  %-46s EditorPipeline=%s but no bBakeMaterials (%s)"
            % (label, ep.get_name(), berr))
        return False

    sname, serr = try_set(ep, BAKE_NAMES, False)
    if sname is None:
        log("  %-46s FOUND bBakeMaterials(%s)=%s but SET FAILED (%s)"
            % (label, bname, before, serr))
        return False

    # Read back -- this is the only proof that matters.
    _, after, _ = try_get(ep, BAKE_NAMES)
    ok = (after is False or after == 0)
    log("  %-46s prop=%-16s ep=%-34s %s -> %s  %s"
        % (label, bname, ep.get_name(), before, after, "OK" if ok else "NOT APPLIED"))
    return ok


def main():
    log("=" * 78)
    log("Forcing bBakeMaterials=False on every reachable MetaHuman editor pipeline")
    log("=" * 78)

    applied = []

    log("-- native editor-pipeline class CDOs --")
    for cn in EDITOR_PIPELINE_CLASSES:
        cls = getattr(unreal, cn, None)
        if cls is None:
            log("  %-46s class not exposed to Python" % cn)
            continue
        cdo = cdo_of(cls)
        if cdo is None:
            log("  %-46s no CDO" % cn)
            continue
        # On the editor-pipeline CDO the flag lives directly on the object.
        bname, before, berr = try_get(cdo, BAKE_NAMES)
        if bname is None:
            log("  %-46s no bBakeMaterials on CDO (%s)" % (cn, berr))
            continue
        sname, serr = try_set(cdo, BAKE_NAMES, False)
        if sname is None:
            log("  %-46s SET FAILED (%s)" % (cn, serr))
            continue
        _, after, _ = try_get(cdo, BAKE_NAMES)
        log("  %-46s prop=%-16s %s -> %s" % (cn, bname, before, after))
        if after is False or after == 0:
            applied.append(cn)

    log("-- native collection-pipeline CDOs (EditorPipeline member) --")
    for cn in NATIVE_CLASSES:
        cls = getattr(unreal, cn, None)
        if cls is None:
            log("  %-46s class not exposed to Python" % cn)
            continue
        cdo = cdo_of(cls)
        if cdo is None:
            log("  %-46s no CDO" % cn)
            continue
        if force_no_bake_on_cdo(cdo, cn):
            applied.append(cn)

    log("-- project-settings BP pipeline classes --")
    for path in CLASS_PATHS:
        cls = resolve_class(path)
        if cls is None:
            log("  %-46s could not load" % path.rsplit("/", 1)[-1])
            continue
        cdo = cdo_of(cls)
        if cdo is None:
            log("  %-46s no CDO" % path.rsplit("/", 1)[-1])
            continue
        if force_no_bake_on_cdo(cdo, path.rsplit("/", 1)[-1]):
            applied.append(path)

    log("-" * 78)
    log("CDOs now carrying bBakeMaterials=False: %d" % len(applied))
    for a in applied:
        log("   * %s" % a)
    log("C26_NOBAKE_DONE")
    return 0


main()
