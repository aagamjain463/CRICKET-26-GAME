#pragma once
#include "C26Types.h"

/**
 * C26Delivery - the bowling model.
 *
 * WORLD CONVENTION (matches FC26Simulation and the batting side):
 *   +X is the OFF side of a right-handed batter, -X is the leg side.
 *   The ball travels +Y from the bowler's end toward the striker.
 *   FC26DeliveryPlan::Swing     = lateral acceleration IN THE AIR before the bounce.
 *   FC26DeliveryPlan::Deviation = lateral acceleration OFF THE PITCH after it.
 * Positive values push the ball toward +X (away from a right-hander).
 *
 * A cutter, an off break and a leg break are Deviation. Conventional and
 * reverse swing are Swing, separated only by SwingOnset - reverse starts late,
 * which is the whole point of it. Nothing here is a renamed copy of anything
 * else, because the integrator applies the two in different phases of flight.
 *
 * Movement DIRECTION is authored batter-relative (+1 = away from the batter)
 * and only converted to world X at composition time, so every combination of
 * bowler arm and batter hand works without a hardcoded sign anywhere.
 */
namespace C26Delivery
{
    // ---------------------------------------------------------------- naming
    inline const TCHAR* Name(EC26Delivery Type)
    {
        switch (Type)
        {
        case EC26Delivery::Outswing:     return TEXT("OUTSWINGER");
        case EC26Delivery::Inswing:      return TEXT("INSWINGER");
        case EC26Delivery::ReverseOut:   return TEXT("REVERSE OUT");
        case EC26Delivery::ReverseIn:    return TEXT("REVERSE IN");
        case EC26Delivery::OffCutter:    return TEXT("OFF CUTTER");
        case EC26Delivery::LegCutter:    return TEXT("LEG CUTTER");
        case EC26Delivery::Slower:       return TEXT("SLOWER BALL");
        case EC26Delivery::SlowerCutter: return TEXT("SLOWER CUTTER");
        case EC26Delivery::Yorker:       return TEXT("YORKER");
        case EC26Delivery::Bouncer:      return TEXT("BOUNCER");
        case EC26Delivery::OffBreak:     return TEXT("OFF BREAK");
        case EC26Delivery::ArmBall:      return TEXT("ARM BALL");
        case EC26Delivery::TopSpinner:   return TEXT("TOP SPINNER");
        case EC26Delivery::Doosra:       return TEXT("DOOSRA");
        case EC26Delivery::LegBreak:     return TEXT("LEG BREAK");
        case EC26Delivery::Googly:       return TEXT("GOOGLY");
        case EC26Delivery::Flipper:      return TEXT("FLIPPER");
        default:                         return TEXT("STOCK PACE");
        }
    }

    inline const TCHAR* ShortName(EC26Delivery Type)
    {
        switch (Type)
        {
        case EC26Delivery::Outswing:     return TEXT("OUT");
        case EC26Delivery::Inswing:      return TEXT("IN");
        case EC26Delivery::ReverseOut:   return TEXT("R-OUT");
        case EC26Delivery::ReverseIn:    return TEXT("R-IN");
        case EC26Delivery::OffCutter:    return TEXT("OFF CUT");
        case EC26Delivery::LegCutter:    return TEXT("LEG CUT");
        case EC26Delivery::Slower:       return TEXT("SLOWER");
        case EC26Delivery::SlowerCutter: return TEXT("SL CUT");
        case EC26Delivery::OffBreak:     return TEXT("OFF BR");
        case EC26Delivery::ArmBall:      return TEXT("ARM");
        case EC26Delivery::TopSpinner:   return TEXT("TOP");
        case EC26Delivery::Doosra:       return TEXT("DOOSRA");
        case EC26Delivery::LegBreak:     return TEXT("LEG BR");
        case EC26Delivery::Googly:       return TEXT("GOOGLY");
        case EC26Delivery::Flipper:      return TEXT("FLIP");
        default:                         return TEXT("STOCK");
        }
    }

    inline const TCHAR* LengthName(float Length)
    {
        return Length > 730.f ? TEXT("YORKER") : Length > 550.f ? TEXT("FULL")
             : Length > 180.f ? TEXT("GOOD LENGTH") : Length > 60.f ? TEXT("SHORT") : TEXT("BOUNCER");
    }

