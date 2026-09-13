# CRICKET 26: Commentary Voice Asset Manifest

**Document Version:** 2.0  
**Broadcast Audio Specification**  
**Roles:**  
- **Commentator A (Lead / Play-by-Play):** Immediate, punchy, energetic delivery of live action. Short, athletic phrases that capture high-velocity moments (boundaries, wickets, near misses).
- **Commentator B (Analyst / Expert):** Tactical, measured, situational insight. Observes field settings, bowler variations, match equations, and offers broadcast conversation handoffs 1.2–2.0s after major events.

---

## Technical Audio Specification

| Parameter | Specification | Notes |
|---|---|---|
| Sample Rate | 24,000 Hz / 44,100 Hz | Broadcast speech standard |
| Channels | 1 (Mono) | Centered in broadcast submix, spatialized reverb send |
| Bit Depth | 16-bit Linear PCM | Uncompressed WAV |
| Peak Normalization | -1.0 dBFS True Peak | Prevents inter-sample clipping |
| Integrated Loudness | -23 LUFS (±1.0 LU) | EBU R128 / ITU-R BS.1770 broadcast standard |
| High-Pass Filter | 80 Hz @ 18 dB/oct | Eliminates sub-rumble, AC, and microphone proximity boom |
| Presence EQ | +2.5 dB @ 3.2 kHz (Q=1.2) | Broadcast commentary microphone presence & intelligibility |
| Lead / Tail Padding | 15 ms head fade, 40 ms tail fade | Eliminates digital clicks on voice boundary |

---

## Master Voice Asset Registry

### 1. Match Start & Venue Intro (`MATCH_START`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Match.Start.001` | Comm A | "Welcome to Eclipse Oval under the lights! Six balls each, everything on the line!" | Excited | 0.65 | `Commentary_Match_Start_001.wav` | Match |
| `Commentary.Match.Start.002` | Comm A | "A packed house tonight. High stakes, maximum tension, and not an inch of margin." | Dramatic | 0.70 | `Commentary_Match_Start_002.wav` | Match |
| `Commentary.Match.Start.003` | Comm A | "The Super Over is here! Settle in, because every single delivery is vital." | Excited | 0.60 | `Commentary_Match_Start_003.wav` | Match |
| `Commentary.Match.Start.004` | Comm A | "Electric atmosphere around the ground. The fielders are ready, the batters are in." | Appreciative | 0.55 | `Commentary_Match_Start_004.wav` | Match |
| `Commentary.Match.Start.005` | Comm B | "In this format, there is zero settling in. You have to commit to your contact from ball one." | Analytical | 0.50 | `Commentary_Match_Start_005.wav` | Match |
| `Commentary.Match.Start.006` | Comm B | "The bowlers will be targeting the wide blockhole. Any miss, and the ball goes into the stands." | Analytical | 0.50 | `Commentary_Match_Start_006.wav` | Match |

---

### 2. Innings Break & Target Equation (`INNINGS_BREAK` / `CHASE_START`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Innings.End.001` | Comm A | "First six balls are in the books. The target is locked in." | Neutral | 0.60 | `Commentary_Innings_End_001.wav` | Match |
| `Commentary.Innings.End.002` | Comm A | "That wraps up the first innings! The fielding side has a real fight on their hands now." | Excited | 0.65 | `Commentary_Innings_End_002.wav` | Match |
| `Commentary.Innings.End.003` | Comm B | "That is a very competitive total on this deck. Discipline at the death is going to decide this." | Analytical | 0.55 | `Commentary_Innings_End_003.wav` | Match |
| `Commentary.Innings.End.004` | Comm B | "Both captains know the math. Clean boundaries or defensive dots—that's all that matters." | Reflective | 0.50 | `Commentary_Innings_End_004.wav` | Match |
| `Commentary.Chase.Start.001` | Comm A | "Here comes the chase! Six deliveries to settle the entire match." | Dramatic | 0.75 | `Commentary_Chase_Start_001.wav` | Match |
| `Commentary.Chase.Start.002` | Comm A | "The chase is underway. Heart rates up all over the stadium." | Tense | 0.70 | `Commentary_Chase_Start_002.wav` | Match |
| `Commentary.Chase.Start.003` | Comm B | "The batting side needs boundaries early. Trying to run hard in six balls is a dangerous game." | Analytical | 0.55 | `Commentary_Chase_Start_003.wav` | Match |
| `Commentary.Chase.Start.004` | Comm B | "Look at the field adjustments. The bowler is backing their defensive boundary sweepers." | Analytical | 0.50 | `Commentary_Chase_Start_004.wav` | Match |

