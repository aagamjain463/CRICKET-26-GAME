#pragma once
#include "CoreMinimal.h"
#include "C26Types.h"

/**
 * C26Controls — GesturePro control mathematics (WCC3-style pull-and-release).
 *
 * Pure functions shared by the batting gesture, bowling charge, bounce marker
 * and HUD preview layers. Kept free of actors/state so the models stay tunable
 * in one place and testable from automation without a live match.
 *
 * Design canvas is 1600x900 (see AC26HUD). All gesture distances are consumed
 * in DESIGN units so gameplay never depends on raw screen pixels.
 */
namespace C26Controls
{
    // ------------------------------------------------------------------
    // Pull vector (PART H)
    // ------------------------------------------------------------------
    inline FVector2D PullVector(const FVector2D& Start, const FVector2D& Current)
    {
        return Current - Start;
    }

    inline float PullMagnitude01(const FVector2D& Pull, float DeadZone, float MaxPull)
    {
        const float Dist = Pull.Size();
        if (Dist <= DeadZone || MaxPull <= DeadZone) return 0.f;
        return FMath::Clamp((Dist - DeadZone) / (MaxPull - DeadZone), 0.f, 1.f);
    }

    /** Same normalisation with a sensitivity gain on raw finger travel. */
    inline float PullMagnitude01(const FVector2D& Pull, float DeadZone, float MaxPull, float Sensitivity)
    {
        return PullMagnitude01(Pull * FMath::Max(0.05f, Sensitivity), DeadZone, MaxPull);
    }

    /**
     * Default aggression curve (overridable with a CurveFloat in FC26GestureTuning).
     * Deliberately NOT linear: the first quarter of the pull stays controlled so a
     * small nudge cannot produce a heave, and the last 15% is where maximum
     * commitment - and maximum risk - lives.
     *   0.00-0.25 controlled | 0.25-0.65 normal | 0.65-0.85 aggressive | 0.85-1.00 maximum
     */
    inline float DefaultMagnitudeCurve(float Mag01)
    {
        const float M = FMath::Clamp(Mag01, 0.f, 1.f);
        if (M < 0.25f) return (M / 0.25f) * 0.22f;
        if (M < 0.65f) return 0.22f + ((M - 0.25f) / 0.40f) * 0.36f;
        if (M < 0.85f) return 0.58f + ((M - 0.65f) / 0.20f) * 0.27f;
        return 0.85f + ((M - 0.85f) / 0.15f) * 0.15f;
    }

    /** 0 controlled, 1 normal, 2 aggressive, 3 maximum. Drives the on-screen band label. */
    inline int AggressionBand(float Aggression01)
    {
        if (Aggression01 < 0.22f) return 0;
        if (Aggression01 < 0.58f) return 1;
        if (Aggression01 < 0.85f) return 2;
        return 3;
    }

    inline const TCHAR* AggressionBandName(float Aggression01)
    {
        switch (AggressionBand(Aggression01))
        {
        case 0:  return TEXT("CONTROLLED");
        case 1:  return TEXT("ATTACK");
        case 2:  return TEXT("POWER");
        default: return TEXT("MAXIMUM");
        }
    }

    // ------------------------------------------------------------------
    // Shot direction from gesture (PART I)
    //
    // The batting camera sits BEHIND THE BOWLER looking down the pitch, so the
    // picture the player aims at is mirrored against the world axes: for a
    // right-hander the OFF side (+X world) is on the LEFT of the screen and the
    // LEG side (-X world) is on the RIGHT. The drag has to follow the SCREEN,
    // not world X - pulling right plays to leg, pulling left plays to off, and
    // pulling up (toward the bowler) straightens the shot.
    //
    // Design space: -X design = batter's off side, +X design = leg side,
    // -Y design = up (toward the bowler), +Y design = down (toward the keeper).
    // Returns a continuous aim angle in degrees, -135..135, where POSITIVE is
    // the batter's off side - the same convention FC26Simulation::Hit and
    // DirectionZoneName already use.
    // ------------------------------------------------------------------
    inline float AimAngleFromPull(const FVector2D& Pull, float DeadZone, float Sensitivity, bool bLeftHandedBatter)
    {
        if (Pull.Size() <= DeadZone) return 0.f;
        // Angle of pull measured from screen-up (-Y). Screen-right (+X design) is
        // the batter's LEG side, so X is negated against the world convention:
        // pulling right yields a negative (leg-side) angle, pulling left positive.
        const float RawDeg = FMath::RadiansToDegrees(FMath::Atan2(-Pull.X, -Pull.Y));
        float Angle = FMath::Clamp(RawDeg * Sensitivity, -135.f, 135.f);
        if (bLeftHandedBatter) Angle = -Angle;
        return Angle;
    }

