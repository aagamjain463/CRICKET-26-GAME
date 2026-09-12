#include "C26CommentaryLibrary.h"

TArray<FC26CommentaryLineDef> FC26CommentaryLibrary::Lines;
bool FC26CommentaryLibrary::bInitialized = false;

const TArray<FC26CommentaryLineDef>& FC26CommentaryLibrary::GetAllLines()
{
    if (!bInitialized)
    {
        Initialize();
    }
    return Lines;
}

const FC26CommentaryLineDef* FC26CommentaryLibrary::FindLineById(FName LineId)
{
    for (const FC26CommentaryLineDef& Line : GetAllLines())
    {
        if (Line.LineId == LineId)
        {
            return &Line;
        }
    }
    return nullptr;
}

TArray<const FC26CommentaryLineDef*> FC26CommentaryLibrary::FindMatchingLines(
    FName Category,
    ECommentatorRole Role,
    ECommentaryEmotion Emotion,
    float Pressure,
    float Intensity,
    bool bIsFinalBall,
    bool bIsMatchWinning,
    bool bFollowUpOnly
)
{
    TArray<const FC26CommentaryLineDef*> Matches;
    for (const FC26CommentaryLineDef& L : GetAllLines())
    {
        if (L.Category != Category)
        {
            continue;
        }
        if (L.bFollowUpOnly != bFollowUpOnly)
        {
            continue;
        }
        if (L.Role != Role)
        {
            continue;
        }
        if (L.bFinalBallOnly && !bIsFinalBall)
        {
            continue;
        }
        if (L.bMatchWinningOnly && !bIsMatchWinning)
        {
            continue;
        }
        if (bIsMatchWinning && !L.bMatchWinningOnly && L.Emotion != ECommentaryEmotion::Celebratory)
        {
            // Prefer match-winning / celebratory lines in a winning moment
            continue;
        }
        if (Pressure < L.MinPressure || Pressure > L.MaxPressure)
        {
            continue;
        }
        if (Intensity < L.MinIntensity || Intensity > L.MaxIntensity)
        {
            continue;
        }

        Matches.Add(&L);
    }

    // Relax emotion/pressure constraints if pool is empty
    if (Matches.IsEmpty())
    {
        for (const FC26CommentaryLineDef& L : GetAllLines())
        {
            if (L.Category == Category && L.Role == Role && L.bFollowUpOnly == bFollowUpOnly)
            {
                if (L.bMatchWinningOnly && !bIsMatchWinning)
                {
                    continue;
                }
                if (L.bFinalBallOnly && !bIsFinalBall)
                {
                    continue;
                }
                Matches.Add(&L);
            }
        }
    }

    return Matches;
}

