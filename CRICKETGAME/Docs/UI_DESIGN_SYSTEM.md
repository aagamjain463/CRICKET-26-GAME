# CRICKET 26 — Commercial Sports Broadcast UI & Design System

## 1. Executive Summary & Design Vision

The **CRICKET 26 Design System** replaces prototype-level and AI-generated interface tropes (glowing neon cyan borders, crypto-dashboard tab bars, generic rounded glass rectangles, debug progress meters) with an **authentic, commercial sports broadcast presentation** inspired by modern sports entertainment benchmarks:
- **EA Sports FC / Cricket 24**: High-impact condensed athletic typography, solid layered card structures, and dedicated context badges.
- **Formula 1 / NBA 2K**: High-contrast telemetry ribbons, precision timing reticles, and tactile sports buttons.
- **International Cricket Broadcasts (Sky Sports, Fox Cricket, Star Sports)**: Compact lower-third and top-left broadcast score bugs, clean 6-ball over tickers with semantic colors, and TV-grade event stingers.

---

## 2. Color System & Semantic Palette

CRICKET 26 utilizes a **dark neutral/charcoal foundation** accented by **Championship Gold** and semantic sports signals:

| Token | Hex / Linear Value | Usage |
|---|---|---|
| `Void` | `#020304` (`.008f, .011f, .016f, .98f`) | Liquid Obsidian Carbon backing |
| `SurfaceBase` | `#040608` (`.016f, .022f, .032f, .94f`) | Deep Stadium Charcoal Canvas |
| `SurfaceCard` | `#06090C` (`.025f, .034f, .048f, .94f`) | Solid Elevated Sports Card Plane |
| `SurfaceWell` | `#030406` (`.012f, .016f, .024f, .96f`) | Recessed Inset Data Well |
| `SurfacePill` | `#0A0D12` (`.038f, .050f, .068f, .88f`) | Tactile Interactive Surfaces |
| `WhiteAthletic`| `#F5FAFF` (`.96f, .98f, 1.00f, 1.f`) | Primary Text & Digits |
| `SilverCool` | `#BDCCE0` (`.74f, .80f, .88f, 1.f`) | Secondary Labels & Subtext |
| `SlateMuted` | `#708099` (`.44f, .50f, .60f, 1.f`) | Metadata, Footers, Captions |
| `HairlineSoft`| `rgba(46,61,87, 0.42)` | 1px Structural Framing |
| `HairlineGleam`| `rgba(153,184,230, 0.22)`| Top-Edge Physical Sheen |
| `Gold` | `#FFC224` (`1.00f, .76f, .14f, 1.f`) | Primary Game Brand Accent, Trophies, 6s, Victory |
| `Crimson` | `#EB2433` (`.92f, .14f, .20f, 1.f`) | Wickets, Danger, Outs, Late Releases |
| `TurfGreen` | `#14D170` (`.08f, .82f, .44f, 1.f`) | Boundaries (4s), Sweet Spot, Perfect Release |
| `MumbaiBlue` | `#0A78F2` (`.04f, .47f, .95f, 1.f`) | Mumbai Meteors Club Identity |
| `MelbourneRed`| `#F83A52` (`.97f, .23f, .32f, 1.f`) | Melbourne Cyclones Club Identity |

---

## 3. Typographic Hierarchy

CRICKET 26 utilizes a strictly stratified four-tier font system powered by authentic sports fonts in `Content/Cricket26/UI/Fonts`:

1. **`SportsFont` (`DINCondensed-Bold.ttf`)**
   - **Role**: Hero scores (`148 / 4`), Over counters (`18.3 OVERS`), chase requirements, match clock, speedometers (`142 KM/H`).
   - **Character**: Tall, condensed, powerful sports broadcast standard.
2. **`DisplayFont` (`BarlowCondensed-Bold.ttf`)**
   - **Role**: Screen titles, club names, primary CTA button labels, navigation headers.
   - **Character**: Confident, punchy, modern athletic weight.
3. **`TitleFont` (`BarlowCondensed-SemiBold.ttf`)**
   - **Role**: High-legibility body descriptions, player roles, commentary subtitles, hints.
   - **Character**: Clean, legible, generous x-height.
4. **`HeavyFont` (`BarlowCondensed-ExtraBold.ttf`)**
   - **Role**: Event stingers (`WICKET!`, `SIX!`, `FOUR!`), milestone titles.
   - **Character**: Maximum visual punch and broadcast impact.

---

## 4. Spacing & Layout Rhythm

Every gap and every padding in the HUD resolves to one of the tokens declared in
`C26HUD.h`. The same relationship therefore always reads the same distance, instead of
drifting between 6/10/14/22/26 as screens were added one at a time.

### 4.1 The 8-Point Scale

| Token | Value | Token | Value |
|---|---|---|---|
| `Sp4` | 4 | `Sp24` | 24 |
| `Sp8` | 8 | `Sp32` | 32 |
| `Sp12` | 12 | `Sp48` | 48 |
| `Sp16` | 16 | `Sp64` | 64 |
| `Sp20` | 20 | | |

### 4.2 Semantic Aliases

Raw scale numbers are never used directly. Each token names a relationship:

| Alias | Value | Applies to |
|---|---|---|
| `GapLine` | 8 | Consecutive lines inside one block |
| `GapItem` | 12 | Tightly bound items (label ↔ value) |
| `GapComp` | 16 | Sibling components |
| `GapBlock` | 24 | Blocks inside one card |
| `GapSect` | 32 | Sections on a page |
| `PadCard` | 32 | Feature-card inner padding |
| `PadPanel` | 16 | Compact-panel inner padding |
| `PadEdge` | 12 | Minimum inset for text against a border |
| `SafeBot` | 858 | Bottom safe line for full-width UI |