    // +X is the off side of a right-hander; see the header comment.
    inline const TCHAR* LineName(float Line)
    {
        return Line > 50.f ? TEXT("WIDE OUTSIDE OFF") : Line > 20.f ? TEXT("OUTSIDE OFF")
             : Line > 6.f ? TEXT("OFF STUMP") : Line >= -6.f ? TEXT("MIDDLE STUMP")
             : Line >= -20.f ? TEXT("LEG STUMP") : TEXT("DOWN LEG");
    }

    // ------------------------------------------------------- movement model
    /** How long a late-onset swing takes to ramp in once it starts, seconds.
        Shared by the integrator and by the launch trim in FC26Simulation, so a
        reverse swing still arrives on the point the player aimed at. */
    inline constexpr float ReverseRampSeconds = 0.08f;

    /**
     * Lateral displacement, cm, produced by a swing of `Accel` cm/s^2 over a
     * flight of `FlightTime` seconds that only begins at `Onset` seconds.
     *
     * Onset 0 is conventional swing: it works from the moment of release, so the
     * displacement integrates over the whole flight. A late onset is reverse
     * swing: the ball travels straight for most of its journey and only then
     * bends, so the same acceleration moves it far less. That is precisely why
     * reverse swing needs its own, larger acceleration constant - and why the
     * launch has to be trimmed by THIS number rather than by a whole-flight
     * estimate, or a reverse swinger would pitch short of its target.
     */
    inline float SwingDeflection(float Accel, float FlightTime, float Onset, float Ramp = ReverseRampSeconds)
    {
        if (Accel == 0.f || FlightTime <= 0.f) return 0.f;
        if (Onset <= 0.f) return 0.5f * Accel * FlightTime * FlightTime;
        const float Window = FMath::Clamp(FlightTime - Onset, 0.f, FlightTime);
        if (Window <= Ramp) return Accel * Window * Window * Window / (6.f * Ramp);
        const float After = Window - Ramp;
        return Accel * (Ramp * Ramp / 6.f + 0.5f * Ramp * After + 0.5f * After * After);
    }

    /** What phase of flight this delivery moves in. */
    inline EC26Movement MovementOf(EC26Delivery Type)
    {
        switch (Type)
        {
        case EC26Delivery::Outswing:
        case EC26Delivery::Inswing:      return EC26Movement::Swing;
        case EC26Delivery::ReverseOut:
        case EC26Delivery::ReverseIn:    return EC26Movement::ReverseSwing;
        case EC26Delivery::OffCutter:
        case EC26Delivery::LegCutter:
        case EC26Delivery::SlowerCutter: return EC26Movement::Seam;
        case EC26Delivery::OffBreak:
        case EC26Delivery::Doosra:
        case EC26Delivery::LegBreak:
        case EC26Delivery::Googly:
        case EC26Delivery::TopSpinner:
        case EC26Delivery::Flipper:
        case EC26Delivery::ArmBall:      return EC26Movement::Spin;
        default:                         return EC26Movement::None;
        }
    }

    /** Natural direction, batter-relative: +1 away from the batter, -1 into them. */
    inline float NaturalDirection(EC26Delivery Type)
    {
        switch (Type)
        {
        case EC26Delivery::Outswing:
        case EC26Delivery::ReverseOut:
        case EC26Delivery::LegCutter:
        case EC26Delivery::LegBreak:
        case EC26Delivery::Doosra:       return  1.f;
        case EC26Delivery::Inswing:
        case EC26Delivery::ReverseIn:
        case EC26Delivery::OffCutter:
        case EC26Delivery::OffBreak:
        case EC26Delivery::Googly:       return -1.f;
        case EC26Delivery::SlowerCutter: return -1.f;
        default:                         return  0.f; // arm ball, top spinner, flipper, stock
        }
    }

    /** Deliveries whose direction the player is allowed to flip on the dial. */
    inline bool DirectionIsFree(EC26Delivery Type)
    {
        // A named delivery already states which way it goes; letting the dial
        // reverse an "outswinger" into an inswinger would make the label a lie.
        // Only the stock ball and the neutral variations are free to be steered.
        return NaturalDirection(Type) == 0.f;
    }