void FC26CommentaryLibrary::Initialize()
{
    if (bInitialized) return;
    Lines.Empty(160);

    auto AddLine = [](
        FName Id, FName Cat, ECommentatorRole R, ECommentaryEmotion Em,
        float MinI, float MaxI, float MinP, float MaxP,
        uint8 Pri, float Del, uint8 W, bool bFollow, bool bWinOnly, bool bFinalOnly,
        const TCHAR* Txt, const TCHAR* Prompt, FName PreGen
    )
    {
        FC26CommentaryLineDef L;
        L.LineId = Id;
        L.Category = Cat;
        L.Role = R;
        L.Emotion = Em;
        L.MinIntensity = MinI;
        L.MaxIntensity = MaxI;
        L.MinPressure = MinP;
        L.MaxPressure = MaxP;
        L.Priority = Pri;
        L.Delay = Del;
        L.Weight = W;
        L.bFollowUpOnly = bFollow;
        L.bMatchWinningOnly = bWinOnly;
        L.bFinalBallOnly = bFinalOnly;
        L.Text = Txt;
        L.ElevenLabsPrompt = Prompt;
        L.PreGenFile = PreGen;
        Lines.Add(L);
    };

    // ==========================================
    // 1. MATCH START & CHASE START
    // ==========================================
    AddLine(TEXT("START_001"), TEXT("MATCH_START"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.3f, 0.7f, 0.0f, 1.0f, 60, 0.4f, 10, false, false, false,
        TEXT("Welcome to Eclipse Oval. One over. Everything on the line."),
        TEXT("[excited] Welcome to Eclipse Oval! One over, six deliveries... everything on the line!"),
        TEXT("Commentary_Match_Start_001"));

    AddLine(TEXT("START_002"), TEXT("MATCH_START"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.3f, 0.7f, 0.0f, 1.0f, 60, 0.4f, 10, false, false, false,
        TEXT("Six balls a side under lights. This should be a cracker."),
        TEXT("[energetic] Six balls a side under lights at Eclipse Oval. This should be an absolute cracker!"),
        TEXT("Commentary_Match_Start_002"));

    AddLine(TEXT("START_003"), TEXT("MATCH_START"), ECommentatorRole::Lead, ECommentaryEmotion::Tense,
        0.4f, 0.8f, 0.0f, 1.0f, 60, 0.4f, 10, false, false, false,
        TEXT("The Super Over is here. No second chances from here on."),
        TEXT("[tense, building anticipation] The Super Over is here! Six deliveries, no second chances from here on."),
        TEXT("Commentary_Match_Start_003"));

    AddLine(TEXT("START_005"), TEXT("MATCH_START"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.2f, 0.5f, 0.0f, 1.0f, 30, 1.2f, 6, false, false, false,
        TEXT("The key is simple. Strike clean from ball one, there is no settling in."),
        TEXT("[calm, analytical] The key is simple in this format. Strike clean from ball one... there is no time to settle in."),
        TEXT("Commentary_Match_Start_005"));

    AddLine(TEXT("CHASE_001"), TEXT("CHASE_START"), ECommentatorRole::Lead, ECommentaryEmotion::Tense,
        0.4f, 0.8f, 0.2f, 1.0f, 70, 0.5f, 10, false, false, false,
        TEXT("The equation is clear. Six balls to chase it down."),
        TEXT("[tense, focused] The equation is crystal clear. Six balls to chase it down!"),
        TEXT("Commentary_Chase_Start_001"));

    AddLine(TEXT("CHASE_003"), TEXT("CHASE_START"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.3f, 0.6f, 0.2f, 1.0f, 35, 1.4f, 6, false, false, false,
        TEXT("Boundaries win chases like this. Ones and twos will not be enough alone."),
        TEXT("[analytical] Boundaries win chases like this. Ones and twos won't be enough on their own under this rate."),
        TEXT("Commentary_Chase_Start_003"));

    // ==========================================
    // 2. PRE-BALL & PRESSURE BUILDUP
    // ==========================================
    AddLine(TEXT("PRE_GEN_001"), TEXT("PRE_BALL"), ECommentatorRole::Lead, ECommentaryEmotion::Neutral,
        0.1f, 0.4f, 0.0f, 0.5f, 20, 0.0f, 10, false, false, false,
        TEXT("Here comes the bowler once again."),
        TEXT("[conversational] Here comes the bowler once again."),
        TEXT("Commentary_PreBall_Generic_001"));

    AddLine(TEXT("PRE_GEN_002"), TEXT("PRE_BALL"), ECommentatorRole::Lead, ECommentaryEmotion::Neutral,
        0.1f, 0.4f, 0.0f, 0.5f, 20, 0.0f, 10, false, false, false,
        TEXT("The batter takes guard. The field comes in."),
        TEXT("[observational] The batter takes guard. Ring fielders creeping in."),
        TEXT("Commentary_PreBall_Generic_002"));

    AddLine(TEXT("PRE_AFT_BND_001"), TEXT("AFTER_BOUNDARY"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.4f, 0.7f, 0.3f, 0.8f, 24, 0.0f, 9, false, false, false,
        TEXT("Momentum with the batting side after that boundary."),
        TEXT("[alert] Momentum firmly with the batting side after that boundary. Bowler under notice."),
        TEXT("Commentary_PreBall_AfterBoundary_001"));

    AddLine(TEXT("PRE_AFT_WKT_001"), TEXT("AFTER_WICKET"), ECommentatorRole::Lead, ECommentaryEmotion::Tense,
        0.5f, 0.8f, 0.4f, 0.9f, 24, 0.0f, 9, false, false, false,
        TEXT("A wicket changes everything. The new batter is under instant pressure."),
        TEXT("[tense, dramatic] A wicket changes everything! The new batter is under instant, immense pressure."),
        TEXT("Commentary_PreBall_AfterWicket_001"));

    AddLine(TEXT("PRESSURE_001"), TEXT("PRESSURE"), ECommentatorRole::Lead, ECommentaryEmotion::Tense,
        0.6f, 0.9f, 0.65f, 1.0f, 55, 0.0f, 10, false, false, false,
        TEXT("The equation is tightening now. Every ball matters."),
        TEXT("[tense, lowered tone] The equation is tightening right up now. Every single delivery matters."),
        TEXT("Commentary_Pressure_001"));

    AddLine(TEXT("FINAL_BALL_001"), TEXT("FINAL_BALL"), ECommentatorRole::Lead, ECommentaryEmotion::Dramatic,
        0.8f, 1.0f, 0.75f, 1.0f, 85, 0.0f, 10, false, false, true,
        TEXT("One ball left. This is what the whole night has built toward."),
        TEXT("[dramatic, slow cadence] One ball left. This is what the entire match has built toward."),
        TEXT("Commentary_FinalBall_001"));

    AddLine(TEXT("FINAL_BALL_002"), TEXT("FINAL_BALL"), ECommentatorRole::Lead, ECommentaryEmotion::Dramatic,
        0.8f, 1.0f, 0.75f, 1.0f, 85, 0.0f, 10, false, false, true,
        TEXT("The final delivery. Hold your breath around the ground."),
        TEXT("[dramatic, quiet intensity] The final delivery. Hold your breath all around Eclipse Oval."),
        TEXT("Commentary_FinalBall_002"));

    AddLine(TEXT("FINAL_BALL_003"), TEXT("FINAL_BALL"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.7f, 1.0f, 0.75f, 1.0f, 40, 1.2f, 7, false, false, true,
        TEXT("A boundary wins it. A dot could defend it. Nothing in between matters."),
        TEXT("[calm, razor-sharp] A boundary wins it. A dot defends it. Nothing in between matters now."),
        TEXT("Commentary_FinalBall_003"));

    // ==========================================
    // 3. DOT BALLS & ROUTINE RUNS
    // ==========================================
    AddLine(TEXT("DOT_001"), TEXT("DOT"), ECommentatorRole::Lead, ECommentaryEmotion::Analytical,
        0.1f, 0.35f, 0.0f, 0.5f, 30, 0.65f, 10, false, false, false,
        TEXT("Excellent control. No run there."),
        TEXT("[composed] Excellent control from the bowler. No run there."),
        TEXT("Commentary_Dot_001"));

    AddLine(TEXT("DOT_002"), TEXT("DOT"), ECommentatorRole::Lead, ECommentaryEmotion::Tense,
        0.4f, 0.7f, 0.5f, 0.9f, 30, 0.65f, 10, false, false, false,
        TEXT("A valuable dot ball under pressure."),
        TEXT("[tense, firm] A massive dot ball under pressure. That hurts the batting side."),
        TEXT("Commentary_Dot_002"));

    AddLine(TEXT("DOT_005"), TEXT("DOT"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.3f, 0.7f, 0.4f, 1.0f, 28, 1.3f, 6, true, false, false,
        TEXT("That dot is worth its weight in gold at this stage."),
        TEXT("[analytical] That dot ball is worth its weight in gold at this stage of the contest."),
        TEXT("Commentary_Dot_005"));

    AddLine(TEXT("RUN_001"), TEXT("RUNS"), ECommentatorRole::Lead, ECommentaryEmotion::Neutral,
        0.15f, 0.4f, 0.0f, 0.6f, 40, 0.6f, 10, false, false, false,
        TEXT("Worked away for a single. Smart cricket."),
        TEXT("[smooth] Worked away into the gap for a single. Smart cricket."),
        TEXT("Commentary_Runs_One_001"));

    AddLine(TEXT("RUN_002"), TEXT("RUNS"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.35f, 0.65f, 0.3f, 0.8f, 42, 0.6f, 9, false, false, false,
        TEXT("Two runs. Superb urgency between the wickets."),
        TEXT("[alert] Two runs! Superb urgency between the wickets."),
        TEXT("Commentary_Runs_Two_001"));

    AddLine(TEXT("EDGE_001"), TEXT("EDGE"), ECommentatorRole::Lead, ECommentaryEmotion::Surprised,
        0.5f, 0.85f, 0.2f, 0.9f, 60, 0.5f, 9, false, false, false,
        TEXT("Thick edge, and that could have gone anywhere."),
        TEXT("[surprised, sudden] Thick edge! And that could have gone anywhere!"),
        TEXT("Commentary_Runs_Edge_001"));

    // ==========================================
    // 4. FOURS (ROUTINE, CRASHED, PRESSURE)
    // ==========================================
    AddLine(TEXT("FOUR_001"), TEXT("FOUR"), ECommentatorRole::Lead, ECommentaryEmotion::Appreciative,
        0.4f, 0.7f, 0.0f, 0.6f, 70, 0.45f, 10, false, false, false,
        TEXT("That is beautifully placed. The fielder had no chance."),
        TEXT("[appreciative, smooth] That is beautifully placed. Struck with pure timing, fielder had no chance!"),
        TEXT("Commentary_Four_001"));

    AddLine(TEXT("FOUR_002"), TEXT("FOUR"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.45f, 0.75f, 0.1f, 0.7f, 70, 0.45f, 10, false, false, false,
        TEXT("Timed cleanly through the gap. Four runs."),
        TEXT("[excited, energetic] Timed cleanly through the gap! That races away to the boundary for four!"),
        TEXT("Commentary_Four_002"));

    AddLine(TEXT("FOUR_003"), TEXT("FOUR"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.6f, 0.9f, 0.6f, 1.0f, 75, 0.45f, 10, false, false, false,
        TEXT("He has found the boundary when his side needed it."),
        TEXT("[high energy, building excitement] He has found the boundary just when his side needed it most!"),
        TEXT("Commentary_Four_003"));

    AddLine(TEXT("FOUR_004"), TEXT("FOUR"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.55f, 0.85f, 0.2f, 0.8f, 70, 0.45f, 10, false, false, false,
        TEXT("Crashed away to the rope. What a strike."),
        TEXT("[punchy, loud] Crashed away to the rope! What a strike off the front foot!"),
        TEXT("Commentary_Four_004"));

    AddLine(TEXT("FOUR_009"), TEXT("FOUR"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.65f, 0.95f, 0.4f, 0.9f, 72, 0.45f, 12, false, false, false,
        TEXT("Back to back boundaries! The bowler is under real pressure now."),
        TEXT("[very excited, rapid] Back to back boundaries! The bowler is under serious pressure now!"),
        TEXT("Commentary_Four_009"));

    AddLine(TEXT("FOUR_011"), TEXT("FOUR"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.4f, 0.7f, 0.0f, 0.8f, 42, 1.5f, 6, true, false, false,
        TEXT("The placement was perfect. He barely seemed to hit it."),
        TEXT("[analytical, admiring] The placement was immaculate. He barely seemed to hit it, pure timing."),
        TEXT("Commentary_Four_011"));

    // ==========================================
    // 5. SIXES (MODERATE, HUGE, HIGH PRESSURE, MATCH WINNING)
    // ==========================================
    // Moderate Six (Low to mid pressure)
    AddLine(TEXT("SIX_MOD_001"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.65f, 0.82f, 0.0f, 0.55f, 80, 0.5f, 10, false, false, false,
        TEXT("That is launched high and long. All the way!"),
        TEXT("[excited, soaring tone] That is launched high and long... into the night sky, and all the way for six!"),
        TEXT("Commentary_Six_001"));

    AddLine(TEXT("SIX_MOD_002"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::Appreciative,
        0.65f, 0.82f, 0.0f, 0.55f, 80, 0.5f, 10, false, false, false,
        TEXT("Clean strike. That has gone all the way."),
        TEXT("[smooth, appreciative] Clean strike off the middle! That has cleared the ropes comfortably."),
        TEXT("Commentary_Six_002"));

    AddLine(TEXT("SIX_MOD_004"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::Excited,
        0.7f, 0.88f, 0.1f, 0.65f, 80, 0.5f, 10, false, false, false,
        TEXT("Maximum! Absolutely smoked off the middle."),
        TEXT("[punchy, energetic] Maximum! Absolutely smoked right out of the middle!"),
        TEXT("Commentary_Six_004"));

    // High Pressure Six (Pressure > 0.65)
    AddLine(TEXT("SIX_PRESS_005"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.8f, 0.98f, 0.65f, 1.0f, 85, 0.45f, 12, false, false, false,
        TEXT("That is huge! The crowd is on its feet."),
        TEXT("[shouts, intense excitement] That is huge! Absolutely dispatched under intense pressure! The crowd is on its feet!"),
        TEXT("Commentary_Six_005"));

    AddLine(TEXT("SIX_PRESS_007"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.82f, 0.98f, 0.65f, 1.0f, 85, 0.45f, 12, false, false, false,
        TEXT("What a way to swing the over! Six more."),
        TEXT("[roaring excitement] What a sensational way to swing the over! Six more into the stands!"),
        TEXT("Commentary_Six_007"));

    // Match Winning Six (Target achieved on a 6)
    AddLine(TEXT("SIX_WIN_001"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::Celebratory,
        0.92f, 1.0f, 0.75f, 1.0f, 98, 0.35f, 20, false, true, false,
        TEXT("Victory sealed in style! Eclipse Oval erupts!"),
        TEXT("[shouts, ecstatic, full volume] HE'S HIT IT ALL THE WAY! VICTORY SEALED IN BREATHTAKING STYLE! ECLIPSE OVAL ERUPTS!"),
        TEXT("Commentary_Result_Win_002"));

    AddLine(TEXT("SIX_WIN_002"), TEXT("SIX"), ECommentatorRole::Lead, ECommentaryEmotion::Celebratory,
        0.92f, 1.0f, 0.75f, 1.0f, 98, 0.35f, 20, false, true, false,
        TEXT("They have done it! What a finish to this Super Over!"),
        TEXT("[shouts, maximum excitement] IT'S OUT OF THE STADIUM! THEY HAVE DONE IT! WHAT AN UNBELIEVABLE FINISH TO THIS SUPER OVER!"),
        TEXT("Commentary_Result_Win_001"));

    // Analyst Follow-up for Six
    AddLine(TEXT("SIX_ANA_011"), TEXT("SIX"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.5f, 0.85f, 0.0f, 1.0f, 45, 1.6f, 6, true, false, false,
        TEXT("The extension of the arms there was textbook power hitting."),
        TEXT("[analytical, respectful] The extension through the ball there was textbook power hitting. Kept his shape completely."),
        TEXT("Commentary_Six_011"));

    // ==========================================
    // 6. WICKETS (BOWLED, CAUGHT, KEEPER, DRAMA)
    // ==========================================
    AddLine(TEXT("WKT_GEN_001"), TEXT("WICKET"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.75f, 0.95f, 0.0f, 0.75f, 90, 0.55f, 10, false, false, false,
        TEXT("Gone! That is a huge breakthrough."),
        TEXT("[shouts, sudden] GONE! That is a massive breakthrough in this contest!"),
        TEXT("Commentary_Wicket_001"));

    AddLine(TEXT("WKT_BOWLED_001"), TEXT("BOWLED"), ECommentatorRole::Lead, ECommentaryEmotion::Shocked,
        0.85f, 1.0f, 0.0f, 1.0f, 92, 0.5f, 12, false, false, false,
        TEXT("Bowled him! Right through the gate. Timber!"),
        TEXT("[shouts, dramatic shock] BOWLED HIM! Right through the gate! Absolute timber!"),
        TEXT("Commentary_Wicket_Bowled_001"));

    AddLine(TEXT("WKT_BOWLED_002"), TEXT("BOWLED"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.85f, 1.0f, 0.0f, 1.0f, 92, 0.5f, 12, false, false, false,
        TEXT("The furniture is rearranged! A perfect yorker."),
        TEXT("[excited, authoritative] The furniture is completely rearranged! A devastating yorker!"),
        TEXT("Commentary_Wicket_Bowled_002"));

    AddLine(TEXT("WKT_CAUGHT_001"), TEXT("CAUGHT"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.75f, 0.95f, 0.0f, 1.0f, 90, 0.55f, 10, false, false, false,
        TEXT("Skied it, and taken! The fielder never looked troubled."),
        TEXT("[building excitement, then decisive] Skied high into the air... and TAKEN! Safe hands in the deep!"),
        TEXT("Commentary_Wicket_Caught_001"));

    AddLine(TEXT("WKT_KEEPER_001"), TEXT("KEEPER_CATCH"), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited,
        0.75f, 0.95f, 0.0f, 1.0f, 88, 0.55f, 10, false, false, false,
        TEXT("Feathered behind! The keeper makes no mistake."),
        TEXT("[sharp, reactive] Feathered behind! The keeper makes no mistake whatsoever!"),
        TEXT("Commentary_Wicket_Keeper_001"));

    AddLine(TEXT("WKT_CRUCIAL_003"), TEXT("WICKET"), ECommentatorRole::Lead, ECommentaryEmotion::Dramatic,
        0.88f, 1.0f, 0.7f, 1.0f, 94, 0.5f, 12, false, false, false,
        TEXT("That changes this Super Over completely!"),
        TEXT("[dramatic shock, loud] GONE! That turns this entire Super Over completely on its head!"),
        TEXT("Commentary_Wicket_003"));

    AddLine(TEXT("WKT_ANA_005"), TEXT("WICKET"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.5f, 0.85f, 0.0f, 1.0f, 48, 1.7f, 6, true, false, false,
        TEXT("That was the right ball at the right time. Full credit to the bowler."),
        TEXT("[analytical, measured] That was the right delivery at the absolute right moment. Full credit to the bowler's discipline."),
        TEXT("Commentary_Wicket_005"));

    // ==========================================
    // 7. MATCH RESULTS (WIN, LOSS, TIE)
    // ==========================================
    AddLine(TEXT("RES_WIN_001"), TEXT("MATCH_WIN"), ECommentatorRole::Lead, ECommentaryEmotion::Celebratory,
        0.9f, 1.0f, 0.0f, 1.0f, 100, 1.8f, 10, false, true, false,
        TEXT("They have done it! What a finish to this Super Over!"),
        TEXT("[celebratory, full throttle] THEY HAVE DONE IT! WHAT A SENSATIONAL FINISH TO THIS SUPER OVER!"),
        TEXT("Commentary_Result_Win_001"));

    AddLine(TEXT("RES_WIN_003"), TEXT("MATCH_WIN"), ECommentatorRole::Lead, ECommentaryEmotion::Celebratory,
        0.9f, 1.0f, 0.0f, 1.0f, 100, 1.8f, 10, false, true, false,
        TEXT("The chase is completed! Nerves of steel at the death!"),
        TEXT("[celebratory, passionate] The chase is complete! Nerves of absolute steel at the death!"),
        TEXT("Commentary_Result_Win_003"));

    AddLine(TEXT("RES_LOSS_001"), TEXT("MATCH_LOSS"), ECommentatorRole::Lead, ECommentaryEmotion::Dramatic,
        0.75f, 0.95f, 0.0f, 1.0f, 100, 1.8f, 10, false, false, false,
        TEXT("The target is defended! The bowling side holds its nerve!"),
        TEXT("[dramatic, resounding] The target is defended! The bowling side holds its nerve in the clutch!"),
        TEXT("Commentary_Result_Loss_001"));

    AddLine(TEXT("RES_LOSS_002"), TEXT("MATCH_LOSS"), ECommentatorRole::Lead, ECommentaryEmotion::Disappointed,
        0.7f, 0.9f, 0.0f, 1.0f, 100, 1.8f, 10, false, false, false,
        TEXT("Heartbreak for the chasers. So close, yet so far."),
        TEXT("[somber, reflective] Heartbreak for the batting side. So close to victory, yet just out of reach."),
        TEXT("Commentary_Result_Loss_002"));

    AddLine(TEXT("RES_TIE_001"), TEXT("MATCH_TIE"), ECommentatorRole::Lead, ECommentaryEmotion::Shocked,
        0.85f, 1.0f, 0.0f, 1.0f, 100, 1.8f, 10, false, false, false,
        TEXT("It finishes level! Nothing could separate these sides!"),
        TEXT("[shocked, disbelieving] IT FINISHES TIED! Absolute deadlock! Nothing could separate these two sides!"),
        TEXT("Commentary_Result_Tie_001"));

    AddLine(TEXT("RES_ANA_004"), TEXT("MATCH_WIN"), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical,
        0.6f, 0.85f, 0.0f, 1.0f, 60, 2.2f, 7, true, true, false,
        TEXT("That is how you close out a tight game. Calm heads throughout."),
        TEXT("[calm, thoughtful] That is exactly how you close out a championship game. Composure when the heat was on."),
        TEXT("Commentary_Result_Win_004"));

    bInitialized = true;
}
