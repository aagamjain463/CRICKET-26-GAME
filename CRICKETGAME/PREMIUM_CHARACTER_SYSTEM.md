# CRICKET 26 character presentation — implementation status

**In progress; not approved for match migration.** See [handoff](HANDOFF_PREMIUM_CHARACTERS.md) for exact assets, commands, tests and next steps, and [root causes](Docs/PREMIUM_CHARACTER_ROOT_CAUSES.md) for verified baseline findings.

`AC26Athlete` remains the authoritative match actor. `UC26CharacterPresentationComponent` can replace its visible body and equipment after `UC26CharacterProfile` validation; it does not spawn a second player or modify scoring. The old posing path is bypassed when activation succeeds. No approved profile exists yet, so the normal match remains on its old visuals.

The replacement uses a real SkeletalMeshComponent and `UC26CricketerAnimInstance`, with full-body sequence evaluators and smooth transitions. Measured actor displacement drives speed, direction, acceleration and turns; exact contact-time evaluations do not advance stride. Simulation release/contact frames map to named asset markers. Root motion is disabled to keep match movement authoritative. Replay now has sequence/time/blend snapshots, pending visual regression testing.

Roles are centralized: batter/non-striker require batting pads, gloves, bat and helmet; bowler/normal fielder have none of that equipment; keeper requires separate keeper pads/gloves; umpire uses an alternate outfit without cricket gear. Equipment uses bone sockets and fixed relative transforms. There is no duplicate equipment ball. Handedness uses distinct animation keys and alternate bat sockets; complete roster integration remains pending.

The new original MetaHuman assembly is saved with body, face, materials and PhysicsAsset. Blender source for a combined anatomical full-body candidate with new garment geometry and fitted original cricket shoes is in `ArtSource/Premium/FullBody/`; it is not imported or visually approved yet. Source and target IK Rigs plus a Retargeter exist, but retarget pose/motion quality is unverified.

Profile validation rejects missing body/skeleton, incompatible animation skeletons, absent required bones/weighted geometry in imported LODs, missing equipment/sockets, duplicate equipment, missing contact markers, root-motion clips and absent approval evidence. New character tests cover role isolation, actual displacement, event mapping and native skeletal evaluation. The latest recorded suite passed 12 tests; final replay/handedness edits need regression tests.

Quality selection changes skeletal LOD and shadows without changing equipment or disabling basic animation. No mobile device profiling has been performed. Motion Matching, Control Rig/FBIK, foot locking, hand IK, facial motion and Motion Warping have not been implemented. Authored cricket action assets, keeper kit, umpire outfit, final material/LOD fitting and mandatory match acceptance remain outstanding.

To add a player/body/action/equipment item, use the profile's named asset fields and common skeleton, import compatible authored assets, run its validation, inspect the visual result and only then approve migration. Exact required clip keys are in `C26CharacterProfile.cpp`. Do not create placeholder clips to satisfy the validator.