    // ------------------------------------------------------------------
    // Power from pull distance (PART J): 0..0.25 controlled, 0.25..0.65
    // normal, 0.65..1.0 powerful. Smooth curve, never unbounded.
    // ------------------------------------------------------------------
    inline float PowerFromPull(float Mag01, float MinPower, float MaxPower)
    {
        return FMath::Lerp(MinPower, MaxPower, DefaultMagnitudeCurve(Mag01));
    }

    /** Power from an already-shaped aggression value (CurveFloat path). */
    inline float PowerFromAggression(float Aggression01, float MinPower, float MaxPower)
    {
        return FMath::Lerp(MinPower, MaxPower, FMath::Clamp(Aggression01, 0.f, 1.f));
    }

    /**
     * Risk that rides with aggression (PART: risk/reward). A maximum pull is a
     * bigger swing, not a bigger guarantee: control drops, so mistiming bites
     * harder, the edge chance rises and the ball goes up more readily.
     */
    inline float ControlPenaltyFromAggression(float Aggression01)
    {
        const float A = FMath::Clamp(Aggression01, 0.f, 1.f);
        return A <= 0.58f ? 0.f : (A - 0.58f) / 0.42f * 0.30f;
    }

    /**
     * The same risk expressed against FC26ShotIntent::Power, which is what the
     * simulation receives. 0.73 power is the top of the "normal stroke" band, so
     * only genuine over-hitting costs control: less margin for a mistimed ball,
     * a thinner edge threshold and a livelier chance of the ball going up.
     */
    inline float ControlPenaltyFromPower(float Power01)
    {
        const float P = FMath::Clamp(Power01, 0.f, 1.f);
        return P <= 0.73f ? 0.f : (P - 0.73f) / 0.27f * 0.22f;
    }

    /**
     * Cricket name for a batter-relative aim angle. Angles are already mirrored
     * for a left-hander, so "COVER" means the batter's off side either way.
     */
    inline const TCHAR* DirectionZoneName(float AimAngleDeg)
    {
        if (AimAngleDeg > 100.f) return TEXT("THIRD MAN");
        if (AimAngleDeg > 68.f)  return TEXT("POINT");
        if (AimAngleDeg > 34.f)  return TEXT("COVER");
        if (AimAngleDeg > 14.f)  return TEXT("MID OFF");
        if (AimAngleDeg > -14.f) return TEXT("STRAIGHT");
        if (AimAngleDeg > -34.f) return TEXT("MID ON");
        if (AimAngleDeg > -68.f) return TEXT("MIDWICKET");
        if (AimAngleDeg > -100.f) return TEXT("SQUARE LEG");
        return TEXT("FINE LEG");
    }

    inline float PowerBand(float Power01)
    {
        if (Power01 < 0.45f) return 0.f; // controlled
        if (Power01 < 0.75f) return 1.f; // normal
        return 2.f;                      // powerful
    }

    // ------------------------------------------------------------------
    // Release timing (PART K/L). DeltaMs = actual - ideal in milliseconds.
    // Returns label + quality 0..1. Windows scale with difficulty.
    // ------------------------------------------------------------------
    inline EC26Timing TimingLabelFromDelta(float DeltaMs, float PerfectMs, float GoodMs, float ContactMs, float& OutQuality)
    {
        const float A = FMath::Abs(DeltaMs);
        if (A <= PerfectMs) OutQuality = FMath::Lerp(1.f, 0.95f, A / FMath::Max(1.f, PerfectMs));
        else if (A <= GoodMs) OutQuality = FMath::Lerp(0.95f, 0.75f, (A - PerfectMs) / FMath::Max(1.f, GoodMs - PerfectMs));
        else if (A <= ContactMs) OutQuality = FMath::Lerp(0.75f, 0.25f, (A - GoodMs) / FMath::Max(1.f, ContactMs - GoodMs));
        else OutQuality = 0.f;

        if (A > ContactMs) return EC26Timing::Miss;
        if (A <= PerfectMs) return EC26Timing::Perfect;
        if (A <= GoodMs) return EC26Timing::Good;
        return DeltaMs < 0 ? EC26Timing::Early : EC26Timing::Late;
    }

    /**
     * Six-band release-timing classification. Fed the SAME DeltaMs that becomes
     * the simulation's timing error, so the meter and the result are one number.
     * DeltaMs < 0 = released before the ideal instant.
     */
    inline EC26ReleaseTiming ReleaseTimingFromDelta(float DeltaMs, float PerfectMs, float GoodMs, float VeryMs, float ContactMs)
    {
        const float A = FMath::Abs(DeltaMs);
        if (A > ContactMs) return EC26ReleaseTiming::NoShot;
        if (A <= PerfectMs) return EC26ReleaseTiming::Perfect;
        if (A <= GoodMs) return EC26ReleaseTiming::Good;
        if (A <= VeryMs) return DeltaMs < 0.f ? EC26ReleaseTiming::Early : EC26ReleaseTiming::Late;
        return DeltaMs < 0.f ? EC26ReleaseTiming::VeryEarly : EC26ReleaseTiming::VeryLate;
    }

