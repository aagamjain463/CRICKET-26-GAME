"""Create MH_C26_Player_001: an original MetaHuman-founded professional cricket batsman.

Uses the local (non-cloud) MetaHuman Creator Python API bundled with UE 5.8's
MetaHumanCharacter plugin -- no MetaHuman Creator web app, no Quixel Bridge login. Sculpts an
athletic male body via the plugin's parametric body constraints (there is no face-sculpt-from-
scratch API exposed to Python at this fidelity, so the face stays the plugin's own default
archetype face, which is not any real player's likeness). Everything here is driven by the
project's actual, present-on-disk MetaHumanCharacterEditorSubsystem API, confirmed against
Tools/MetaHumanDiag.py output -- not guessed.

------------------------------------------------------------------------------------------------
CRASH FIX -- read this before changing anything below (verified 2026-09-11)
------------------------------------------------------------------------------------------------
Assembling crashed hard on this machine with a native fault in:

    UTG_AsyncExportTask::TG_AsyncExportTask(...)
    <- UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials(...)
    <- UMetaHumanDefaultEditorPipelineBase::ProcessBakedMaterials(...)
    <- UMetaHumanDefaultEditorPipelineBase::BuildCollection(...)
    <- UMetaHumanCollection::Build(...)
    <- FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter(...)

The bake is behind three gates, ALL of which sit inside `bBakeMaterials`:

    MetaHumanDefaultEditorPipelineBase.cpp:769   if (bBakeMaterials)                 -> ProcessBakedMaterials
    MetaHumanDefaultEditorPipelineBase.cpp:1015  if (!FaceBakingOptions...IsNull())  -> TryBakeMaterials
    MetaHumanDefaultEditorPipelineBase.cpp:1046  if (!BodyBakingOptions...IsNull())  -> TryBakeMaterials

`bBakeMaterials` is UPROPERTY(EditAnywhere) on UMetaHumanDefaultEditorPipelineBase, and it is
reachable from Python through UMetaHumanDefaultPipeline::EditorPipeline, which is
UPROPERTY(EditAnywhere, NoClear, Instanced). The C++ accessors GetSelectedPipeline() /
GetMutableEditorPipeline() are plain virtuals (not UFUNCTION) so they are NOT callable from
Python -- but the `EditorPipeline` member they hand back IS a readable/writable UPROPERTY.
That is the whole trick: force the flag off on the pipeline CDOs, and every instance the build
creates afterwards (NewObject copies Instanced subobjects from the CDO) inherits False.

Measured state of the shipped pipeline blueprints on this machine:

    BP_DefaultPipeline                      bBakeMaterials = False
    BP_DefaultLegacyPipeline                bBakeMaterials = True   <- was the crash source
    BP_DefaultLegacyPipeline_High/Medium/Low            = False
    BP_DefaultUEFNPipeline_High                         = True   <- also baked
    BP_DefaultUEFNPipeline_Medium/Low                   = False

So `force_no_bake()` below is not optional on this project -- removing it brings the crash back.

Turning baking off does NOT reduce final fidelity: baking exists to *simplify* materials for
faster realtime rendering by pre-baking them to textures. With it off we keep the pipeline's
own full-fidelity materials, which is the better trade for a hero player.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/MetaHumanBuildPlayer001.py
"""
import unreal as u

TARGET_PATH = '/Game/Cricket26/Characters/MetaHumans/Players/Player_001'
ASSET_NAME = 'MH_C26_Player_001'

# JOINTS_ONLY is temporary because Epic's Full Rig cloud service currently times out after 300s
# (confirmed repeatedly: request accepted, HTTP request to mh-uemhc-autorig-service times out at
# 300.00s, service then returns 'Server Error'; not a local network/DNS block -- the service and
# S3 are both reachable, auth succeeds). Flip this to True once that service is reliable again and
# re-run this same script against the same MH_C26_Player_001 -- see Phase 14 note below the
# auto-rig block for why that upgrade is non-destructive.
USE_FULL_RIG = False

