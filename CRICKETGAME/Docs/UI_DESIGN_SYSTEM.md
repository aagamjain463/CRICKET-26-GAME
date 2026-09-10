# CRICKET 26 — AAA Sports Broadcast Design System (v3 Rebuild)

Immediate-mode Canvas HUD (`Source/CRICKETGAME/SuperOver/C26HUD.*`), 1600×900 virtual design space letterboxed via `Scale = Min(W / 1600, H / 900)`. Touch-first, sports broadcast interaction. Flow state in `AC26MatchGameMode` (`MenuScreen`, `TossStage`, `PendingConfirm`, `Toast`, `ScreenFade`).

## Design Philosophy: Framing the Sport
1. **Cricket is the visual hero**: The 3D stadium, floodlights, pitch, and player are front and center. UI frames the sport; UI never buries the sport.
2. **Zero AI-slop anti-patterns**:
   - No grids of identical rounded cards.
   - No generic cyan-purple gradients or blur-behind-everything.
   - No neon glowing outlines or floating transparent boxes.
   - No colliding background ghost numerals (watermark depth strictly separated from foreground text).
   - Strict measure-aware layout: zero panel overflow beyond the 1600×900 boundary.
3. **Single cohesive athletic palette**: Deep Midnight Carbon, Crisp Off-White, Muted Technical Slate, and ONE signature accent: Electric Stadium Cyan. (Gold reserved strictly for targets/trophies/championship; Coral for wickets/danger/alerts).

## Tokens
- `Void`: `#020406` (`.008, .014, .022, .98f`) — Deepest midnight carbon
- `PanelBg`: `#04060A` (`.014, .024, .038, .92f`) — Solid athletic broadcast slate
- `CardSurface`: `#05080E` (`.018, .030, .046, .84f`) — Secondary surface
- `Paper`: `#F0F5F8` (`.94, .96, .98, 1.f`) — Crisp athletic off-white
- `PaperDim`: `#B8CCD6` (`.72, .80, .86, 1.f`) — High-legibility secondary text
- `Muted`: `#668499` (`.40, .52, .60, 1.f`) — Technical metadata slate
- `Hairline`: `#294257` (`.16, .26, .34, .65f`) — Razor architectural hairline
- `Cyan`: `#00E5FF` (`.00, .90, 1.00, 1.f`) — Signature CRICKET 26 Electric Cyan
- `Gold`: `#FFB82E` (`1.00, .72, .18, 1.f`) — Championship gold (targets/victory)
- `Coral`: `#FF3D2E` (`1.00, .24, .18, 1.f`) — Danger / wickets / Melbourne accent

## Typography (Barlow Condensed SemiBold)
- `Display XXL` (96–120pt): Hero mode titles, score callouts, victory headlines.
- `Display XL` (60–72pt): Page headers, team codes.
- `H1` (38–44pt): Section headers, team names.
- `H2` (26–32pt): Card titles, sub-headings.
- `Body` (21–25pt): Explanatory text, stats, rules.
- `Label` (18–20pt): Track-spaced kickers (`M A T C H D A Y   S P E C I A L`).

## Components
- `Btn`:
  - `Style 1 (Primary)`: Solid Electric Cyan block with black/Void text, sharp corner cut, instant tactile flash on touch. Unmistakable visual hierarchy.
  - `Style 0 (Secondary)`: Dark slate bar with hairline border and Paper text.
  - `Style 2 (Danger)`: Deep coral bar for destructive actions (Restart / Exit).
  - `Style 3 (Selected)`: Deep teal bar with solid Cyan left indicator strip and Paper text.
- `NavBtn`: Rail item with razor 4px Cyan left indicator when active, clean Muted text when idle (no rectangular boxes).
- `Tag`: Razor athletic pill/chip with accent edge.
- `PageHead`: Tracked kicker with Cyan notch + display title + concise subtitle + hairline rule.

## Screen Architectures
- **Home (Screen 0)**: Asymmetrical editorial composition. Left 32% holds the navigation rail and brand anchor. Center-left holds the Super Over hero block with title, format specs, matchup preview, and the dominant `PLAY NOW >` CTA. Right 40% breathes freely for the 3D stadium, floodlights, and player model. Bottom modules end safely at Y=684 (no clipping).
- **Play (Screen 1)**: Dominant featured Super Over card (65% width) + stacked secondary mode preview column with honest coming soon badges.
- **Teams (Screen 2)**: Broadcast head-to-head club selector. Distinct team code display (`MUM` vs `MEL`), club crests, tactical identity, and clear selection CTAs. Zero ghost collisions.
- **Matchup (Screen 3)**: Broadcast pre-match card with head-to-head presentation, condition summary bar, and `TO THE TOSS >` CTA.
- **Toss (Screen 4)**: 3-stage flow (Ready, Spinning, Result). Generous vertical pitch. Clear winner announcement and wide choice cards (`BAT FIRST` vs `BOWL FIRST`).
- **Match HUD**: Ultra-compact broadcast score bug (520×52px) with top accent bar, big bold score, balls remaining, and docked lower-third batter/bowler strip. Target strip docks cleanly only during chase. Out of the way of the bowler approach and batsman sightlines.
- **In-Play & Callouts**: Layered vertical zones (Callout at Y=560, Runs at Y=700, Subtitle at Y=742, Ball strip at Y=830) with zero overlap.
- **Match Result (Screen 25)**: Full-screen sports celebration hero. Bold headline, victory margin, both innings scorecards with best batter and boundary counts, and instant replay/rematch CTAs.
- **Pause Menu**: Clean vertical sports broadcast drawer over darkened stadium.

## Visual Acceptance & QA Pipeline
- Script: `Tools/Capture.sh [label]`
- Runs engine with `-game -C26Shots -ResX=1600 -ResY=900 -windowed`.
- Captures all 25 deterministic beats (`01_home` to `25_result`).
- Verified 100% clean passes with zero skipped beats.