---

### 3. Pre-Delivery & Bowler/Batter Setup (`PRE_BALL` / `PRESSURE`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.PreBall.Generic.001` | Comm A | "Bowler running in from around the wicket." | Neutral | 0.40 | `Commentary_PreBall_Generic_001.wav` | 3 Balls |
| `Commentary.PreBall.Generic.002` | Comm A | "Batter taking guard. Eyes locked on the bowler." | Neutral | 0.40 | `Commentary_PreBall_Generic_002.wav` | 3 Balls |
| `Commentary.PreBall.Generic.003` | Comm A | "Fielders creeping in. Huge delivery coming up." | Tense | 0.65 | `Commentary_PreBall_Generic_003.wav` | 3 Balls |
| `Commentary.PreBall.Generic.004` | Comm A | "A packed ring on the off-side. Bowler begins the stride." | Neutral | 0.45 | `Commentary_PreBall_Generic_004.wav` | 3 Balls |
| `Commentary.PreBall.Bowler.001` | Comm A | "The bowler has hit immaculate lengths so far. Big moment." | Analytical | 0.60 | `Commentary_PreBall_Bowler_001.wav` | 4 Balls |
| `Commentary.PreBall.Batter.001` | Comm A | "The striker looks composed. Backlift nice and high." | Appreciative | 0.55 | `Commentary_PreBall_Batter_001.wav` | 4 Balls |
| `Commentary.PreBall.AfterBoundary.001` | Comm A | "Pressure firmly on the bowling side after that boundary." | Dramatic | 0.70 | `Commentary_PreBall_AfterBoundary_001.wav` | 2 Balls |
| `Commentary.PreBall.AfterWicket.001` | Comm A | "New batter facing their first delivery. Instant trial by fire." | Tense | 0.75 | `Commentary_PreBall_AfterWicket_001.wav` | 2 Balls |
| `Commentary.Pressure.001` | Comm A | "The equation is brutally tight now. No room for error." | Tense | 0.85 | `Commentary_Pressure_001.wav` | 2 Balls |
| `Commentary.Pressure.002` | Comm A | "Every spectator on their feet! Crucial ball here." | Excited | 0.80 | `Commentary_Pressure_002.wav` | 2 Balls |
| `Commentary.Pressure.003` | Comm A | "One mistake here and the match slips away." | Tense | 0.85 | `Commentary_Pressure_003.wav` | 2 Balls |
| `Commentary.Pressure.004` | Comm B | "Notice the field shift. Deep cover has dropped back to the rope." | Analytical | 0.65 | `Commentary_Pressure_004.wav` | 2 Balls |

---