# Target tier. Cinematic is the legacy hero tier; Optimized is the game-ready tier. HIGH is a real
# Epic-defined tier (EMetaHumanQualityLevel: Low/Medium/High/Cinematic, MetaHumanSDKRuntime/Public/
# MetaHumanTypes.h). Raise to CINEMATIC only after HIGH is confirmed working end to end -- the
# plugin logs "MetaHuman Optional Content folder not found ... limited features" on this machine,
# and the cinematic tier depends on that optional content (grooms etc).
PIPELINE_QUALITY = u.MetaHumanQualityLevel.HIGH


def log(msg):
    u.log('C26_MH_BUILD %s' % msg)


# --------------------------------------------------------------------------------------------
# STEP 0 -- make the TextureGraph bake crash unreachable. See the header for the full evidence.
# --------------------------------------------------------------------------------------------
BAKE_NAMES = ['bBakeMaterials', 'b_bake_materials', 'BakeMaterials']
EP_NAMES = ['EditorPipeline', 'editor_pipeline']

PIPELINE_BP_PATHS = [
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultPipeline.BP_DefaultPipeline_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline.BP_DefaultLegacyPipeline_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_High.BP_DefaultLegacyPipeline_High_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Medium.BP_DefaultLegacyPipeline_Medium_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Low.BP_DefaultLegacyPipeline_Low_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_High.BP_DefaultUEFNPipeline_High_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Medium.BP_DefaultUEFNPipeline_Medium_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Low.BP_DefaultUEFNPipeline_Low_C',
]

# Editor-pipeline classes whose own CDO may carry the flag directly.
EDITOR_PIPELINE_CLASSES = [
    'MetaHumanCollectionEditorPipeline',
    'MetaHumanDefaultEditorPipeline',
    'MetaHumanDefaultEditorPipelineLegacy',
    'MetaHumanDefaultEditorPipelineUEFN',
]

# Collection-pipeline classes whose CDO holds an Instanced EditorPipeline subobject.
COLLECTION_PIPELINE_CLASSES = [
    'MetaHumanDefaultPipeline',
    'MetaHumanDefaultPipelineLegacy',
    'MetaHumanDefaultPipelineUEFN',
]


def _try_get(obj, names):
    err = None
    for n in names:
        try:
            return n, obj.get_editor_property(n), None
        except Exception as e:
            err = e
    return None, None, err


def _try_set(obj, names, value):
    err = None
    for n in names:
        try:
            obj.set_editor_property(n, value)
            return n, None
        except Exception as e:
            err = e
    return None, err


def _force_on_editor_pipeline(ep, label):
    """Set bBakeMaterials=False on an editor-pipeline object and read it back as proof."""
    if ep is None:
        return False
    bname, before, berr = _try_get(ep, BAKE_NAMES)
    if bname is None:
        log('  no bBakeMaterials on %s (%s)' % (label, berr))
        return False
    if before is False or before == 0:
        return True  # already off
    sname, serr = _try_set(ep, BAKE_NAMES, False)
    if sname is None:
        log('  SET FAILED on %s (%s)' % (label, serr))
        return False
    _, after, _ = _try_get(ep, BAKE_NAMES)
    ok = (after is False or after == 0)
    log('  %-44s bBakeMaterials %s -> %s %s' % (label, before, after, 'OK' if ok else 'NOT APPLIED'))
    return ok


def _force_on_collection_cdo(cdo, label):
    """Walk cdo.EditorPipeline and force the flag off there."""
    name, ep, err = _try_get(cdo, EP_NAMES)
    if ep is None:
        return False
    return _force_on_editor_pipeline(ep, label)


