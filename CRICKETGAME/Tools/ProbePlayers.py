"""Read-only probe of every player-facing asset. Answers, in one editor run:

  * which skeletal meshes exist, their triangle/vertex counts, skeleton and bounds;
  * what each material slot on the animated kit body is called and what is bound to it;
  * every material's parameter names and which of them are texture parameters (a texture
    parameter with no texture assigned is the "flat plastic" failure mode);
  * whether MH_C26_Player_001 has a skeleton yet.

Run: UnrealEditor-Cmd <abs>/CRICKETGAME.uproject -run=pythonscript -script=<abs>/Tools/ProbePlayers.py
Writes Artifacts/probe_players.txt. stdout logging in this commandlet is unreliable, so the
report goes to a file rather than the log.
"""
import unreal as u

LIB = u.EditorAssetLibrary
ML = u.MaterialEditingLibrary

LINES = []


def log(msg):
    LINES.append(str(msg))


def _try(fn, default='?'):
    try:
        return fn()
    except Exception:
        return default


def dump_material(path):
    mat = LIB.load_asset(path)
    if not mat:
        log('MATERIAL MISSING %s' % path)
        return
    scalars = [(n, round(ML.get_material_default_scalar_parameter_value(mat, n), 3))
               for n in ML.get_scalar_parameter_names(mat)]
    vectors = []
    for n in ML.get_vector_parameter_names(mat):
        c = ML.get_material_default_vector_parameter_value(mat, n)
        vectors.append((n, tuple(round(v, 3) for v in (c.r, c.g, c.b))))
    textures = []
    for n in ML.get_texture_parameter_names(mat):
        t = ML.get_material_default_texture_parameter_value(mat, n)
        textures.append((n, t.get_name() if t else 'NONE'))
    wired = {}
    for prop in ('MP_BASE_COLOR', 'MP_NORMAL', 'MP_ROUGHNESS', 'MP_SPECULAR',
                 'MP_EMISSIVE_COLOR', 'MP_METALLIC', 'MP_AMBIENT_OCCLUSION'):
        src = _try(lambda: ML.get_material_property_input_node(
            mat, getattr(u.MaterialProperty, prop)), None)
        if src:
            wired[prop.replace('MP_', '')] = src.get_class().get_name().replace(
                'MaterialExpression', '')
    log('MATERIAL %-22s blend=%-16s two_sided=%s' % (
        mat.get_name(), _try(lambda: mat.get_editor_property('blend_mode')),
        _try(lambda: mat.get_editor_property('two_sided'))))
    log('    scalar  %s' % scalars)
    log('    vector  %s' % vectors)
    log('    texture %s' % textures)
    log('    wired   %s' % wired)


def dump_skeletal(path):
    mesh = LIB.load_asset(path)
    if not mesh:
        log('SKELETAL MISSING %s' % path)
        return
    skel = _try(lambda: mesh.get_editor_property('skeleton'), None)
    lod = _try(lambda: mesh.get_editor_property('lod_info'), None)
    tris = _try(lambda: lod[0].get_editor_property('num_triangles'))
    verts = _try(lambda: lod[0].get_editor_property('num_vertices'))
    ext = _try(lambda: mesh.get_bounds().box_extent, None)
    height = '%.1f' % (ext.z * 2) if ext else '?'
    slots = _try(lambda: [(i, str(s.material_slot_name),
                           s.material.get_name() if s.material else 'NONE')
                          for i, s in enumerate(mesh.get_editor_property('materials'))])
    log('SKELETAL %-32s skel=%-26s lods=%s tris=%s verts=%s height=%s' % (
        mesh.get_name(), skel.get_name() if skel else 'NONE',
        len(lod) if lod else '?', tris, verts, height))
    log('    slots %s' % slots)


log('=== skeletal players (Characters/) ===')
for name in ('SK_Cricketer', 'SK_Cricketer_KitBase', 'SK_Cricketer_KitCricket',
             'SK_Cricketer_Match'):
    dump_skeletal('/Game/Cricket26/Characters/' + name)

log('=== skeletal hero bodies (Characters/Players/) ===')
for name in ('SK_Cricketer_HeroBatter', 'SK_Cricketer_HeroBowler',
             'SK_Cricketer_HeroKeeper', 'SK_Cricketer_HeroUmpire',
             'SK_Cricketer_HeroFielder01'):
    dump_skeletal('/Game/Cricket26/Characters/Players/' + name)

log('=== hero static scans (Characters/Players/) ===')
for name in ('SM_C26_Player_Batter', 'SM_C26_Player_Bowler', 'SM_C26_Player_Keeper'):
    sm = LIB.load_asset('/Game/Cricket26/Characters/Players/' + name)
    if not sm:
        log('STATIC MISSING %s' % name)
        continue
    lod = _try(lambda: sm.get_editor_property('lod_for_collision'), None)
    ext = _try(lambda: sm.get_bounds().box_extent, None)
    log('STATIC %-28s tris=%s verts=%s height=%.1f slots=%s' % (
        name, _try(lambda: sm.get_editor_property('lod_info')[0].get_editor_property('num_triangles')),
        _try(lambda: sm.get_editor_property('lod_info')[0].get_editor_property('num_vertices')),
        ext.z * 2 if ext else -1,
        _try(lambda: [(str(m.material_slot_name), m.material.get_name() if m.material else 'NONE')
                      for m in sm.get_editor_property('static_materials')])))

log('=== metahuman ===')
mh = LIB.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001')
log('MH asset = %s' % (mh.get_name() if mh else 'MISSING'))
if mh:
    log('MH class = %s' % mh.get_class().get_name())
    for prop in ('body_state', 'has_skeleton', 'skeletal_mesh', 'rigged'):
        log('MH.%s = %s' % (prop, _try(lambda: mh.get_editor_property(prop))))

log('=== materials ===')
for name in ('M_Surface', 'M_Fabric', 'M_C26_Kit', 'M_C26_PlayerSkin', 'M_Willow',
             'M_Athlete_PBR', 'M_Shade', 'M_C26_Cloth', 'M_C26_Gear', 'M_C26_Shell'):
    dump_material('/Game/Cricket26/Materials/' + name)

log('=== animations ===')
for name in ('A_Run', 'A_Idle', 'A_Catch', 'A_FielderThrow'):
    a = LIB.load_asset('/Game/Cricket26/Animations/' + name)
    if not a:
        log('ANIM MISSING %s' % name)
        continue
    log('ANIM %-18s length=%.3fs tracks=%s' % (
        name, _try(lambda: a.get_editor_property('sequence_length')),
        _try(lambda: len(a.get_editor_property('bone_tracks')))))

log('C26_PROBE_COMPLETE')

with open('/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/Artifacts/probe_players.txt',
          'w') as handle:
    handle.write('\n'.join(LINES) + '\n')