### 4. Dot Balls & Defensive Play (`DOT` / `SHOT_DEFENCE`)
*(Note: 60% of routine dot balls deliberately trigger SILENCE to allow crowd bed and on-field turf sounds to breathe)*
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Dot.001` | Comm A | "Dot ball. Superb discipline from the bowler." | Calm | 0.40 | `Commentary_Dot_001.wav` | 2 Balls |
| `Commentary.Dot.002` | Comm A | "No run! That is absolute gold in a Super Over." | Tense | 0.60 | `Commentary_Dot_002.wav` | 2 Balls |
| `Commentary.Dot.003` | Comm A | "Beaten for pace. The dot ball pressure mounts." | Analytical | 0.55 | `Commentary_Dot_003.wav` | 2 Balls |
| `Commentary.Dot.004` | Comm A | "Right on target. Scoreboard refuses to budge." | Neutral | 0.45 | `Commentary_Dot_004.wav` | 2 Balls |
| `Commentary.Dot.005` | Comm B | "In a six-ball match, a dot is as good as a wicket." | Analytical | 0.60 | `Commentary_Dot_005.wav` | 3 Balls |
| `Commentary.Dot.006` | Comm A | "Tight lines! Batter couldn't get underneath that one." | Appreciative | 0.50 | `Commentary_Dot_006.wav` | 2 Balls |
| `Commentary.Delivery.Defended.001` | Comm A | "Solid forward defense. Respects the delivery." | Calm | 0.35 | `Commentary_Delivery_Defended_001.wav` | 3 Balls |
| `Commentary.Delivery.Beaten.001` | Comm A | "Past the outside edge! Inches away from disaster." | Dramatic | 0.75 | `Commentary_Delivery_Beaten_001.wav` | 2 Balls |
| `Commentary.Delivery.Beaten.002` | Comm A | "Whistled past the stumps! What an absolute seed!" | Shocked | 0.80 | `Commentary_Delivery_Beaten_002.wav` | 2 Balls |

---

### 5. Running Between Wickets (`RUNS` / `EDGE`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Runs.One.001` | Comm A | "Turned into the leg side. They will settle for a single." | Calm | 0.40 | `Commentary_Runs_One_001.wav` | 2 Balls |
| `Commentary.Runs.One.002` | Comm A | "Pushed into the outfield, keeps the scoreboard moving." | Neutral | 0.40 | `Commentary_Runs_One_002.wav` | 2 Balls |
| `Commentary.Runs.One.003` | Comm A | "Just a single. Good recovery by the cover sweeper." | Neutral | 0.45 | `Commentary_Runs_One_003.wav` | 2 Balls |
| `Commentary.Runs.Two.001` | Comm A | "Great running! They push hard and make two comfortable." | Excited | 0.65 | `Commentary_Runs_Two_001.wav` | 2 Balls |
| `Commentary.Runs.Two.002` | Comm A | "Superb hustle between the wickets! Two vital runs." | Appreciative | 0.65 | `Commentary_Runs_Two_002.wav` | 2 Balls |
| `Commentary.Runs.Three.001` | Comm A | "Tremendous effort in the deep saves the boundary! Three runs." | Excited | 0.70 | `Commentary_Runs_Three_001.wav` | 4 Balls |
| `Commentary.Runs.Edge.001` | Comm A | "Thick edge! Flies away past third man!" | Surprised | 0.75 | `Commentary_Runs_Edge_001.wav` | 2 Balls |
| `Commentary.Runs.Edge.002` | Comm A | "Inside edge, off the pads! Heart in mouth for the batter." | Dramatic | 0.70 | `Commentary_Runs_Edge_002.wav` | 2 Balls |

---

### 6. Four Boundaries (`FOUR`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Four.001` | Comm A | "Cracked through the covers! What a magnificent boundary!" | Excited | 0.80 | `Commentary_Four_001.wav` | 2 Balls |
| `Commentary.Four.002` | Comm A | "Pierced the gap with surgical precision. Four runs!" | Appreciative | 0.75 | `Commentary_Four_002.wav` | 2 Balls |
| `Commentary.Four.003` | Comm A | "Pure timing! The ball simply flies across the turf." | Excited | 0.80 | `Commentary_Four_003.wav` | 2 Balls |
| `Commentary.Four.004` | Comm A | "Drilled to the rope! No fielder is cutting that off!" | VeryExcited | 0.85 | `Commentary_Four_004.wav` | 2 Balls |
| `Commentary.Four.005` | Comm A | "Class written all over that stroke. Four more to the total!" | Appreciative | 0.75 | `Commentary_Four_005.wav` | 2 Balls |
| `Commentary.Four.006` | Comm A | "Glorious stroke! Up and over the infield for four!" | Excited | 0.80 | `Commentary_Four_006.wav` | 2 Balls |
| `Commentary.Four.007` | Comm A | "Short, dispatched with utter disdain! That's four!" | VeryExcited | 0.85 | `Commentary_Four_007.wav` | 2 Balls |
| `Commentary.Four.008` | Comm A | "Smashing boundary! The bowler missed the mark and paid the price." | Analytical | 0.75 | `Commentary_Four_008.wav` | 2 Balls |
| `Commentary.Four.009` | Comm A | "Back-to-back boundaries! The batting side is running away with this!" | Celebratory | 0.90 | `Commentary_Four_009.wav` | 4 Balls |
| `Commentary.Four.010` | Comm A | "Exquisite placement! Found the shortest rope on the ground." | Appreciative | 0.75 | `Commentary_Four_010.wav` | 2 Balls |
| `Commentary.Four.011` | Comm A | "Punched off the back foot! That is pure quality." | Appreciative | 0.75 | `Commentary_Four_011.wav` | 2 Balls |
| `Commentary.Four.012` | Comm A | "Muscled through midwicket! Four runs under immense pressure!" | VeryExcited | 0.85 | `Commentary_Four_012.wav` | 2 Balls |

