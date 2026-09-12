# CRICKET 26 — Broadcast Commentary & Audio Architecture

## 1. Executive Summary

The **CRICKET 26** broadcast commentary system upgrades the audio layer from flat, generic AI/TTS voice lines into an emotionally dynamic, context-aware television commentary experience. Inspired by elite international cricket broadcasts (Sky Sports, Star Sports, Channel 7), the architecture responds to match situation, required run rate, wickets in hand, and ball-by-ball momentum.

```
+---------------------------------------------------------------------------------------+
|                                    CRICKET 26 MATCH                                   |
|               (Bowler Delivery -> Bat Contact -> Trajectory -> Result)                |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|                                  FC26CommentaryEvent                                  |
|  DeliveryId | EventType | Striker/Bowler | Runs | Target | RRR | Pressure | Ball/Over |
+-------------------------------------------+-------------------------------------------+
                                            |
                                            v
+---------------------------------------------------------------------------------------+
|                              UC26CommentaryDirector                                   |
|  - Pressure Scoring [0.0 - 1.0] (RRR, Balls Rem., Wickets, Dot/Boundary streaks)      |
|  - Emotion & Intensity Model (Neutral -> Celebratory, 0.0 - 1.0)                      |
|  - Natural Broadcast Pacing & Ambience Breathing (Suppresses ~60% routine balls)      |
|  - Line Selection with Anti-Repetition Ring Buffer (12 deliveries)                    |
|  - Two-Commentator Team: Lead Play-by-Play vs. Tactical Color Analyst                 |
+---------------------+-------------------------------------+---------------------------+
                      |                                     |
                      v (Lead Call)                         v (Analyst Follow-Up: 1.5s)
+-------------------------------------------+  +----------------------------------------+
|           Dynamic Audio Routing           |  |          Schedule Follow-Up            |
|  1. Disk Cache Check (Saved/Cache/*.wav)  |  |  "Bowled too full under extreme        |
|  2. Async Non-blocking ElevenLabs TTS     |  |   pressure... that missed the blockhole|
|  3. Offline Pre-generated WAV Fallback    |  |   by inches."                          |
+---------------------+---------------------+  +----------------------------------------+
                      |
                      v
+---------------------------------------------------------------------------------------+
|                             UC26Audio Mixing & Ducking                                |
|  - Crowd roar ducks by ~4.5 dB during commentary voice playback                       |
|  - Restores full stadium acoustics & atmospheric crowd reverberation when silent      |
|  - Real-time broadcast HUD subtitle synchronization                                   |
+---------------------------------------------------------------------------------------+
```

---

## 2. Match Context & Pressure Modeling

Commentary lines are never chosen solely from event type. Instead, every delivery evaluates an analytical **Match Pressure Score** ($P \in [0.0, 1.0]$):

$$P = 0.25 \cdot P_{\text{balls}} + 0.35 \cdot P_{\text{chase}} + 0.20 \cdot P_{\text{wickets}} + 0.20 \cdot P_{\text{streak}}$$

### Factors Evaluated:
1. **Innings Phase & Balls Remaining ($P_{\text{balls}}$)**: Early deliveries (Balls 1–2) start at low pressure ($0.20$); the final over/ball naturally spikes urgency ($1.0$).
2. **Chase Urgency & Required Run Rate ($P_{\text{chase}}$)**:
   - Evaluated during 2nd innings (`Target > 0`).
   - $\text{RRR} = \frac{\text{RunsRequired} \times 6}{\text{BallsRemaining}}$.
   - $\text{RRR} \ge 18 \implies P_{\text{chase}} = 1.0$.
   - $\text{RRR} \le 6 \implies P_{\text{chase}} \le 0.2$.
3. **Wickets Lost / Pressure on Batting Side ($P_{\text{wickets}}$)**:
   - In a Super Over (where 2 wickets end the innings), 1 wicket down brings pressure to $0.85$.
