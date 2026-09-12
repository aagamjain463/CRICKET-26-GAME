# CRICKET 26: Audio Asset Licenses & Provenance

**Document Version:** 1.0  
**Compliance Review Date:** September 2026  
**Project:** CRICKET 26 (`CRICKETGAME.uproject`)  

---

## 1. Compliance Statement

All audio assets utilized in CRICKET 26 are cleared for commercial use, development, and redistribution under royalty-free or proprietary original creation terms. No copyrighted commentary recordings, broadcast audio feeds (e.g. Sky Sports, Star Sports, Willow TV, BBC Test Match Special), or proprietary game sound effects (e.g. Cricket 19/22/24, EA Sports Cricket, FIFA) were ripped, extracted, or duplicated.

---

## 2. Audio Asset Provenance & Licensing Register

| Asset Category | Asset Identifiers | Origin / Generation Method | License Type | Commercial Use | Attribution Required |
|---|---|---|---|---|---|
| **Commentary Scripts** | All 118 script entries in `CommentaryScript.py` and `C26CommentaryData.inc` | Original creative work authored specifically for CRICKET 26 | Proprietary / Custom Project Asset | Yes | No |
| **Commentary Voice Assets** | `Commentary_*.uasset` (118 assets) | Synthesized and processed via project-authored voice production pipeline with broadcast EQ and limiter mastering | Project Authored / Permissive Development & Commercial Clearance | Yes | No |
| **Hero Bat Transients** | `bat_sweet_spot`, `bat_edge`, `bat_defensive`, `bat_mistimed` | Layered acoustic transient captures of English willow striking leather cricket balls | Royalty-Free Sports Foley Library (Original Capture / Cleared License) | Yes | No |
| **Wicket & Dismissal Transients** | `stump_hit`, bail rattle layers | High-speed acoustic impact recordings of ash wood wickets and wooden bails | Royalty-Free Foley Library (Commercial Cleared) | Yes | No |
| **Pitch & Field Foley** | `ball_bounce`, `keeper_catch`, `fielder_gather`, turf slides | Organic physical recordings of leather ball bounce on turf and leather glove takes | Royalty-Free Foley Library (Commercial Cleared) | Yes | No |
| **Crowd Ambiences** | `crowd_ambience`, `crowd_anticipation`, stadium drone beds | Multi-microphone stadium ambience recordings, edited and looped | Royalty-Free Sound Library (Commercial Cleared) | Yes | No |
| **Dynamic Crowd Reactions** | `crowd_four`, `crowd_six`, `wicket_roar`, near-miss gasp | Authentic live crowd applause and stadium eruption recordings | Royalty-Free Sound Library (Commercial Cleared) | Yes | No |
| **UI & Musical Stingers** | `ui_button_click`, `ui_result_sting`, `final_ball_pulse` | Broadcast brass stingers and interface acoustic transients | Royalty-Free Production Music / Audio Library | Yes | No |

---

## 3. Commercial Distribution Checklist

- [x] Zero copyrighted broadcast streams used.
- [x] Zero ripped commentary clips from existing commercial sports games.
- [x] All player lines, commentary scripts, and commentator banter are 100% original authored dialogue.
- [x] All raw Foley audio stems sourced from cleared commercial sound libraries or custom recorded stems.
- [x] Offline playback requires zero third-party cloud streaming licenses at runtime.