---

### 7. Six Boundaries (`SIX`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Six.001` | Comm A | "High, handsome, and into the crowd! That is a monstrous six!" | VeryExcited | 0.95 | `Commentary_Six_001.wav` | 2 Balls |
| `Commentary.Six.002` | Comm A | "Sent into orbit! What a colossal strike!" | Celebratory | 0.95 | `Commentary_Six_002.wav` | 2 Balls |
| `Commentary.Six.003` | Comm A | "Clears the rope by a country mile! Absolutely sensational!" | VeryExcited | 0.95 | `Commentary_Six_003.wav` | 2 Balls |
| `Commentary.Six.004` | Comm A | "Sweetly timed! That sailed deep into the second tier!" | Shocked | 0.92 | `Commentary_Six_004.wav` | 2 Balls |
| `Commentary.Six.005` | Comm A | "Launched over long-on! That is sheer brute power!" | VeryExcited | 0.92 | `Commentary_Six_005.wav` | 2 Balls |
| `Commentary.Six.006` | Comm A | "Picked up and deposited over square leg! What an exhibition!" | Excited | 0.90 | `Commentary_Six_006.wav` | 2 Balls |
| `Commentary.Six.007` | Comm A | "Right out of the middle! That is cricket at its most destructive!" | Celebratory | 0.95 | `Commentary_Six_007.wav` | 2 Balls |
| `Commentary.Six.008` | Comm A | "Another maximum! They are raining down here tonight!" | Celebratory | 0.95 | `Commentary_Six_008.wav` | 4 Balls |
| `Commentary.Six.009` | Comm A | "Massive pressure, but that is answered with pure gold! Six runs!" | VeryExcited | 0.95 | `Commentary_Six_009.wav` | 2 Balls |
| `Commentary.Six.010` | Comm A | "Up, up, and all the way! The stadium has erupted!" | Celebratory | 0.98 | `Commentary_Six_010.wav` | 2 Balls |
| `Commentary.Six.011` | Comm A | "That went flat and ferocious into the deep! Six runs!" | Excited | 0.90 | `Commentary_Six_011.wav` | 2 Balls |
| `Commentary.Six.012` | Comm A | "Clattered into the sightscreen! Utterly unplayable batting!" | Shocked | 0.95 | `Commentary_Six_012.wav` | 2 Balls |

---