    inline const TCHAR* DirectionName(EC26Delivery Type, float Direction)
    {
        if (MovementOf(Type) == EC26Movement::None || FMath::Abs(Direction) < 0.12f) return TEXT("STRAIGHT ON");
        const bool Away = Direction > 0.f;
        switch (MovementOf(Type))
        {
        case EC26Movement::Seam: return Away ? TEXT("SEAMS AWAY") : TEXT("SEAMS IN");
        case EC26Movement::Spin: return Away ? TEXT("TURNS AWAY") : TEXT("TURNS IN");
        case EC26Movement::ReverseSwing: return Away ? TEXT("REVERSES AWAY") : TEXT("REVERSES IN");
        default: return Away ? TEXT("SWINGS AWAY") : TEXT("SWINGS IN");
        }
    }

    // ------------------------------------------------- per-bowler libraries
    /** Deliveries this bowler can actually attempt. A quick can't bowl a googly. */
    inline void Library(const FC26BowlerProfile& P, TArray<EC26Delivery>& Out)
    {
        Out.Reset();
        switch (P.Kind)
        {
        case EC26BowlerKind::Fast:
        case EC26BowlerKind::FastMedium:
            Out.Add(EC26Delivery::Pace);
            Out.Add(EC26Delivery::Outswing);
            Out.Add(EC26Delivery::Inswing);
            Out.Add(EC26Delivery::OffCutter);
            Out.Add(EC26Delivery::LegCutter);
            Out.Add(EC26Delivery::Slower);
            Out.Add(EC26Delivery::SlowerCutter);
            Out.Add(EC26Delivery::ReverseOut);
            Out.Add(EC26Delivery::ReverseIn);
            break;
        case EC26BowlerKind::Medium:
            Out.Add(EC26Delivery::Pace);
            Out.Add(EC26Delivery::Outswing);
            Out.Add(EC26Delivery::Inswing);
            Out.Add(EC26Delivery::OffCutter);
            Out.Add(EC26Delivery::Slower);
            break;
        case EC26BowlerKind::OffSpin:
            Out.Add(EC26Delivery::OffBreak);
            Out.Add(EC26Delivery::ArmBall);
            Out.Add(EC26Delivery::TopSpinner);
            if (P.SpinSkill > 0.72f) Out.Add(EC26Delivery::Doosra); // only if it is genuinely in the locker
            break;
        case EC26BowlerKind::LegSpin:
            Out.Add(EC26Delivery::LegBreak);
            Out.Add(EC26Delivery::Googly);
            Out.Add(EC26Delivery::TopSpinner);
            if (P.SpinSkill > 0.68f) Out.Add(EC26Delivery::Flipper);
            break;
        }
    }

    /** Reverse swing is conditional, not a free variation. */
    inline bool ReverseAvailable(const FC26BowlerProfile& P, const FC26BowlingTuning& T, int BallsBowled)
    {
        if (T.bAllowReverseAlways) return true;
        return P.ReverseSkill >= 0.35f && float(BallsBowled) >= T.ReverseBallAgeBalls;
    }

    inline bool Available(EC26Delivery Type, const FC26BowlerProfile& P, const FC26BowlingTuning& T, int BallsBowled)
    {
        TArray<EC26Delivery> Lib; Library(P, Lib);
        if (!Lib.Contains(Type)) return false;
        if (Type == EC26Delivery::ReverseOut || Type == EC26Delivery::ReverseIn)
            return ReverseAvailable(P, T, BallsBowled);
        return true;
    }

    /** Skill that governs this delivery's movement, 0..1. */
    inline float MovementSkill(EC26Delivery Type, const FC26BowlerProfile& P)
    {
        switch (MovementOf(Type))
        {
        case EC26Movement::Swing:        return P.SwingSkill;
        case EC26Movement::ReverseSwing: return P.ReverseSkill;
        case EC26Movement::Seam:         return P.CutterSkill;
        case EC26Movement::Spin:         return P.SpinSkill;
        default:                         return P.Control;
        }
    }

    // ------------------------------------------------------ speed and pace
    inline float KphToUnits(float Kph) { return Kph / 0.036f; }   // km/h -> cm/s
    inline float UnitsToKph(float Units) { return Units * 0.036f; }