4. **Delivery Streaks ($P_{\text{streak}}$)**:
   - Consecutive dot balls increment bowling pressure on the batter.
   - Consecutive boundaries build attacking momentum.

### Contextual Emotion & Intensity Matrix:

| Event Type | Match Situation | Selected Emotion | Intensity ($0.0 - 1.0$) | Voice Delivery Style |
| :--- | :--- | :--- | :--- | :--- |
| **Dot Ball** | Over 1, Ball 1 (Routine) | `Neutral` | $0.20 - 0.30$ | Calm, steady rhythm |
| **Dot Ball** | 6 needed off 2 balls | `Tense` | $0.85 - 0.95$ | Gripping, breathless |
| **Four** | Mid-innings normal chase | `Appreciative` | $0.55 - 0.65$ | Smooth admiration of timing |
| **Six** | Early Super Over ($20/0$) | `Excited` | $0.70 - 0.80$ | High energy, boundary call |
| **Six** | Final ball, 6 to win | `Celebratory` | $0.98 - 1.00$ | Peak climax, roaring call |
| **Bowled** | Ball clips off-stump | `Shocked` | $0.85 - 0.95$ | Stunned, explosive play-by-play |
| **Caught** | High skier taken on rope | `Dramatic` | $0.80 - 0.90$ | Rising pitch, dramatic climax |

---

## 3. Two-Commentator Team Dynamic

Broadcast authenticity requires contrast between booth personalities. CRICKET 26 features two distinct roles:

### 1. Lead Commentator (`ECommentatorRole::Lead`)
- **Profile**: Energetic, punchy, play-by-play anchor (inspired by Ian Bishop / Ravi Shastri / Mark Nicholas).
- **Function**: Fires immediately at the moment of contact/wicket resolution.
- **Expressive Style**: Uses exclamation, short sharp bursts, and immediate match-state announcements.

### 2. Expert Color Analyst (`ECommentatorRole::Analyst`)
- **Profile**: Composed, technical, observational tactician (inspired by Nasser Hussain / Michael Atherton / Ricky Ponting).
- **Function**: Automatically scheduled **$1.2 - 1.8$ seconds** after a major Lead call.
- **Lines**: Analyzes bowling errors (e.g. *"missed the yorker length"*, *"fed the batter right in the slot"*) and batter mechanics (*"clean extension through the line"*).

---

## 4. Intelligent Audio Pacing & Stadium Breathing

A major drawback of generic AI commentary is constant, robotic chatter. Real cricket broadcasts allow stadium atmosphere to breathe:

- **Routine Ball Suppression**: ~60% of routine dot balls and single runs without high match pressure are intentionally suppressed.
- **Critical Moment Exemption**: Boundaries (4s, 6s), wickets (Bowled, Caught, LBW, Run Out), final-over balls, and match-winning deliveries are **never** suppressed.
- **Anti-Repetition Buffer**: A circular history buffer tracks the last 12 spoken line IDs, ensuring phrases are not repeated within the same match.
- **Crowd Ducking Bridge**: When `UC26CommentaryDirector` dispatches audio through `UC26Audio::PlayCommentarySound`, the crowd ambiance audio component automatically ducks by **$-4.5\text{ dB}$**, restoring dynamically as speech finishes.

---

## 5. ElevenLabs Voice API & Expression Tag Integration

CRICKET 26 supports state-of-the-art neural speech synthesis via the ElevenLabs Text-to-Speech API (`eleven_multilingual_v2` / `eleven_turbo_v2_5`).

### Expressive Tagging
Commentary line definitions include prompt tags that guide the neural voice actor model:
```
[shouting] INCREDIBLE! HE HAS LAUNCHED THAT ALL THE WAY INTO THE SECOND TIER!
[gasp] HE IS CLEAN BOWLED! Absolute perfection from the paceman!
[whispering] You can feel the tension in the stadium right now.
[dramatic pause] Six needed off the final delivery. This is for all the marbles.
```