def force_no_bake():
    log('--- STEP 0: disabling material bake (bypasses UTG_AsyncExportTask crash) ---')
    turned_off = []

    # (a) editor-pipeline class CDOs -- flag lives directly on the object
    for cn in EDITOR_PIPELINE_CLASSES:
        cls = getattr(u, cn, None)
        if cls is None:
            continue
        try:
            cdo = u.get_default_object(cls)
        except Exception:
            continue
        if cdo is None:
            continue
        if _force_on_editor_pipeline(cdo, cn):
            turned_off.append(cn)

    # (b) collection-pipeline class CDOs -- flag lives on the Instanced EditorPipeline subobject
    for cn in COLLECTION_PIPELINE_CLASSES:
        cls = getattr(u, cn, None)
        if cls is None:
            continue
        try:
            cdo = u.get_default_object(cls)
        except Exception:
            continue
        if cdo is None:
            continue
        if _force_on_collection_cdo(cdo, cn):
            turned_off.append(cn)

    # (c) the shipped pipeline blueprint classes -- these are what the build actually instantiates
    for path in PIPELINE_BP_PATHS:
        short = path.rsplit('/', 1)[-1]
        cls = u.load_object(None, path)
        if cls is None:
            continue
        if not isinstance(cls, u.Class):
            try:
                cls = cls.get_class()
            except Exception:
                continue
        try:
            cdo = u.get_default_object(cls)
        except Exception:
            continue
        if cdo is None:
            continue
        if _force_on_collection_cdo(cdo, short):
            turned_off.append(short)

    log('bake disabled on %d pipeline object(s)' % len(turned_off))
    return turned_off


force_no_bake()


# --------------------------------------------------------------------------------------------
# STEP 1 -- get the character asset
# --------------------------------------------------------------------------------------------
subsystem = u.get_editor_subsystem(u.MetaHumanCharacterEditorSubsystem)
asset_tools = u.AssetToolsHelpers.get_asset_tools()

existing_path = TARGET_PATH + '/' + ASSET_NAME
if u.EditorAssetLibrary.does_asset_exist(existing_path):
    log('found existing asset, reusing: %s' % existing_path)
    character = u.load_asset(existing_path)
else:
    character = asset_tools.create_asset(
        asset_name=ASSET_NAME,
        package_path=TARGET_PATH,
        asset_class=u.MetaHumanCharacter,
        factory=u.new_object(type=u.MetaHumanCharacterFactoryNew),
    )
    log('created: %s' % character)

if character is None:
    log('FATAL could not create or load character asset')
    quit()

if not subsystem.try_add_object_to_edit(character):
    log('FATAL could not open character for edit (already open elsewhere?)')
    quit()


# --------------------------------------------------------------------------------------------
# STEP 2 -- body sculpt (only when the asset is new; re-running must not redo this blindly)
# --------------------------------------------------------------------------------------------
def apply_body_constraints():
    try:
        # Constraint names confirmed by MetaHumanDiag.py: Height, Chest, Bicep, Forearm, Thigh,
        # Calf, Waist, Across Shoulder, Masculine/Feminine, Muscularity, etc.
        body_constraints = subsystem.get_body_constraints(character)
        by_name = {str(c.name).lower().replace(' ', '_').replace('/', '_'): c
                   for c in body_constraints}

        # (constraint_key, target_cm, note)
        targets = [
            ('height', 182.0, 'brief: 182cm target scale used elsewhere in the project'),
            ('across_shoulder', 47.0, 'realistic athletic shoulder width, not exaggerated'),
            ('chest', 98.0, 'developed but natural upper body'),
            ('bicep', 32.0, 'strong, not bodybuilder'),
            ('forearm', 27.5, 'strong forearms for bat grip'),
            ('waist', 80.0, 'lean athletic waist'),
            ('thigh', 55.0, 'athletic thighs'),
            ('calf', 37.0, 'athletic calves'),
            ('masculine_feminine', 1.0, 'fully masculine end of the slider'),
            ('muscularity', 0.55, 'moderate: athletic, not extreme'),
        ]
        applied = []
        for key, value, note in targets:
            c = by_name.get(key)
            if c is None:
                log('SKIP constraint not found: %s' % key)
                continue
            try:
                c.is_active = True
                # masculine_feminine / muscularity read as normalized sliders rather than cm
                # measurements -- set the same way, but treat the units as unconfirmed.
                c.target_measurement = value
                applied.append(key)
            except Exception as e:
                log('FAIL set constraint %s: %s' % (key, e))
        subsystem.set_body_constraints(character, list(by_name.values()))
        subsystem.commit_body_state(character)
        log('applied body constraints: %s' % applied)
    except Exception as e:
        log('FAIL body sculpt stage: %s' % e)


