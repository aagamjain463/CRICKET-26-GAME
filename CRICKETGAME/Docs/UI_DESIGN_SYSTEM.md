# CRICKET 26 — UI Design System

Immediate-mode Canvas HUD (`Source/CRICKETGAME/SuperOver/C26HUD.*`), 1600×900
design space letterboxed via `Scale=Min(W/1600,H/900)`. No UMG. Touch-first;
every CTA ≥ 56 design px tall. Flow state in `AC26MatchGameMode`
(`MenuScreen`, `TossStage`, `PendingConfirm`, `Toast`, `ScreenFade`).

## Tokens
`Ink` #04080d / `Panel` / `CardBg` rgba(.016,.030,.050,.78) / `Paper` off-white /
`Muted` gray-teal / `Teal` primary accent / `Gold` prestige / `Coral` danger.
One accent at a time. Hairline dividers, 5px accent edges, corner-cut primary
CTAs, ghost display type at 4–13% alpha, tracked (letter-spaced) kickers.

## Type (Barlow Condensed SemiBold, single family)
Display 96–160 / H1 52–84 / H2 30–46 / Body 22–25 / Caption 19–21 /
Micro 18–20. Hierarchy from size + case, never extra families.

## Components
`Btn` styles: 0 secondary / 1 primary (shine + corner cut) / 2 danger /
3 selected-ghost; 160 ms press wash via `GameMode.LastAction`.
`NavBtn` rail item (accent tick = selected, never boxes).
`Tag` status chip / `HeroCrest` team emblem / `ScrimLeft/Bottom` readability
gradients (scene always visible) / `Toast` / `Confirm` modal /
`PageHead` (tracked kicker + display title + sub).

## Motion
Screen fade 3.2/s; `Enter(delay)` cubic-out rise 0.38 s; callouts rise/hold/
release (1.35 s big moments); toss coin squash 1.5 s. `ReducedMotion` skips
all of it. No animation holds widget state; rapid nav is re-entrant.

## Flow
Home → Play → Teams → Matchup → Toss → Match; quick paths: PLAY NOW,
QUICK START. Back stack: Teams→Play→Home, Matchup→Teams, Toss→Matchup,
hubs→Home. Destructive match actions behind `PendingConfirm`.

## Match HUD
Broadcast bug (crest/team/score/overs/innings + batter/bowler strip + chase
row), edge-anchored II/LIVE bugs, bottom ball strip, segmented shot control,
plan list + pitch marker + gold-zone meter, animated callouts, corner replay
tag, interval card, result + real-data summary (margin, best, 4s/6s), pause
hub with stacked bus toggles. Subtitles capped at 1020 px, toggleable.

## Future screens
MyTeam/Career/Leagues/Online/Nets/World: full art direction, honest
COMING SOON/PREVIEW tags, tappable cards toast instead of dead-ending.

## QA suite
`-C26Shots` captures 25 beats (10 menu + toss result + pause + full match).
Verified 16:9 + 19.5:9, zero skips, zero errors.
