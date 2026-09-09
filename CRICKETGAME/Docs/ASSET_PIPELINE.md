# Asset pipeline

Reuse `/Game/Cricket26` conventions; preserve original source FBX and saved scenes.
Existing source root: `/Users/aagamjain/Desktop/CRICKET-26/Assets/_Project`.
Only generic Idle, Run, Catch and FielderThrow FBX are present there.
Blender 5.2.1 MCP responds; initial scene is an unsaved default cube, no actions.
Scene/prompt telemetry disabled before project inspection to respect private-file policy.
Cascadeur 2026.2.1 is installed; local MCP responds on 127.0.0.1:8765.
No Meshy integration was discovered or used.

Blocked premium assets: right-handed front-foot straight drive and fast bowling
run-up → gather → release → follow-through. Acquire licensed cricket motion or
record real cricket action with suitable consent/rights; clean feet, balance and
arcs in Cascadeur; retarget to production skeleton; export FBX; integrate deterministic
release/contact markers; test hand/bat contact in Unreal. Do not relabel procedural
motion as authored or premium. Generic running clips cannot substitute for bowling.

New DCC source should use ArtSource/Blender/{Characters,Equipment,Stadium,Props}
and ArtSource/Exports; create directories when an actual asset needs them.
For hero kit: inspect base, duplicate source, repair clothes/skin coverage, UVs,
weights, helmet/grille/glove silhouette; author LODs; export a new version, never
overwrite an irreplaceable source. Generated assets require legal provenance.