### 8. Wickets & Dismissals (`WICKET` / `BOWLED` / `CAUGHT` / `KEEPER_CATCH`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Wicket.001` | Comm A | "GONE! OUT! A massive breakthrough in the match!" | VeryExcited | 0.90 | `Commentary_Wicket_001.wav` | 2 Balls |
| `Commentary.Wicket.002` | Comm A | "He's out! The fielding side is celebrating ecstatically!" | Celebratory | 0.90 | `Commentary_Wicket_002.wav` | 2 Balls |
| `Commentary.Wicket.003` | Comm A | "WICKET! Right when they needed it most!" | Dramatic | 0.92 | `Commentary_Wicket_003.wav` | 2 Balls |
| `Commentary.Wicket.004` | Comm A | "What a twist! That wicket changes the entire equation!" | Dramatic | 0.90 | `Commentary_Wicket_004.wav` | 2 Balls |
| `Commentary.Wicket.005` | Comm A | "Gone! Tried to force the pace and paid the ultimate price!" | Analytical | 0.85 | `Commentary_Wicket_005.wav` | 2 Balls |
| `Commentary.Wicket.006` | Comm A | "Timber! Middle stump knocked out of the ground!" | Shocked | 0.95 | `Commentary_Wicket_006.wav` | 2 Balls |
| `Commentary.Wicket.007` | Comm A | "A body blow to the batting side! Stunned silence from the dugout." | Tense | 0.85 | `Commentary_Wicket_007.wav` | 2 Balls |
| `Commentary.Wicket.008` | Comm A | "Sensational bowling! Strikes right through the defense!" | VeryExcited | 0.90 | `Commentary_Wicket_008.wav` | 2 Balls |
| `Commentary.Wicket.Bowled.001` | Comm A | "CLEAN BOWLED! Stumps shattered, bails flying everywhere!" | Shocked | 0.98 | `Commentary_Wicket_Bowled_001.wav` | 2 Balls |
| `Commentary.Wicket.Bowled.002` | Comm A | "Through the gate and crashes into the timber! Beautiful delivery!" | Celebratory | 0.95 | `Commentary_Wicket_Bowled_002.wav` | 2 Balls |
| `Commentary.Wicket.Caught.001` | Comm A | "Taken! Safe hands in the deep under high pressure!" | Excited | 0.90 | `Commentary_Wicket_Caught_001.wav` | 2 Balls |
| `Commentary.Wicket.Caught.002` | Comm A | "In the air... and GONE! Fielder made no mistake!" | Dramatic | 0.92 | `Commentary_Wicket_Caught_002.wav` | 2 Balls |
| `Commentary.Wicket.Caught.003` | Comm A | "Skied high... judged to absolute perfection on the rope!" | Excited | 0.90 | `Commentary_Wicket_Caught_003.wav` | 2 Balls |
| `Commentary.Wicket.Keeper.001` | Comm A | "Edged and TAKEN behind! The keeper snaffles it cleanly!" | VeryExcited | 0.92 | `Commentary_Wicket_Keeper_001.wav` | 2 Balls |

---

