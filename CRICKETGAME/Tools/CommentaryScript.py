"""CRICKET 26 development commentary library.

Every line is ORIGINAL, written for this project. No broadcast, game, or
published commentary is copied, transcribed, or imitated here.

Fields per entry:
  id          stable ID, e.g. Commentary.Boundary.Four.003 (never renamed)
  category    selection bucket used by the CommentaryManager
  voice       A = play-by-play (Daniel, en_GB) | B = analyst (Samantha, en_US)
  text        spoken line (also the subtitle source of truth)
  priority    higher interrupts lower (match result 100 > wicket 90 > six 80 ...)
  cooldown    minimum balls before this exact line may repeat
  delay       seconds after the triggering event before speech starts
  weight      relative selection weight inside its category
  follow      true = analyst follow-up, only queued after a primary call

Categories cover: match start, innings/target, pre-ball, pressure/final,
delivery analysis, dot, runs, four, six, wicket, analyst follow-ups, result.
Total: 118 lines.
"""
LIBRARY = [
    # ---- MATCH START (6, priority 60, pre-match) ----
    dict(id="Commentary.Match.Start.001", category="MATCH_START", voice="A", priority=60, cooldown=99, delay=0.4, weight=10, text="Welcome to Eclipse Oval. One over. Everything on the line."),
    dict(id="Commentary.Match.Start.002", category="MATCH_START", voice="A", priority=60, cooldown=99, delay=0.4, weight=10, text="Six balls a side under lights. This should be a cracker."),
    dict(id="Commentary.Match.Start.003", category="MATCH_START", voice="A", priority=60, cooldown=99, delay=0.4, weight=10, text="The Super Over is here. No second chances from here on."),
    dict(id="Commentary.Match.Start.004", category="MATCH_START", voice="A", priority=60, cooldown=99, delay=0.4, weight=10, text="A packed house tonight, and the noise is building already."),
    dict(id="Commentary.Match.Start.005", category="MATCH_START", voice="B", priority=30, cooldown=99, delay=1.2, weight=6, text="The key is simple. Strike clean from ball one, there is no settling in."),
    dict(id="Commentary.Match.Start.006", category="MATCH_START", voice="B", priority=30, cooldown=99, delay=1.2, weight=6, text="Bowlers have to back their best ball. There is nowhere to hide in six deliveries."),

    # ---- INNINGS / TARGET (8) ----
    dict(id="Commentary.Innings.End.001", category="INNINGS_BREAK", voice="A", priority=70, cooldown=99, delay=0.6, weight=10, text="The first six balls are done. The target is set."),
    dict(id="Commentary.Innings.End.002", category="INNINGS_BREAK", voice="A", priority=70, cooldown=99, delay=0.6, weight=10, text="That is the end of the first innings. Time for the chase."),
    dict(id="Commentary.Chase.Start.001", category="CHASE_START", voice="A", priority=70, cooldown=99, delay=0.5, weight=10, text="The equation is clear. Six balls to chase it down."),
    dict(id="Commentary.Chase.Start.002", category="CHASE_START", voice="A", priority=70, cooldown=99, delay=0.5, weight=10, text="Here comes the chase. Every ball will swing this match."),
    dict(id="Commentary.Chase.Start.003", category="CHASE_START", voice="B", priority=35, cooldown=99, delay=1.4, weight=6, text="Boundaries win chases like this. Ones and twos will not be enough alone."),
    dict(id="Commentary.Chase.Start.004", category="CHASE_START", voice="B", priority=35, cooldown=99, delay=1.4, weight=6, text="The bowling side needs dots early. Pressure does strange things in a chase."),
    dict(id="Commentary.Innings.End.003", category="INNINGS_BREAK", voice="B", priority=35, cooldown=99, delay=1.4, weight=6, text="A defendable total if they bowl with discipline at the death."),
    dict(id="Commentary.Innings.End.004", category="INNINGS_BREAK", voice="B", priority=35, cooldown=99, delay=1.4, weight=6, text="Both sides know exactly what is needed. No guesswork now."),

    # ---- PRE-BALL (8) ----
    dict(id="Commentary.PreBall.Generic.001", category="PRE_BALL", voice="A", priority=20, cooldown=4, delay=0.0, weight=10, text="Here comes the bowler once again."),
    dict(id="Commentary.PreBall.Generic.002", category="PRE_BALL", voice="A", priority=20, cooldown=4, delay=0.0, weight=10, text="The batter takes guard. The field comes in."),
    dict(id="Commentary.PreBall.Generic.003", category="PRE_BALL", voice="A", priority=20, cooldown=4, delay=0.0, weight=10, text="A quiet word at the top of the mark. Away we go."),
    dict(id="Commentary.PreBall.Generic.004", category="PRE_BALL", voice="A", priority=20, cooldown=4, delay=0.0, weight=8, text="Huge moment in this over. The crowd senses it too."),
    dict(id="Commentary.PreBall.Bowler.001", category="BOWLER_BUILDUP", voice="A", priority=22, cooldown=5, delay=0.0, weight=8, text="The bowler has been excellent tonight. Can he hold his nerve?"),
    dict(id="Commentary.PreBall.Batter.001", category="BATTER_BUILDUP", voice="A", priority=22, cooldown=5, delay=0.0, weight=8, text="The striker is seeing it well. This could travel."),
    dict(id="Commentary.PreBall.AfterBoundary.001", category="AFTER_BOUNDARY", voice="A", priority=24, cooldown=3, delay=0.0, weight=9, text="Momentum with the batting side after that boundary."),
    dict(id="Commentary.PreBall.AfterWicket.001", category="AFTER_WICKET", voice="A", priority=24, cooldown=3, delay=0.0, weight=9, text="A wicket changes everything. The new batter is under instant pressure."),

    # ---- PRESSURE / FINAL BALL (8) ----
    dict(id="Commentary.Pressure.001", category="PRESSURE", voice="A", priority=55, cooldown=2, delay=0.0, weight=10, text="The equation is tightening now. Every ball matters."),
    dict(id="Commentary.Pressure.002", category="PRESSURE", voice="A", priority=55, cooldown=2, delay=0.0, weight=10, text="No room for hesitation. The batters have to commit."),
    dict(id="Commentary.Pressure.003", category="PRESSURE", voice="A", priority=55, cooldown=2, delay=0.0, weight=9, text="The bowler knows one loose ball ends the contest."),
    dict(id="Commentary.Pressure.004", category="PRESSURE", voice="B", priority=38, cooldown=3, delay=1.0, weight=7, text="Watch the field. They are guarding one side of the ground now."),
    dict(id="Commentary.FinalBall.001", category="FINAL_BALL", voice="A", priority=85, cooldown=99, delay=0.0, weight=10, text="One ball left. This is what the whole night has built toward."),
    dict(id="Commentary.FinalBall.002", category="FINAL_BALL", voice="A", priority=85, cooldown=99, delay=0.0, weight=10, text="The final delivery. Hold your breath around the ground."),
    dict(id="Commentary.FinalBall.003", category="FINAL_BALL", voice="B", priority=40, cooldown=99, delay=1.2, weight=7, text="A boundary wins it. A dot could defend it. Nothing in between matters."),
    dict(id="Commentary.FinalBall.004", category="FINAL_BALL", voice="A", priority=85, cooldown=99, delay=0.0, weight=10, text="Last ball of the Super Over. Heroes are made right here."),

    # ---- DELIVERY ANALYSIS (14) ----
    dict(id="Commentary.Delivery.Fast.001", category="DELIVERY", voice="A", priority=45, cooldown=4, delay=0.55, weight=8, text="Sharp pace, right on the money."),
    dict(id="Commentary.Delivery.Slower.001", category="DELIVERY", voice="A", priority=45, cooldown=4, delay=0.55, weight=8, text="The slower ball. Cleverly disguised."),
    dict(id="Commentary.Delivery.Yorker.001", category="DELIVERY", voice="A", priority=50, cooldown=4, delay=0.55, weight=9, text="Right in the blockhole. Superb execution under pressure."),
    dict(id="Commentary.Delivery.Short.001", category="DELIVERY", voice="A", priority=45, cooldown=4, delay=0.55, weight=8, text="Short and into the body. The batter had to hurry."),
    dict(id="Commentary.Delivery.GoodLength.001", category="DELIVERY", voice="A", priority=42, cooldown=4, delay=0.55, weight=8, text="Lovely length. Asking all the questions."),
    dict(id="Commentary.Delivery.Excellent.001", category="DELIVERY", voice="A", priority=52, cooldown=4, delay=0.55, weight=9, text="That is as good as it gets in this format."),
    dict(id="Commentary.Delivery.Beaten.001", category="DELIVERY", voice="A", priority=52, cooldown=4, delay=0.55, weight=9, text="Beaten! Past the outside edge by a whisker."),
    dict(id="Commentary.Delivery.Beaten.002", category="DELIVERY", voice="A", priority=52, cooldown=4, delay=0.55, weight=9, text="No contact. The bowler wins that little battle."),
    dict(id="Commentary.Delivery.Outswing.001", category="DELIVERY", voice="A", priority=48, cooldown=4, delay=0.55, weight=8, text="A hint of movement away. Just enough to cause doubt."),
    dict(id="Commentary.Delivery.Inswing.001", category="DELIVERY", voice="A", priority=48, cooldown=4, delay=0.55, weight=8, text="Nipping back in. The stumps were in play there."),
    dict(id="Commentary.Delivery.Defended.001", category="SHOT_DEFENCE", voice="A", priority=40, cooldown=4, delay=0.55, weight=8, text="Watchfully kept out. Good respect for a good ball."),
    dict(id="Commentary.Delivery.Driven.001", category="SHOT_DRIVE", voice="A", priority=44, cooldown=4, delay=0.55, weight=8, text="Creamed through the covers. Beautiful shape on that stroke."),
    dict(id="Commentary.Delivery.Lofted.001", category="SHOT_LOFT", voice="A", priority=46, cooldown=4, delay=0.55, weight=8, text="He went aerial. High risk, high reward in this format."),
    dict(id="Commentary.Delivery.Mistimed.001", category="SHOT_MISTIMED", voice="A", priority=46, cooldown=4, delay=0.55, weight=8, text="Not off the middle. That could have gone anywhere."),

    # ---- DOT BALL (6) ----
    dict(id="Commentary.Dot.001", category="DOT", voice="A", priority=30, cooldown=2, delay=0.65, weight=10, text="Excellent control. No run there."),
    dict(id="Commentary.Dot.002", category="DOT", voice="A", priority=30, cooldown=2, delay=0.65, weight=10, text="A valuable dot ball under pressure."),
    dict(id="Commentary.Dot.003", category="DOT", voice="A", priority=30, cooldown=2, delay=0.65, weight=10, text="The bowler wins that contest. Scoreboard stays still."),
    dict(id="Commentary.Dot.004", category="DOT", voice="A", priority=30, cooldown=2, delay=0.65, weight=9, text="Tight lines. The batter could not free the arms."),
    dict(id="Commentary.Dot.005", category="DOT", voice="B", priority=28, cooldown=4, delay=1.3, weight=6, text="That dot is worth its weight in gold at this stage.", follow=True),
    dict(id="Commentary.Dot.006", category="DOT", voice="A", priority=30, cooldown=2, delay=0.65, weight=9, text="Pressure builds with every scoreless delivery."),

    # ---- RUNS (8) ----
    dict(id="Commentary.Runs.One.001", category="RUNS", voice="A", priority=40, cooldown=2, delay=0.6, weight=10, text="Worked away for a single. Smart cricket."),
    dict(id="Commentary.Runs.One.002", category="RUNS", voice="A", priority=40, cooldown=2, delay=0.6, weight=10, text="One run. They keep the scoreboard ticking."),
    dict(id="Commentary.Runs.One.003", category="RUNS", voice="A", priority=40, cooldown=2, delay=0.6, weight=9, text="Pushed into the gap. A comfortable single."),
    dict(id="Commentary.Runs.Two.001", category="RUNS", voice="A", priority=42, cooldown=2, delay=0.6, weight=9, text="Two runs. Superb urgency between the wickets."),
    dict(id="Commentary.Runs.Two.002", category="RUNS", voice="A", priority=42, cooldown=2, delay=0.6, weight=9, text="They come back for two. The chase stays alive."),
    dict(id="Commentary.Runs.Three.001", category="RUNS", voice="A", priority=44, cooldown=3, delay=0.6, weight=8, text="Three runs! Tremendous commitment in the deep."),
    dict(id="Commentary.Runs.Edge.001", category="EDGE", voice="A", priority=60, cooldown=3, delay=0.5, weight=9, text="Thick edge, and that could have gone anywhere."),
    dict(id="Commentary.Runs.Edge.002", category="EDGE", voice="A", priority=60, cooldown=3, delay=0.5, weight=9, text="Not where the batter intended. A lucky escape."),

    # ---- FOUR (12) ----
    dict(id="Commentary.Four.001", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=10, text="That is beautifully placed. The fielder had no chance."),
    dict(id="Commentary.Four.002", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=10, text="Timed cleanly through the gap. Four runs."),
    dict(id="Commentary.Four.003", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=10, text="He has found the boundary when his side needed it."),
    dict(id="Commentary.Four.004", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=10, text="Crashed away to the rope. What a strike."),
    dict(id="Commentary.Four.005", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=9, text="Pierced the infield perfectly. No need to run for that."),
    dict(id="Commentary.Four.006", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=9, text="Glorious timing. The ball races to the fence."),
    dict(id="Commentary.Four.007", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=9, text="Short and punished. That was asking for trouble."),
    dict(id="Commentary.Four.008", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=9, text="Overpitched, and clinically dispatched to the boundary."),
    dict(id="Commentary.Four.009", category="FOUR", voice="A", priority=72, cooldown=2, delay=0.45, weight=9, text="Back to back boundaries! The bowler is under real pressure now."),
    dict(id="Commentary.Four.010", category="FOUR", voice="A", priority=70, cooldown=3, delay=0.45, weight=8, text="Edged, and it flies past the keeper! Fortune favors the brave."),
    dict(id="Commentary.Four.011", category="FOUR", voice="B", priority=42, cooldown=5, delay=1.5, weight=6, text="The placement was perfect. He barely seemed to hit it.", follow=True),
    dict(id="Commentary.Four.012", category="FOUR", voice="B", priority=42, cooldown=5, delay=1.5, weight=6, text="That is smart batting. Using the pace instead of forcing it.", follow=True),

    # ---- SIX (12) ----
    dict(id="Commentary.Six.001", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=10, text="That is launched high and long. All the way!"),
    dict(id="Commentary.Six.002", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=10, text="Clean strike. That has gone all the way."),
    dict(id="Commentary.Six.003", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=10, text="He picked that up superbly. Into the stands!"),
    dict(id="Commentary.Six.004", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=10, text="Maximum! Absolutely smoked off the middle."),
    dict(id="Commentary.Six.005", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=9, text="That is huge! The crowd is on its feet."),
    dict(id="Commentary.Six.006", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=9, text="Straight down the ground. A monster hit."),
    dict(id="Commentary.Six.007", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=9, text="What a way to swing the over! Six more."),
    dict(id="Commentary.Six.008", category="SIX", voice="A", priority=82, cooldown=2, delay=0.5, weight=9, text="Two in a row! This batting is simply breathtaking."),
    dict(id="Commentary.Six.009", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=8, text="Picked the length early and deposited it with interest."),
    dict(id="Commentary.Six.010", category="SIX", voice="A", priority=80, cooldown=3, delay=0.5, weight=8, text="That sounded sweet from the moment it left the bat."),
    dict(id="Commentary.Six.011", category="SIX", voice="B", priority=45, cooldown=5, delay=1.6, weight=6, text="The extension of the arms there was textbook power hitting.", follow=True),
    dict(id="Commentary.Six.012", category="SIX", voice="B", priority=45, cooldown=5, delay=1.6, weight=6, text="He committed to the loft early and backed himself fully.", follow=True),

    # ---- WICKET (14) ----
    dict(id="Commentary.Wicket.001", category="WICKET", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="Gone! That is a huge breakthrough."),
    dict(id="Commentary.Wicket.002", category="WICKET", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="The stumps are disturbed, and the bowler has his reward."),
    dict(id="Commentary.Wicket.003", category="WICKET", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="That changes this Super Over completely!"),
    dict(id="Commentary.Wicket.Bowled.001", category="BOWLED", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="Bowled him! Right through the gate. Timber!"),
    dict(id="Commentary.Wicket.Bowled.002", category="BOWLED", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="The furniture is rearranged! A perfect yorker."),
    dict(id="Commentary.Wicket.Caught.001", category="CAUGHT", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="Skied it, and taken! The fielder never looked troubled."),
    dict(id="Commentary.Wicket.Caught.002", category="CAUGHT", voice="A", priority=90, cooldown=2, delay=0.55, weight=10, text="Gone for the big one, and holes out! What drama."),
    dict(id="Commentary.Wicket.Caught.003", category="CAUGHT", voice="A", priority=90, cooldown=2, delay=0.55, weight=9, text="Safe hands under the high ball. The breakthrough arrives."),
    dict(id="Commentary.Wicket.Keeper.001", category="KEEPER_CATCH", voice="A", priority=88, cooldown=2, delay=0.55, weight=9, text="Feathered behind! The keeper makes no mistake."),
    dict(id="Commentary.Wicket.004", category="WICKET", voice="A", priority=90, cooldown=2, delay=0.55, weight=9, text="The pressure told in the end. A massive moment."),
    dict(id="Commentary.Wicket.005", category="WICKET", voice="B", priority=48, cooldown=4, delay=1.7, weight=6, text="That was the right ball at the right time. Full credit to the bowler.", follow=True),
    dict(id="Commentary.Wicket.006", category="WICKET", voice="B", priority=48, cooldown=4, delay=1.7, weight=6, text="The batter had to go for it, but the execution let him down.", follow=True),
    dict(id="Commentary.Wicket.007", category="WICKET", voice="A", priority=90, cooldown=2, delay=0.55, weight=9, text="Silence from one dugout, delirium in the other!"),
    dict(id="Commentary.Wicket.008", category="WICKET", voice="B", priority=48, cooldown=4, delay=1.7, weight=6, text="Two wickets gone changes the whole arithmetic of this over.", follow=True),

    # ---- ANALYST FOLLOW-UPS (12) ----
    dict(id="Commentary.Analysis.001", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=7, text="The lengths have been brave. Full is risky, but it buys wickets.", follow=True),
    dict(id="Commentary.Analysis.002", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=7, text="Notice how straight the field is. They are cutting off the easy single.", follow=True),
    dict(id="Commentary.Analysis.003", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=7, text="Timing matters more than muscle on this surface.", follow=True),
    dict(id="Commentary.Analysis.004", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=7, text="The bowlers are varying pace nicely. Nothing predictable so far.", follow=True),
    dict(id="Commentary.Analysis.005", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Batting first here is about intent, not preservation.", follow=True),
    dict(id="Commentary.Analysis.006", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Chasing sides love a boundary early. It settles every nerve.", follow=True),
    dict(id="Commentary.Analysis.007", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="That over shows why yorkers at pace are still gold at the death.", follow=True),
    dict(id="Commentary.Analysis.008", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Fielding standards have been sharp. Every run is being earned.", follow=True),
    dict(id="Commentary.Analysis.009", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Dot balls are currency in a Super Over. Each one doubles the pressure.", follow=True),
    dict(id="Commentary.Analysis.010", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="The contact has been clean tonight. The outfield is lightning fast.", follow=True),
    dict(id="Commentary.Analysis.011", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Smart bowling change of pace. The batter was through the shot early.", follow=True),
    dict(id="Commentary.Analysis.012", category="ANALYSIS", voice="B", priority=25, cooldown=5, delay=1.4, weight=6, text="Composure now. The side that stays calm usually takes these.", follow=True),

    # ---- MATCH RESULT (10) ----
    dict(id="Commentary.Result.Win.001", category="MATCH_WIN", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="They have done it! What a finish to this Super Over!"),
    dict(id="Commentary.Result.Win.002", category="MATCH_WIN", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="Victory sealed in style! Eclipse Oval erupts!"),
    dict(id="Commentary.Result.Win.003", category="MATCH_WIN", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="The chase is completed! Nerves of steel at the death!"),
    dict(id="Commentary.Result.Win.004", category="MATCH_WIN", voice="B", priority=60, cooldown=99, delay=2.2, weight=7, text="That is how you close out a tight game. Calm heads throughout.", follow=True),
    dict(id="Commentary.Result.Loss.001", category="MATCH_LOSS", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="The target is defended! The bowling side holds its nerve!"),
    dict(id="Commentary.Result.Loss.002", category="MATCH_LOSS", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="Heartbreak for the chasers. So close, yet so far."),
    dict(id="Commentary.Result.Loss.003", category="MATCH_LOSS", voice="B", priority=60, cooldown=99, delay=2.2, weight=7, text="The bowlers owned the big moments, and that made the difference.", follow=True),
    dict(id="Commentary.Result.Tie.001", category="MATCH_TIE", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="It finishes level! Nothing could separate these sides!"),
    dict(id="Commentary.Result.Win.005", category="MATCH_WIN", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="Unbelievable scenes! The Super Over delivers once again!"),
    dict(id="Commentary.Result.Loss.004", category="MATCH_LOSS", voice="A", priority=100, cooldown=99, delay=2.4, weight=10, text="Defended brilliantly! A triumph of nerve over power."),
]