# --------------------------------------------------------------------------------------------
# STEP 3 -- rig + texture sources, SKIPPED when the character is already buildable
# --------------------------------------------------------------------------------------------
# Both of these are Epic cloud calls and are intermittent on this machine (the full-rig request
# has been observed to answer in seconds once and to hang for 8+ minutes the next time). There is
# no reason to re-invoke them on a character that is already in a buildable state, so probe first.
buildable = False
try:
    buildable = subsystem.can_build_meta_human(character)
except Exception as e:
    log('FAIL can_build_meta_human probe: %s' % e)

if buildable:
    log('character already buildable -- skipping auto-rig and texture-source cloud calls')
else:
    apply_body_constraints()

    rig_type = (u.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES if USE_FULL_RIG
                else u.MetaHumanRigType.JOINTS_ONLY)
    try:
        rig_req = u.MetaHumanCharacterAutoRiggingRequestParams()
        rig_req.blocking = True
        rig_req.report_progress = False
        rig_req.rig_type = rig_type
        subsystem.request_auto_rigging(character, rig_req)
        log('auto-rigging request returned (blocking, %s)' % rig_type)
    except Exception as e:
        log('FAIL auto-rigging: %s' % e)

    # Discovered empirically, not assumed: build_meta_human refuses to assemble with the real error
    # "The Character is missing textures, use Download Texture Sources to create them before
    # assembling" even when auto-rigging itself succeeded cleanly. can_build_meta_human() folds
    # this in too (it was False right after a rig that the log shows finished with no error), so it
    # is not a pure rig-state check. request_texture_sources is Epic's own documented API for this
    # (Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Python/examples/example_download_textures.py)
    # -- not a substitute or fabricated asset, the real synthesis/fetch call assembly needs.
    try:
        tex_req = u.MetaHumanCharacterTextureRequestParams()
        tex_req.blocking = True
        tex_req.report_progress = False
        subsystem.request_texture_sources(character, tex_req)
        log('texture sources request returned (blocking)')
    except Exception as e:
        log('FAIL request_texture_sources: %s' % e)

    # Do not infer success from either call merely returning -- ask the subsystem whether the
    # character is actually in a buildable state before trying to assemble it.
    try:
        buildable = subsystem.can_build_meta_human(character)
        log('can_build_meta_human after auto-rig + texture sources = %s' % buildable)
    except Exception as e:
        log('FAIL can_build_meta_human check: %s' % e)


# --------------------------------------------------------------------------------------------
# STEP 4 -- assemble into a real skeletal mesh
# --------------------------------------------------------------------------------------------
if not buildable:
    log('SKIP build_meta_human: character is not in a buildable state, see rig/texture results '
        'above for the actual failure reason (this is not treated as a soft warning)')
else:
    try:
        build_params = u.MetaHumanCharacterEditorBuildParameters()
        build_params.pipeline_type = u.MetaHumanDefaultPipelineType.OPTIMIZED
        build_params.pipeline_quality = PIPELINE_QUALITY
        build_params.absolute_build_path = TARGET_PATH
        build_params.common_folder_path = '/Game/Cricket26/Characters/MetaHumans/Common'
        build_params.enable_wardrobe_item_validation = False

        # Read back what the struct actually accepted -- a silently-ignored assignment here is
        # exactly how the previous attempt ended up on the baking Cinematic pipeline.
        for pname in ('pipeline_type', 'pipeline_quality', 'absolute_build_path'):
            try:
                log('build_params.%s = %s' % (pname, build_params.get_editor_property(pname)))
            except Exception as e:
                log('build_params.%s unreadable (%s)' % (pname, e))

        log('calling build_meta_human (bake disabled, quality=%s)' % PIPELINE_QUALITY)
        subsystem.build_meta_human(character=character, params=build_params)
        log('build_meta_human returned')
    except Exception as e:
        log('FAIL build_meta_human: %s' % e)


if subsystem.is_object_added_for_editing(character):
    subsystem.remove_object_to_edit(character)

try:
    u.EditorAssetLibrary.save_asset(existing_path)
    log('saved %s' % existing_path)
except Exception as e:
    log('FAIL save: %s' % e)
log('DONE')