    /** Where a delivery type sits inside the bowler's own range before the
        player's pace choice is applied. Slower balls cap out well below full. */
    inline void PaceWindow(EC26Delivery Type, const FC26BowlerProfile& P, float& OutMin, float& OutMax)
    {
        OutMin = P.MinSpeedKph; OutMax = P.MaxSpeedKph;
        const float Span = FMath::Max(1.f, P.MaxSpeedKph - P.MinSpeedKph);
        switch (Type)
        {
        case EC26Delivery::Slower:
        case EC26Delivery::SlowerCutter:
            OutMax = P.MinSpeedKph + Span * 0.34f;
            OutMin = P.MinSpeedKph - Span * 0.34f;
            break;
        case EC26Delivery::OffCutter:
        case EC26Delivery::LegCutter:
            // Cutters cost a little pace: the fingers come across the seam.
            OutMax = P.MaxSpeedKph - Span * 0.14f;
            break;
        case EC26Delivery::Bouncer:
            OutMin = P.MinSpeedKph + Span * 0.30f;
            break;
        default: break;
        }
        OutMin = FMath::Max(50.f, OutMin);
        OutMax = FMath::Max(OutMin + 2.f, OutMax);
    }

    inline float PlannedKph(const FC26BowlingPlan& Plan, const FC26BowlerProfile& P)
    {
        float Lo, Hi; PaceWindow(Plan.Type, P, Lo, Hi);
        return FMath::Lerp(Lo, Hi, FMath::Clamp(Plan.PaceNormalized, 0.f, 1.f));
    }

    /** Effort above this fraction of the range starts costing accuracy. */
    inline float EffortStrain(float PaceNormalized)
    {
        const float N = FMath::Clamp(PaceNormalized, 0.f, 1.f);
        return N <= 0.72f ? 0.f : (N - 0.72f) / 0.28f;
    }

    // ----------------------------------------------------- the release bar
    /** Difficulty-scaled band edges. Good and Perfect shrink on higher
        difficulty, which pushes the perfect release closer to the no-ball line. */
    inline FC26ReleaseBar ScaledBar(const FC26ReleaseBar& In, int Difficulty)
    {
        const float K = Difficulty == 0 ? 1.45f : Difficulty == 2 ? 0.72f : Difficulty >= 3 ? 0.55f : 1.f;
        FC26ReleaseBar B = In;
        const float NoBall = In.NoBallStart;                    // the wall never moves
        B.PerfectStart = NoBall - (NoBall - In.PerfectStart) * K;
        B.GoodStart = B.PerfectStart - (In.PerfectStart - In.GoodStart) * K;
        B.EarlyStart = FMath::Min(B.GoodStart - 0.04f, In.EarlyStart);
        B.NoBallStart = NoBall;
        B.DifficultyWidth = K;
        return B;
    }

    inline EC26ReleaseBand BandAt(float Meter, const FC26ReleaseBar& B)
    {
        if (Meter >= B.NoBallStart) return EC26ReleaseBand::NoBall;
        if (Meter >= B.PerfectStart) return EC26ReleaseBand::Perfect;
        if (Meter >= B.GoodStart) return EC26ReleaseBand::Good;
        if (Meter >= B.EarlyStart) return EC26ReleaseBand::Early;
        return EC26ReleaseBand::TooEarly;
    }

    inline const TCHAR* BandName(EC26ReleaseBand Band)
    {
        switch (Band)
        {
        case EC26ReleaseBand::NoBall:   return TEXT("NO BALL");
        case EC26ReleaseBand::Perfect:  return TEXT("PERFECT");
        case EC26ReleaseBand::Good:     return TEXT("GOOD");
        case EC26ReleaseBand::Early:    return TEXT("EARLY");
        default:                        return TEXT("TOO EARLY");
        }
    }

    /**
     * Execution quality 0..1 from where on the bar the player let go.
     * The peak sits at the very top of the Perfect band - one frame from the
     * no-ball line - so the risk and the reward are the same decision.
     */
    inline float QualityAt(float Meter, const FC26ReleaseBar& B)
    {
        const float Ideal = B.NoBallStart - (B.NoBallStart - B.PerfectStart) * 0.18f;
        if (Meter >= B.NoBallStart)
        {
            // Overstepped: the delivery still happens, just raggedly.
            const float Over = FMath::Clamp((Meter - B.NoBallStart) / FMath::Max(0.01f, 1.f - B.NoBallStart), 0.f, 1.f);
            return FMath::Lerp(0.55f, 0.20f, Over);
        }
        const float Span = FMath::Max(0.02f, Ideal - B.EarlyStart);
        const float Behind = FMath::Clamp((Ideal - Meter) / Span, 0.f, 1.f);
        // Smooth, continuous fall-off - never a bucketed lookup.
        return FMath::Clamp(1.f - Behind * Behind, 0.f, 1.f);
    }

