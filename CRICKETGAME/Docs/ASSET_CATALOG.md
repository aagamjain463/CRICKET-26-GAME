# CRICKET 26 — Asset Integration Catalog

## Overview & Executive Summary

This document serves as the technical and art catalog for the 10 downloaded high-poly FBX assets integrated into **CRICKET 26**. Each asset underwent an automated production pipeline:
1. **Inspection & Classification**: Topology auditing, UV inspection, and texture extraction using Blender 5.2.
2. **Mesh Optimization**: Water-tight decimation from raw scan/AI densities (~750k vertices down to game-ready budgets) and LOD generation.
3. **Rigging & Skinning**: Skeleton retargeting against the 67-bone Unreal `SK_Cricketer_KitBase` armature via KDTree weighted deformation.
4. **Equipment Extraction & Modeling**: Regulation cricket dimensioning (ball diameter 7.2 cm, bat height 87.3 cm, stumps height 71.1 cm, bails 11.1 cm).
5. **Texture & Material Pipeline**: Native UE5 PBR texture ingestion at 4096×4096 resolution with roughness separation to eliminate plastic/wax artifacts.
6. **Engine & Gameplay Wiring**: C++ replacement of procedural shapes in `AC26MatchGameMode` and dynamic socket dressing in `AC26Athlete`.

---

## Asset Classification & Integration Matrix

| Raw Hash | Role / Asset Name | Raw Geometry | Optimized Budget | UE5 Asset Destination | In-Game Systems Connected |
|---|---|---|---|---|---|
| `01_04aa892e` | Outfield Athlete 01 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_01_D` | Boundary sweepers & deep cover fielders |
| `02_4260cecc` | Official / Umpire | 749,620 v / 1.5M t | LOD0: 26k, LOD1: 11k, LOD2: 3k | `/Game/Cricket26/Textures/T_Umpire_Hero_D` | Bowler-end umpire & square leg umpire |
| `03_f4e20b7b` | Outfield Athlete 03 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_03_D` | Mid-wicket & long-on fielders |
| `04_2beed08e` | Outfield Athlete 04 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_04_D` | Slip cordon & gully fielders |
| `05_0434293e` | Outfield Athlete 05 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_05_D` | Point & cover fielders |
| `06_d8114a2f` | Hero Striker / Batter | 749,620 v / 1.5M t | LOD0: 26k, LOD1: 12k, LOD2: 4k | `/Game/Cricket26/Characters/SK_Cricketer_Match` | Striker & non-striker active batters |
| `07_7d3019fc` | Wicketkeeper | 749,620 v / 1.5M t | LOD0: 26k, LOD1: 11k, LOD2: 3k | `/Game/Cricket26/Textures/T_Keeper_Hero_D` | Behind-the-stumps gloveman |
| `08_37929429` | Hero Bowler | 749,620 v / 1.5M t | LOD0: 26k, LOD1: 11k, LOD2: 3k | `/Game/Cricket26/Textures/T_Bowler_Hero_D` | Pace bowler in run-up and delivery |
| `09_19b08966` | Outfield Athlete 09 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_09_D` | Fine leg & third man fielders |
| `10_7996719e` | Outfield Athlete 10 | 749,620 v / 1.5M t | LOD0: 25k, LOD1: 10k, LOD2: 3k | `/Game/Cricket26/Textures/T_Athlete_10_D` | Extra cover & bowler follow-through |

---

## Equipment & Props Roster

In addition to character texturing and skinning, 13 custom regulation equipment assets were authored and imported into `/Game/Cricket26/Equipment/`:

1. **`SM_C26_Ball_Hero`**:
   - **Dimensions**: Regulation 7.20 cm diameter.
   - **Features**: Distinct raised stitched center seam with dual-hemisphere leather curvature.
   - **Gameplay Integration**: Replaced UE default sphere in `AC26MatchGameMode::BeginPlay()` and `AC26MatchGameMode::StartDelivery()`.

2. **`SM_C26_Stump_Single` & `SM_C26_Bail_Single`**:
   - **Dimensions**: Regulation 71.1 cm height, 3.8 cm diameter stump; 11.1 cm bail.
   - **Features**: Turned ash wood dome profile, brass ground ferrule base, spigot notches for bails.
   - **Gameplay Integration**: Replaced cylinder primitives in `AC26MatchGameMode::CreateStumps()`. Physics simulations in `AC26MatchGameMode::BreakWicket()` accurately dislodge individual middle/off/leg stumps and send individual bails spinning into the air.

3. **`SM_C26_Bat_Hero`**:
   - **Dimensions**: 87.3 cm overall height (56.0 cm blade, 31.3 cm handle).
   - **Features**: Dual concave spine profile, cane handle with rubber ripple grip, branded blade stickers.
   - **Gameplay Integration**: Attached to socket `BatSocket` on `SK_Cricketer_Match`.

4. **Protective Kit (`SM_C26_Helmet_Hero`, `SM_C26_HelmetGrille_Hero`, `SM_C26_Cap_Hero`, `SM_C26_Pad_L`, `SM_C26_Pad_R`, `SM_C26_Glove_L`, `SM_C26_Glove_R`, `SM_C26_Shoe_L`, `SM_C26_Shoe_R`)**:
   - **Features**: Multi-material slots (`PadFace`, `PadRoll`, `PadStrap`, `GlovePalm`, `ShoeUpper`, `ShoeSole`).
   - **Gameplay Integration**: Dynamically dressed via `AC26Athlete::Dress()` with physical roughness and team livery tints.

---

## Verification & Match Acceptance

- **Full Match Run**: Deterministic visual acceptance run completed across both innings with score `15/1` vs `14/1`.
- **All 25 Match Beats Captured**: Zero missing or skipped frames.
- **Physical Collision & Replays Verified**: Ball bounce, edge detections, caught out dismissals, bowled wicket dislodgement, sixes, and fours all rendered seamlessly.
