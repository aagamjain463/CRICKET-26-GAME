# Performance budget / measurements

Targets (not measurements): high-end phone 60 FPS / 16.67 ms; mid-range phone
30 FPS / 33.33 ms, stable frame pacing. Phone verification remains outstanding.

Baseline test: UE 5.8.2 Mac Development game, Apple M5 / 16 GB, Metal SM6,
1600×900, capture quality 3. 17/17 beats complete. No quantified GPU/memory or
phone result yet. Desktop screenshots must not be reported as mobile performance.

Existing settings: Android/iOS 400 MB texture streaming pool, 1024 shadow maps,
two cascades, no motion blur. Low/medium/high/ultra concept exists; actual device
profile selection and iOS tier behavior need hardware verification.

Provisional asset ceilings to validate, not current inventory: hero 35k triangles,
mid 15k, distant 4k; hero textures ≤2K, mid ≤1K, distant ≤512; reuse kit materials.
Crowd must remain instanced/impostor geometry, not thousands of athlete skeletons.
Observe shader cost and overdraw before changing these budgets. Current procedural
athlete pose and trouser uploads occur for all 14 athletes each frame; profile before
introducing update-rate changes. Record scene, device, frame time, game/render/GPU,
memory and bottleneck after a measured pass.