    inline const TCHAR* ReleaseTimingName(EC26ReleaseTiming T)
    {
        switch (T)
        {
        case EC26ReleaseTiming::VeryEarly: return TEXT("VERY EARLY");
        case EC26ReleaseTiming::Early:     return TEXT("EARLY");
        case EC26ReleaseTiming::Good:      return TEXT("GOOD");
        case EC26ReleaseTiming::Perfect:   return TEXT("PERFECT");
        case EC26ReleaseTiming::Late:      return TEXT("LATE");
        case EC26ReleaseTiming::VeryLate:  return TEXT("VERY LATE");
        default:                           return TEXT("NO SHOT");
        }
    }

    // ------------------------------------------------------------------
    // Line + length recognition from TRUE trajectory parameters (PART F).
    // Line/Length in the sim's centimetre plan space.
    // ------------------------------------------------------------------
    inline EC26DeliveryLength ClassifyLength(float LengthCm)
    {
        if (LengthCm > 730.f) return EC26DeliveryLength::Yorker;
        if (LengthCm > 550.f) return EC26DeliveryLength::Full;
        if (LengthCm > 180.f) return EC26DeliveryLength::GoodLength;
        if (LengthCm > 60.f) return EC26DeliveryLength::Short;
        return EC26DeliveryLength::Bouncer;
    }

    // World +X is the off side of a right-handed batter: the batter stands at
    // X=-38 with the stumps at X=0, and FC26Simulation::Hit cuts a ball at +X
    // and pulls one at -X. Line classification has to agree with the physics or
    // every "outside off" label - and every swing direction built on top of it -
    // is the wrong side of the wicket.
    inline EC26DeliveryLine ClassifyLine(float LineCm)
    {
        if (LineCm > 50.f) return EC26DeliveryLine::WideOff;
        if (LineCm > 20.f) return EC26DeliveryLine::OutsideOff;
        if (LineCm > 6.f) return EC26DeliveryLine::OffStump;
        if (LineCm >= -6.f) return EC26DeliveryLine::MiddleStump;
        if (LineCm >= -20.f) return EC26DeliveryLine::LegStump;
        return EC26DeliveryLine::DownLeg;
    }

    // ------------------------------------------------------------------
    // Footwork (PART M): ideal stride for a length. +front foot, -back foot.
    // ------------------------------------------------------------------
    inline float IdealStrideForLength(float LengthCm)
    {
        if (LengthCm < 180.f) return -0.8f;  // short / bouncer: back foot
        if (LengthCm > 560.f) return 0.75f;  // full / yorker: front foot
        return 0.25f;                        // good length: neutral-press
    }

    // ------------------------------------------------------------------
    // Shot suitability estimate (PART R). Mirrors the authority in
    // FC26Simulation::Hit so the gesture layer can anticipate and the HUD
    // can hint, but the SIMULATION result always wins.
    // ------------------------------------------------------------------
    inline float ShotSuitability(float AimAngle, float LengthCm, float LineCm, bool bLoft, bool bDefend)
    {
        float S = 1.f;
        const EC26DeliveryLength L = ClassifyLength(LengthCm);
        const bool Short = (L == EC26DeliveryLength::Short || L == EC26DeliveryLength::Bouncer);
        const bool Yorker = (L == EC26DeliveryLength::Yorker);
        const float A = FMath::Abs(AimAngle);

        if (Short)
        {
            // Driving straight off a short ball is the classic mistake.
            if (A < 38.f) S -= 0.30f;
            if (bLoft && L == EC26DeliveryLength::Bouncer) S -= 0.10f;
        }
        else if (Yorker)
        {
            if (A > 60.f) S -= 0.34f;
            if (bLoft) S -= 0.28f;
        }
        else
        {
            // Full/good: square cuts need width outside off (+X); a leg-side
            // stroke to a ball well outside off is a reach across the line.
            if (AimAngle > 78.f && LineCm < 6.f) S -= 0.24f;
            if (AimAngle < -34.f && LineCm > 30.f) S -= 0.22f;
        }
        if (bDefend) S = FMath::Max(S, 0.72f); // defence is rarely "wrong"
        return FMath::Clamp(S, 0.15f, 1.f);
    }

