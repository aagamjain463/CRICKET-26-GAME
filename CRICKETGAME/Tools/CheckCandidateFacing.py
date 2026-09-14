import unreal as u

mesh = u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
skeleton = u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate_Skeleton')
u.log('C26_CHECK_MESH ' + str(mesh))

# Let's inspect reference skeleton bone transforms
ref_skel = skeleton.get_reference_skeleton() if hasattr(skeleton, 'get_reference_skeleton') else None
u.log('C26_CHECK_REF ' + str(ref_skel))

# Let's check an animation clip: A_C26_BatterReady_R and A_C26_STRAIGHTDRIVE_R
clip = u.load_asset('/Game/Cricket26/Characters/Animations/Cricket/A_C26_STRAIGHTDRIVE_R')
u.log('C26_CHECK_CLIP ' + str(clip))

# Check sockets on mesh
for s in mesh.get_editor_property('sockets'):
    u.log(f'C26_CHECK_SOCKET {s.socket_name} bone={s.bone_name} rel_loc={s.relative_location} rel_rot={s.relative_rotation}')