### 4.3 The Leading Rule

`LineH(Size)` returns `Size * 1.48f` — the line's own leading. **A text chain must
advance by the previous line's own `LineH` plus `GapLine`:**

```cpp
const float TitleY = KickerY + LineH(14.f) + GapLine;
const float SubY   = TitleY  + LineH(44.f) + GapLine;
```

Advancing by a raw literal smaller than `LineH` is the single cause of every text
collision found in the HUD: a 44 pt title advanced by +26 px puts the next line *inside*
the title's box. Note also that `Text()` hangs glyphs *below* the Y it is given, so the
real gap is always slightly tighter than the code implies.

### 4.4 Content-Derived Card Heights

A card's height is computed from the same terms its draw uses, so top and bottom padding
come out equal whatever the card contains:

```cpp
const float PanelH = PadCard + (26.f + GapComp) + (LineH(64) + GapComp)
                   + (LineH(19) + GapBlock) + (48.f + GapBlock) + 56.f + PadCard;
```

Hard-coded card heights are what produced the 110–150 px dead bands on Home, Play, Help
and Result.

### 4.5 Fit Helpers

- `TextFit(S, X, Y, Size, Color, MaxW, ...)` — shrinks the font until the string fits
  `MaxW - Sp8`, then draws from `X` (left edge).
- `TextMidFit(S, X, Y, BoxH, Size, Color, MaxW, ...)` — shrink-then-centre inside a box
  of height `BoxH`, guaranteeing an 8 px gutter.
- **Right-aligning**: `Text` anchors at the left edge, so right-align by subtracting the
  measured width — `Text(S, Right - Width(S, Size), Y, ...)`. Passing a right-edge X to
  `TextFit` pushes the label *past* the border.

---

## 5. Reusable Structural Components

### 5.1 Sports Card (`Panel`)
- Structured dark charcoal surface (`SurfaceCard`) with subtle 1px framing (`HairlineSoft`).
- Luminous top-edge rim (`HairlineGleam`) giving tactile physical depth.
- Athletic accent notch (4px solid gold or club color) on the left margin.

### 5.2 Primary CTA Button (`Btn(..., Style = 1)`)
- High-voltage Championship Gold solid button with 45-degree corner chamfer.
- Sharp dark athletic typography (`DarkLabel`).
- Instant tactile depression feedback on click/touch.

### 5.3 Broadcast Lower-Third Score Bar (`Score()`)
- Full-bleed bottom bar (`1600px × 56px` at `Y=844`), Cricket-24 style. The top
  of the screen stays empty for the bowler/batter read; only pause remains up top.
- Left: Batting-club badge + two batter rows (`NAME  runs balls`) with amber
  underlines; the striker carries a solid arrow marker.
- Center: DIN Condensed score (`8-0`) baselined with overs (`0.2 OVERS`), with
  `RUN RATE x.xx` beneath (or `NEED x (yb)` in amber during the chase), framed
  by hairline dividers and a crimson slant accent.
- Right: Bowler name + figures (`0-8 (0.2)`) with amber underline, plus six
  this-over ball slots (newest right, unplayed hollow):
    - **Dot**: Hollow outline.
    - **Runs (1-3, 5)**: White disc with dark digit.
    - **Boundary 4**: Turf green disc with white `4`.
    - **Maximum 6**: Amber/gold disc with dark `6`.
    - **Wicket (W)**: Cricket Crimson disc with white `W`.
    - **Extras (Wd/Nb)**: Light-amber disc with dark label.
- Far right: Fielding-club shield. While the bowling `START RUN-UP` button is up,
  the bar stops 16px short of it instead of sliding underneath.

### 5.5 Batting Controls
- **Footwork Pad (Left)**: Translucent disc at `(140, 740)`, radius 52, with compass crosshair
  and a gold thumbstick puck.
- **Shot Zone (Right)**: The whole right half of the screen is the live shot area. It carries
  **no on-screen furniture** — the translucent guide ring and its "HOLD ANYWHERE RIGHT •
  PULL TO AIM • RELEASE TO PLAY" caption were removed. The only cue is the gesture readout
  box, which appears once a gesture is armed.
- **Shot Intent Switcher**: Segmented 3-pill toggle `[ LOFT | GROUND | DEFEND ]` docked at
  `(1240, 620)`, `280 × 44`.
- **Take Guard CTA**: `READY TO FACE  >` at `(1240, 520)`, `280 × 56`.

### 5.6 Bowling Console & Delivery Meter
- **Tactical Deck (Left)**: Segmented picker for Delivery Type (`PACE SEAM`, `IN-SWINGER`), Pitch Length (`GOOD LENGTH`, `YORKER`), and Line (`OUTSIDE OFF`, `OFF STUMP`).
- **3D Pitch Reticle**: Concentric landing rings projected directly onto the turf.
- **Bowling Meter (Run-Up)**: Broadcast meter with Amber build-up zone, Turf Green / Gold Sweet Spot (`PERFECT RELEASE`), and Crimson overstep risk zone.

### 5.7 Timing Reticle & Event Stingers
- **Timing Feedback**: Floating broadcast badge (`★ PERFECT TIMING ★`, `GOOD TIMING`, `EARLY`, `LATE`) with auto-fade.
- **Event Stingers**: Center broadcast banners for `WICKET!` (Crimson), `SIX!` (Championship Gold with distance), and `FOUR!` (Turf Green).