### Synthesis Architecture:
1. **Local Disk Cache**:
   Synthesized audio is saved as standard RIFF PCM `.wav` files in:
   `CRICKETGAME/Saved/CommentaryCache/<MD5_HASH>.wav`
   Subsequent triggers of identical lines load instantly from disk with zero network latency.
2. **Non-Blocking Async Dispatch**:
   HTTP requests run asynchronously on Unreal's HTTP thread pool. **The game thread, physics simulation, animation system, and batting/bowling controls never block or freeze.**
3. **Match Budget Control**:
   Configurable limit (`MaxDynamicRequestsPerMatch`, default 15) caps dynamic network calls per session to manage API consumption.
4. **Pre-Generated Offline Fallback**:
   If offline, without an API key, or when the match budget is reached, the system automatically falls back to bundled pre-generated `.wav` audio assets and HUD broadcast subtitles.

---

## 6. Secure Configuration (No Committed API Keys)

To keep private API credentials safe, the ElevenLabs API key is **never hardcoded in committed C++ source files**.

The Director searches for the API key in the following order of precedence:

1. **Environment Variable** (Recommended for local dev):
   ```bash
   export ELEVENLABS_API_KEY="sk_your_elevenlabs_api_key_here"
   ```
2. **Command-Line Switch**:
   ```bash
   UnrealEditor CRICKETGAME.uproject -ElevenLabsApiKey="sk_..."
   ```
3. **Untracked Local INI File**:
   Create or edit `Saved/Config/ElevenLabs.ini` (which is git-ignored):
   ```ini
   [ElevenLabs]
   ApiKey=sk_your_elevenlabs_api_key_here
   LeadVoiceId=JBFqnCBsd6RMkjVDRZzb
   AnalystVoiceId=N2lVS1w4EtoT3dr4eOWO
   ```

---

## 7. Diagnostic Testing & Console Verification

Developers can test all key commentary scenarios directly in-game or via console commands without manually simulating dozens of overs.

### In-Game Console Commands (`~` key or `c <scenario>`):

| Command | Scenario Simulated | Expected Output & Emotional Tone |
| :--- | :--- | :--- |
| `c routine_dot` | Early dot ball in over 1 | `Neutral` / `Tense` (suppressed or quiet observational call) |
| `c four` | Elegant boundary | Lead: `Appreciative`, praising timing |
| `c normal_six` | Mid-innings maximum | Lead: `Excited` boundary call + Analyst follow-up |
| `c pressure_six` | Six under high RRR | Lead: `VeryExcited`, high intensity |
| `c bowled` | Clean bowled dismissal | Lead: `Shocked` / `Dramatic` shout + Analyst follow-up |
| `c caught` | Caught in the deep | Lead: `Dramatic` call on fielder's catch |
| `c final_ball` | 6 runs needed off 1 ball | Lead: `Dramatic` build-up on the bowler running in |
| `c win_six` | Match-winning six on last ball | Lead: `Celebratory` (Intensity 1.0), peak broadcast climax |
| `c test_all` | Sequence of all 7 scenarios | Comprehensive diagnostic run in game log |

### Automated Engine Test Suite:
Run headless unit verification from terminal:
```bash
/Users/Shared/Epic\ Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor \
  /Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/CRICKETGAME.uproject \
  -ExecCmds="Automation RunTests Cricket26.Commentary.Director; Quit" \
  -unattended -nopause -testexit="Automation Test Queue Empty" -log -stdout
```
Output:
```
Found 6 automation tests based on 'Cricket26'
  Cricket26.Commentary.Director: Result={Success}
  Cricket26.Production.DeliveryPlan: Result={Success}
  Cricket26.Production.FrameRateAndShots: Result={Success}
  Cricket26.Rules.SuperOver: Result={Success}
  Cricket26.Simulation.GoldenDelivery: Result={Success}
  Cricket26.Simulation.Trajectories: Result={Success}
**** TEST COMPLETE. EXIT CODE: 0 ****
```