    /** Signed release error in "run-up fractions"; negative = early. */
    inline float SignedError(float Meter, const FC26ReleaseBar& B)
    {
        const float Ideal = B.NoBallStart - (B.NoBallStart - B.PerfectStart) * 0.18f;
        return Meter - Ideal;
    }

    // ------------------------------------------------------- COMPOSE
    /**
     * The whole point of the system: plan + execution + ability -> actual ball.
     * Everything the player chose is honoured as INTENT; how much of it survives
     * is decided here, once, so the preview and the ball are the same model.
     */
    inline FC26DeliveryPlan Compose(
        const FC26BowlingPlan& Plan,
        const FC26BowlerProfile& Bowler,
        const FC26BowlingTuning& Tune,
        float ReleaseQuality,          // 0..1 from the bar
        float SignedReleaseError,      // - early, + late
        bool bNoBall,
        bool bBatterLeftHanded,
        int Difficulty,
        float DeterministicRoll)       // 0..1, per-delivery, never frame noise
    {
        FC26DeliveryPlan Out;
        Out.Type = Plan.Type;
        Out.NoBall = bNoBall;
        Out.Movement = MovementOf(Plan.Type);

        const float Q = FMath::Clamp(ReleaseQuality, 0.f, 1.f);
        const float Ragged = 1.f - Q;
        const float Strain = EffortStrain(Plan.PaceNormalized);
        const float DiffScale = Difficulty == 0 ? 0.62f : Difficulty == 2 ? 1.22f : Difficulty >= 3 ? 1.4f : 1.f;

        // ---- pace -----------------------------------------------------------
        const float WantKph = PlannedKph(Plan, Bowler);
        const float PaceLoss = Tune.MaxPaceLoss * Ragged * Ragged;
        Out.Speed = FMath::Clamp(KphToUnits(WantKph * (1.f - PaceLoss)), 1600.f, 4600.f);

        // ---- accuracy: intent scattered by execution, effort and ability ----
        const float Miss = Ragged * (1.f + Strain * Tune.HighEffortErrorPenalty)
                         * (1.f - Bowler.Accuracy * 0.55f) * DiffScale;
        // Deterministic direction so the same release always misses the same way.
        const float Spread = (DeterministicRoll * 2.f - 1.f);
        Out.Line = FMath::Clamp(Plan.TargetLine + Spread * Tune.MaxLineError * Miss, -135.f, 135.f);
        // Early release drags the ball short, late pushes it full: a real,
        // signed consequence rather than a symmetric random blob.
        Out.Length = FMath::Clamp(
            Plan.TargetLength + SignedReleaseError * 320.f * (1.f - Bowler.Control * 0.35f)
            + Spread * Tune.MaxLengthError * Miss * 0.5f, 0.f, 850.f);

        // ---- movement: direction is batter-relative until this line ---------
        const float BatterSign = bBatterLeftHanded ? -1.f : 1.f;
        const float WantDir = DirectionIsFree(Plan.Type)
            ? FMath::Clamp(Plan.MovementDirection, -1.f, 1.f) : NaturalDirection(Plan.Type);
        const float WorldDir = WantDir * BatterSign;
        const float Skill = MovementSkill(Plan.Type, Bowler);
        // Requested amount, capped by ability and by how cleanly it was released.
        const float Achieved = FMath::Clamp(Plan.MovementMagnitude, 0.f, 1.f)
            * FMath::Lerp(0.30f, 1.f, Skill)
            * FMath::Lerp(0.45f, 1.f, Q);

        Out.Swing = 0.f; Out.Deviation = 0.f; Out.SwingOnset = 0.f;
        switch (Out.Movement)
        {
        case EC26Movement::Swing:
            // Faster deliveries swing a little less; the seam has less time to work.
            Out.Swing = WorldDir * Achieved * Tune.MaxSwingAccel * FMath::Lerp(1.10f, 0.86f, Plan.PaceNormalized);
            break;
        case EC26Movement::ReverseSwing:
            Out.Swing = WorldDir * Achieved * Tune.MaxReverseAccel * FMath::Lerp(0.80f, 1.15f, Plan.PaceNormalized);
            // Late onset is what separates reverse from conventional swing.
            Out.SwingOnset = Tune.ReverseOnsetFrac;
            break;
        case EC26Movement::Seam:
            Out.Deviation = WorldDir * Achieved * Tune.MaxCutterAccel;
            break;
        case EC26Movement::Spin:
            Out.Deviation = WorldDir * Achieved * Tune.MaxSpinAccel;
            if (Plan.Type == EC26Delivery::TopSpinner) Out.Deviation *= 0.25f;
            break;
        default:
            // Stock ball: steerable shape rather than a dead straight line. The
            // dial's direction is a real request, so it has to produce a real
            // bend - a delivery the player builds by hand out of a plain ball
            // and the dial must move, or the control is a lie. Weaker than a
            // named outswinger or inswinger, which stay the stronger choice.
            Out.Swing = WorldDir * Achieved * Tune.MaxSwingAccel * 0.55f;
            break;
        }
        Out.Seam = Out.Deviation * 0.02f; // legacy presentation hook

        // ---- bounce ---------------------------------------------------------
        Out.Bounce = Out.Length < 180.f ? .69f : Out.Length > 730.f ? .47f : .55f;
        if (Plan.Type == EC26Delivery::TopSpinner) Out.Bounce += .09f;   // dips and kicks
        if (Plan.Type == EC26Delivery::Flipper) Out.Bounce -= .10f;      // skids on
        if (Plan.Type == EC26Delivery::Slower || Plan.Type == EC26Delivery::SlowerCutter) Out.Bounce += .04f;
        Out.Bounce = FMath::Clamp(Out.Bounce, .2f, .85f);
        return Out;
    }

