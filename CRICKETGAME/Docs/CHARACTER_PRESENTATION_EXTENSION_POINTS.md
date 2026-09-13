# Character presentation extension points

## Current integration

The playable match spawns 14 `AC26Athlete` actors. `Configure()` attempts the existing
validated `UC26CharacterPresentationComponent` profile and otherwise keeps the legacy
`UC26PoseMesh` body, procedural/recorded pose evaluation, and mesh-space equipment placement.
The match still owns actor movement and action timing. Hand, receiving and bat queries
retain their existing implementations because they participate in gameplay contact checks.

This architecture-only change assigns no new assets and enables no replacement characters.
The default profile path remains
`/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer.DA_C26_DefaultPlayer`.
This checkout has no approved default profile, so ordinary matches keep their current visuals.

## Configuration and extension points

| Entry point | Responsibility / contract |
| --- | --- |
| `AC26Athlete::VisualBody()` | Presentation-only access to the active skinned component, whether legacy pose mesh or profile body. Do not use it to replace the existing gameplay contact queries. |
| `UC26CharacterPresentationComponent::ProfileAsset` | Editable soft profile reference, selected **before first activation**. Existing development `-C26CharacterProfile=` override takes precedence. Structural and approval validation remains mandatory under the existing activation rules. Changing the reference on an already active component is not a hot-swap operation. |
| `UC26CharacterProfile::ResolveBody(Role, Preset)` | Central body selection: umpire mesh takes precedence; other roles use a named preset or the default body. |
| `AssignBodyMesh()` | Component-owned binding point. Clears old slot-index material overrides only when the mesh changes, then applies configured named overrides. Leaves the existing animation driver in place. |
| `MaterialOverrides` / `ValidateMaterialOverrides()` | Optional existing materials keyed by imported material slot name. Empty by default. Non-null materials and matching slot names are required on body, umpire and all presets. Useful for body, integrated head and clothing surfaces. |
| `ApplyBodyMaterials()` | Reapplies named overrides during configuration, then the existing team jersey material. Team jersey assignment has final precedence. |
| `FC26EquipmentDefinition::ResolveSocket()` / `ResolveOffset()` | Single handedness rule for equipment attachment. Without a left-handed socket, **both** socket and offset fall back to the normal pair. |
| `RefreshEquipmentAttachments()` | Component-owned socket attachment and local transform assignment. Existing role visibility rules remain in place. |

The head and clothing currently remain part of the profile's body mesh; there is no new
separate head/hair component or facial animation system. A future modular head should be
implemented within presentation, respecting the selected skeleton and attachment contract.

Animations already use `UC26CharacterProfile::Clips`, evaluated by
`UC26CricketerAnimInstance`. That remains the animation extension point. Replacement clips
must satisfy the existing shared-skeleton, in-place, role and contact-marker requirements.
The legacy body still uses its existing Mixamo-name/bind-pose assumptions. An arbitrary
skeleton replacement is not a supported mesh-only swap in either path.

## Files changed

Under `Source/CRICKETGAME/`:

- `SuperOver/C26Athlete.h` and `.cpp`: presentation-only body accessor and required include.
- `Characters/C26CharacterPresentationComponent.h` and `.cpp`: editable profile reference;
  centralized body/material assignment and equipment attachment helpers.
- `Characters/C26CharacterProfile.h` and `.cpp`: body/attachment resolvers and optional
  validated named material overrides.
- `Characters/C26CharacterTests.cpp`: `Cricket26.Characters.VisualConfiguration` covers body
  fallback, umpire precedence, handed socket/offset pairing and material-slot validation.

Documentation: `Docs/CHARACTER_PRESENTATION_EXTENSION_POINTS.md` (this file).
No content assets, project settings, spawn code, match logic, animation assets or cameras changed.

## Verification status

- Attempted UE 5.8 `CRICKETGAMEEditor Mac Development` build **before editing**: failed in
  existing `SuperOver/C26CameraDirector.cpp`, undeclared `Striker` at lines 466, 490 and 491.
- Rebuilt after changes: all modified C++ translation units, including the configuration
  test, compiled successfully. Final build still fails on those same camera errors.
- `git diff --check` passed.
- New automation and rebuilt normal-match runtime/visual regression are **blocked**, not
  passed. An older binary would not validate these source changes.

After the existing camera compile error is resolved, run from the project directory:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" CRICKETGAMEEditor Mac Development -Project="$PWD/CRICKETGAME.uproject" -WaitMutex
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CRICKETGAME.uproject" -nullrhi -unattended -nosplash -nosound '-ExecCmds=Automation RunTests Cricket26; Quit' '-TestExit=Automation Test Queue Empty'
bash Tools/GoldenGate.sh presentation_architecture -C26GateFPS=30
```

Run Unreal processes sequentially. The existing GoldenGate exercises the real match's
delivery/contact, fielding, next-delivery and restart flow. Review rendered captures as
well as its result before claiming unchanged runtime appearance.