### 9. Final Ball & Match Finishes (`FINAL_BALL` / `MATCH_WIN` / `MATCH_LOSS` / `MATCH_TIE`)
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.FinalBall.001` | Comm A | "Here it is. One ball. Everything on the line. Can you believe this tension?!" | Dramatic | 0.95 | `Commentary_FinalBall_001.wav` | Match |
| `Commentary.FinalBall.002` | Comm A | "The entire stadium holding its collective breath. Final delivery of the match!" | Tense | 0.95 | `Commentary_FinalBall_002.wav` | Match |
| `Commentary.FinalBall.003` | Comm B | "A boundary wins it; a dot defends it. It all boils down to this single moment." | Analytical | 0.85 | `Commentary_FinalBall_003.wav` | Match |
| `Commentary.FinalBall.004` | Comm A | "Final delivery! Here comes the bowler to decide it all!" | Dramatic | 0.95 | `Commentary_FinalBall_004.wav` | Match |
| `Commentary.Result.Win.001` | Comm A | "THEY HAVE DONE IT! What an unbelievable victory under pressure!" | Celebratory | 1.00 | `Commentary_Result_Win_001.wav` | Match |
| `Commentary.Result.Win.002` | Comm A | "A historic win! The players storm the field in total euphoria!" | Celebratory | 1.00 | `Commentary_Result_Win_002.wav` | Match |
| `Commentary.Result.Win.003` | Comm A | "VICTORY! Sealed with pure composure in the most dramatic circumstances!" | Celebratory | 1.00 | `Commentary_Result_Win_003.wav` | Match |
| `Commentary.Result.Win.004` | Comm B | "Deserved winners tonight. Held their nerve when the game was on the razor's edge." | Reflective | 0.80 | `Commentary_Result_Win_004.wav` | Match |
| `Commentary.Result.Win.005` | Comm B | "Superb tactical execution right through the closing moments. Masterclass." | Analytical | 0.80 | `Commentary_Result_Win_005.wav` | Match |
| `Commentary.Result.Loss.001` | Comm A | "Heartbreak! So close to glory, but fallen just short in the final moments." | Disappointed | 0.85 | `Commentary_Result_Loss_001.wav` | Match |
| `Commentary.Result.Loss.002` | Comm A | "It slips away! Agony for the batting side as the final ball falls safe." | Disappointed | 0.85 | `Commentary_Result_Loss_002.wav` | Match |
| `Commentary.Result.Loss.003` | Comm B | "They gave it everything. Fine margins at this level, but they will rue those dots." | Reflective | 0.70 | `Commentary_Result_Loss_003.wav` | Match |
| `Commentary.Result.Loss.004` | Comm B | "Tough pill to swallow. One big hit away, but the bowling side executed better." | Analytical | 0.70 | `Commentary_Result_Loss_004.wav` | Match |
| `Commentary.Result.Tie.001` | Comm A | "IT'S A TIE! UNBELIEVABLE! Scores level after six dramatic deliveries!" | Shocked | 1.00 | `Commentary_Result_Tie_001.wav` | Match |

---

### 10. Analyst Follow-Ups (`ANALYSIS`)
*(Played by Commentator B 1.2s to 2.0s after Commentator A calls a boundary, wicket, or high-pressure delivery)*
| Clip ID | Speaker | Script | Emotion | Intensity | Filename | Cooldown |
|---|---|---|---|---|---|---|
| `Commentary.Analysis.001` | Comm B | "Watch the replay. The bat face opened at the exact fraction of a second." | Analytical | 0.60 | `Commentary_Analysis_001.wav` | 3 Balls |
| `Commentary.Analysis.002` | Comm B | "That bowler adjusted the seam angle just enough to get that shape." | Analytical | 0.55 | `Commentary_Analysis_002.wav` | 3 Balls |
| `Commentary.Analysis.003` | Comm B | "In this format, taking that risk on the boundary rope is what wins trophies." | Reflective | 0.65 | `Commentary_Analysis_003.wav` | 3 Balls |
| `Commentary.Analysis.004` | Comm B | "Notice how the bowler took pace off. Completely deceived the striker." | Analytical | 0.60 | `Commentary_Analysis_004.wav` | 3 Balls |
| `Commentary.Analysis.005` | Comm B | "Brilliant field placement from the skipper. Paid off instantly." | Appreciative | 0.65 | `Commentary_Analysis_005.wav` | 3 Balls |
| `Commentary.Analysis.006` | Comm B | "You cannot bowl there to this batter. That is hitting the sweet spot every time." | Analytical | 0.65 | `Commentary_Analysis_006.wav` | 3 Balls |
| `Commentary.Analysis.007` | Comm B | "High pressure, but that swing remained pure and balanced throughout." | Appreciative | 0.70 | `Commentary_Analysis_007.wav` | 3 Balls |
| `Commentary.Analysis.008` | Comm B | "The bowler missed by an inch outside off-stump, and that's all it takes." | Analytical | 0.60 | `Commentary_Analysis_008.wav` | 3 Balls |
| `Commentary.Analysis.009` | Comm B | "That is why you keep mid-on up. Created doubt, forced the false stroke." | Analytical | 0.65 | `Commentary_Analysis_009.wav` | 3 Balls |
| `Commentary.Analysis.010` | Comm B | "Pure power. Even with the boundary pushed back, that wasn't staying in the park." | Appreciative | 0.75 | `Commentary_Analysis_010.wav` | 3 Balls |
| `Commentary.Analysis.011` | Comm B | "A captain's dream delivery. Perfect execution under intense scrutiny." | Appreciative | 0.70 | `Commentary_Analysis_011.wav` | 3 Balls |
| `Commentary.Analysis.012` | Comm B | "Game of inches right here. That could have easily gone to hand." | Reflective | 0.65 | `Commentary_Analysis_012.wav` | 3 Balls |

---

## Ingestion & Verification Criteria

1. **Format Validation:** All clips verified 16-bit 24 kHz mono uncompressed PCM.
2. **Audio Levels:** Peak level ≤ -1.0 dBFS, integrated loudness -23 LUFS ±1.0 LU.
3. **No Robot Artifacts:** Human-like cadence, dynamic pauses, athletic passion, and natural inflection.
4. **Silence Guard:** Deliberate silence flag active for routine dot balls (minimum 50% silence threshold).
5. **No Network Dependency:** 100% offline local playable fallback assets guaranteed.