    /** Legacy shaping for the AI, which still plans in type + length only. */
    inline void Shape(FC26DeliveryPlan& P)
    {
        P.Speed = P.Type == EC26Delivery::Slower ? 2470.f : 3670.f;
        P.Swing = P.Type == EC26Delivery::Outswing ? 165.f : P.Type == EC26Delivery::Inswing ? -165.f : 12.f;
        P.Bounce = P.Length < 180.f ? .69f : P.Length > 730.f ? .47f : .55f;
        P.Seam = P.Type == EC26Delivery::Outswing ? 16.f : P.Type == EC26Delivery::Inswing ? -16.f : 5.f;
        P.Movement = MovementOf(P.Type);
        P.SwingOnset = 0.f;
        P.Deviation = 0.f;
    }

    /** Release error applied to an already-composed plan (AI path). */
    inline FC26DeliveryPlan Execute(const FC26DeliveryPlan& Locked, float Error)
    {
        FC26DeliveryPlan Actual = Locked;
        const float E = FMath::Clamp(Error, -2.f, 2.f), Fault = FMath::Abs(E);
        const float ErrorScale = Fault < 0.25f ? 0.35f : (Fault < 0.7f ? 0.75f : 1.25f);
        Actual.Line += E * 38.f * ErrorScale;
        Actual.Length = FMath::Clamp(Actual.Length + E * 45.f * ErrorScale, 0.f, 850.f);
        Actual.Speed *= 1.f - FMath::Min(.18f, Fault * .07f);
        Actual.Swing *= 1.f - FMath::Min(.6f, Fault * .24f);
        Actual.Deviation *= 1.f - FMath::Min(.6f, Fault * .24f);
        Actual.Seam += E * 13.f;
        Actual.NoBall = Locked.NoBall || Error > 1.35f;
        return Actual;
    }

    inline const TCHAR* ReleaseName(float Error)
    {
        return FMath::Abs(Error) < .22f ? TEXT("PERFECT RELEASE") : FMath::Abs(Error) < .65f ? TEXT("GOOD RELEASE")
             : FMath::Abs(Error) > 1.2f ? TEXT("POOR RELEASE") : Error < 0 ? TEXT("EARLY RELEASE") : TEXT("LATE RELEASE");
    }
}