    /**
     * Shot family from delivery + intent. This is the SINGLE implementation:
     * FC26Simulation::Hit calls it to name the stroke it actually played, and the
     * gesture preview calls it with the predicted contact geometry, so the label
     * the player reads while pulling is the label the scorecard prints.
     *
     * The same drag direction deliberately produces different families by length:
     * off side is a cover drive off a full ball and a cut off a short one.
     */
    inline const TCHAR* ShotFamily(float ShotAngleDeg, float LengthCm, float BallHeightCm, float Stride, float OffsetCm, bool bLoft, bool bDefend)
    {
        const bool Short = BallHeightCm > 108.f || LengthCm < 180.f;
        const bool Yorker = BallHeightCm < 27.f && LengthCm > 710.f;
        if (bDefend) return Short ? TEXT("BACK-FOOT DEFENCE") : Yorker ? TEXT("YORKER BLOCK") : TEXT("DEFENSIVE PUSH");
        if (Short)
        {
            float A = ShotAngleDeg;
            if (FMath::Abs(A) < 38.f) A = OffsetCm > 20.f ? 72.f : -68.f; // no driving a short ball
            if (A > 0.f) return BallHeightCm > 148.f ? TEXT("UPPER CUT") : TEXT("SQUARE CUT");
            return BallHeightCm > 148.f ? TEXT("HOOK") : TEXT("PULL");
        }
        if (Yorker) return TEXT("DUG-OUT DRIVE");
        // Committing well beyond the ideal press to a full leg-side ball below the hip means going
        // down on the back knee: a sweep, or a slog sweep when lofted.
        if (ShotAngleDeg < -34.f && BallHeightCm < 80.f && Stride > IdealStrideForLength(LengthCm) + 0.1f)
            return bLoft ? TEXT("SLOG SWEEP") : TEXT("SWEEP");
        if (ShotAngleDeg > 78.f) return TEXT("LATE CUT");
        if (ShotAngleDeg > 55.f) return Stride < -0.3f ? TEXT("BACK-FOOT PUNCH") : TEXT("EXTRA-COVER DRIVE");
        if (ShotAngleDeg > 22.f) return bLoft ? TEXT("LOFTED COVER DRIVE") : TEXT("COVER DRIVE");
        if (ShotAngleDeg < -75.f) return TEXT("LEG GLANCE");
        if (ShotAngleDeg < -34.f) return bLoft ? TEXT("LEG-SIDE PICKUP") : TEXT("FLICK");
        if (ShotAngleDeg < -15.f) return TEXT("ON DRIVE");
        return bLoft ? TEXT("LOFTED STRAIGHT DRIVE") : TEXT("STRAIGHT DRIVE");
    }

    // ------------------------------------------------------------------
    // Difficulty assistance presets (PART AL / PART D).
    // Difficulty: 0 Easy, 1 Normal, 2 Hard (+3 Expert headroom).
    // ------------------------------------------------------------------
    inline float MarkerUncertainty(int Difficulty)
    {
        switch (Difficulty)
        {
        case 0: return 6.f;
        case 2: return 30.f;
        case 3: return 44.f;
        default: return 16.f;
        }
    }

    inline float TimingWindowScale(int Difficulty)
    {
        switch (Difficulty)
        {
        case 0: return 1.35f;
        case 2: return 0.80f;
        case 3: return 0.65f;
        default: return 1.0f;
        }
    }

    inline float BowlingForgiveness(int Difficulty)
    {
        switch (Difficulty)
        {
        case 0: return 0.29f;
        case 2: return 0.16f;
        case 3: return 0.13f;
        default: return 0.22f;
        }
    }

    inline float BowlingErrorScale(int Difficulty)
    {
        switch (Difficulty)
        {
        case 0: return 0.65f;
        case 2: return 1.15f;
        case 3: return 1.30f;
        default: return 1.0f;
        }
    }

    // ------------------------------------------------------------------
    // Bowling charge (PART AA/AG): pull fraction -> pace effort 0..1, then
    // pace multiplier. Max effort costs accuracy (deterministic risk).
    // ------------------------------------------------------------------
    inline float EffortFromPull(float Mag01)
    {
        return FMath::Clamp(Mag01, 0.f, 1.f);
    }

    inline float PaceMultiplier(float Effort01)
    {
        return FMath::Lerp(0.86f, 1.10f, Effort01);
    }

    inline float EffortErrorPenalty(float Effort01)
    {
        // Only genuine over-exertion costs accuracy.
        return Effort01 > 0.88f ? (Effort01 - 0.88f) * 1.4f : 0.f;
    }

    // Deterministic per-delivery marker jitter direction (golden angle hash
    // on DeliveryId) so uncertainty never flickers frame to frame.
    inline FVector2D MarkerJitterDir(uint32 DeliveryId)
    {
        const float A = float(DeliveryId) * 2.399963f;
        return FVector2D(FMath::Cos(A), FMath::Sin(A));
    }
}
