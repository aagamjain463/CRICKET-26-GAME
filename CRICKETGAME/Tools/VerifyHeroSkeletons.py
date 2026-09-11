"""Sanity check: report which skeleton asset each new hero skeletal mesh is actually bound to."""
import unreal as u

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Characters/Players'
BASE = '/Game/Cricket26/Characters/SK_Cricketer_KitBase'

base = LIB.load_asset(BASE)
base_skel = base.get_editor_property('skeleton')
u.log('C26_VERIFY base_skeleton=%s' % base_skel.get_path_name())

for role in ['HeroBatter', 'HeroBowler', 'HeroKeeper', 'HeroUmpire',
             'HeroFielder01', 'HeroFielder02', 'HeroFielder03',
             'HeroFielder04', 'HeroFielder05', 'HeroFielder06']:
    asset = LIB.load_asset(DEST + '/SK_Cricketer_%s' % role)
    if not asset:
        u.log_error('C26_VERIFY %s missing' % role)
        continue
    skel = asset.get_editor_property('skeleton')
    u.log('C26_VERIFY %s skeleton=%s same_as_base=%s bones=%d' %
          (role, skel.get_path_name() if skel else None, skel == base_skel,
           len(skel.get_editor_property('bone_tree')) if skel else -1))
