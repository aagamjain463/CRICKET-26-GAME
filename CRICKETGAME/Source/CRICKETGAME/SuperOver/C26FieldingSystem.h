#pragma once
#include "CoreMinimal.h"
#include "C26Types.h"

namespace C26Fielding
{
    /**
     * Identifies authentic cricket fielding position names dynamically
     * based on field coordinates and batsman handedness.
     */
    inline FString GetFieldPositionName(const FVector& Pos, bool bBatterLeftHanded)
    {
        // Normalize coordinates so +X is Batter's Off side, -X is Leg side
        const float SideX = bBatterLeftHanded ? -Pos.X : Pos.X;
        const float RelativeY = Pos.Y - C26Field::WicketY; // Relative to striker's stumps
        const float DistFromStumps = FMath::Sqrt(SideX * SideX + RelativeY * RelativeY);
        const float DistFromPitchCenter = FMath::Sqrt(Pos.X * Pos.X + Pos.Y * Pos.Y);

        // Special case: Wicket Keeper is positioned behind batsman near center line
        if (FMath::Abs(SideX) < 160.f && RelativeY > 120.f && RelativeY < 2400.f)
        {
            return TEXT("Wicket Keeper");
        }

        const bool bOffSide = SideX >= 0.f;
        const bool bBehindStumps = RelativeY > 0.f;
        const bool bCloseIn = DistFromStumps < 1100.f;
        const bool bOutfield = DistFromPitchCenter >= C26Field::InnerCircleRadius;

        // Angle in degrees relative to bowling axis (0 = looking down pitch towards bowler)
        const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(SideX, -RelativeY));

        if (bOffSide)
        {
            if (bBehindStumps)
            {
                if (bCloseIn)
                {
                    if (SideX < 450.f) return TEXT("First Slip");
                    if (SideX < 700.f) return TEXT("Second Slip");
                    if (SideX < 950.f) return TEXT("Third Slip");
                    if (SideX < 1200.f) return TEXT("Fly Slip");
                    return TEXT("Gully");
                }
                // Outfield or intermediate behind square on off side
                if (bOutfield)
                {
                    if (AngleDeg > 130.f) return TEXT("Deep Third Man");
                    return TEXT("Third Man");
                }
                if (SideX > 1400.f) return TEXT("Backward Point");
                return TEXT("Short Third Man");
            }
            else // In front of stumps (Off side)
            {
                if (bCloseIn)
                {
                    if (AngleDeg > 65.f) return TEXT("Silly Point");
                    return TEXT("Silly Mid Off");
                }
                if (bOutfield)
                {
                    if (AngleDeg > 80.f) return TEXT("Deep Point");
                    if (AngleDeg > 55.f) return TEXT("Deep Cover");
                    if (AngleDeg > 35.f) return TEXT("Deep Extra Cover");
                    return TEXT("Long Off");
                }
                // Infield ring
                if (AngleDeg > 75.f) return TEXT("Point");
                if (AngleDeg > 50.f) return TEXT("Cover");
                if (AngleDeg > 30.f) return TEXT("Extra Cover");
                return TEXT("Mid Off");
            }
        }
        else // Leg side (On side)
        {
            if (bBehindStumps)
            {
                if (bCloseIn)
                {
                    if (FMath::Abs(SideX) < 220.f) return TEXT("Leg Slip");
                    if (RelativeY < 600.f) return TEXT("Short Leg");
                    return TEXT("Leg Gully");
                }
                if (bOutfield)
                {
                    if (FMath::Abs(SideX) > 2800.f) return TEXT("Deep Fine Leg");
                    return TEXT("Fine Leg");
                }
                if (FMath::Abs(SideX) < 1400.f) return TEXT("Short Fine Leg");
                return TEXT("Backward Square Leg");
            }
            else // In front of stumps (Leg side)
            {
                if (bCloseIn)
                {
                    if (AngleDeg < -60.f) return TEXT("Forward Short Leg");
                    return TEXT("Silly Mid On");
                }
                if (bOutfield)
                {
                    if (AngleDeg < -80.f) return TEXT("Deep Square Leg");
                    if (AngleDeg < -45.f) return TEXT("Deep Mid Wicket");
                    return TEXT("Long On");
                }
                // Infield ring
                if (AngleDeg < -75.f) return TEXT("Square Leg");
                if (AngleDeg < -45.f) return TEXT("Mid Wicket");
                return TEXT("Mid On");
            }
        }
    }

    /**
     * Retrieves the standard 11-player field coordinates for the given preset.
     * Index 0: Bowler runup start
     * Index 1: Wicketkeeper
     * Indices 2..10: The 9 fielding athletes
     */
    inline TArray<FVector> GetPresetFieldPositions(EC26FieldPreset Preset, bool bLeftHandedBatter = false)
    {
        TArray<FVector> Pos;
        Pos.SetNum(11);

        // Standard Bowler & Keeper defaults
        Pos[0] = FVector(-20.f, -2700.f, 5.f); // Bowler
        Pos[1] = FVector(0.f, 2450.f, 5.f);    // Wicketkeeper

        switch (Preset)
        {
        case EC26FieldPreset::Balanced:
            Pos[2]  = FVector(120.f, 1850.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(2200.f, 950.f, 5.f);   // Point
            Pos[4]  = FVector(2100.f, 200.f, 5.f);   // Cover
            Pos[5]  = FVector(1650.f, -600.f, 5.f);  // Extra Cover
            Pos[6]  = FVector(750.f, -1350.f, 5.f);  // Mid Off
            Pos[7]  = FVector(-750.f, -1350.f, 5.f); // Mid On
            Pos[8]  = FVector(-1850.f, 100.f, 5.f);  // Mid Wicket
            Pos[9]  = FVector(-2250.f, 950.f, 5.f);  // Square Leg
            Pos[10] = FVector(-2400.f, 4800.f, 5.f); // Deep Fine Leg
            break;

        case EC26FieldPreset::Attacking:
            Pos[2]  = FVector(110.f, 1750.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(320.f, 1880.f, 5.f);   // 2nd Slip
            Pos[4]  = FVector(750.f, 1600.f, 5.f);   // Gully
            Pos[5]  = FVector(650.f, 850.f, 5.f);    // Silly Point
            Pos[6]  = FVector(-650.f, 850.f, 5.f);   // Short Leg
            Pos[7]  = FVector(1950.f, 950.f, 5.f);   // Point
            Pos[8]  = FVector(1850.f, 250.f, 5.f);   // Cover
            Pos[9]  = FVector(700.f, -1400.f, 5.f);  // Mid Off
            Pos[10] = FVector(-700.f, -1400.f, 5.f); // Mid On
            break;

        case EC26FieldPreset::Defensive:
            Pos[2]  = FVector(3400.f, 4600.f, 5.f);  // Deep Third Man
            Pos[3]  = FVector(4600.f, 1100.f, 5.f);  // Deep Point
            Pos[4]  = FVector(4400.f, -800.f, 5.f);  // Deep Cover
            Pos[5]  = FVector(1800.f, -4200.f, 5.f); // Long Off
            Pos[6]  = FVector(-4200.f, -600.f, 5.f); // Deep Mid Wicket
            Pos[7]  = FVector(1800.f, 400.f, 5.f);   // Cover
            Pos[8]  = FVector(1900.f, 1100.f, 5.f);  // Point
            Pos[9]  = FVector(750.f, -1300.f, 5.f);  // Mid Off
            Pos[10] = FVector(-1800.f, 200.f, 5.f);  // Mid Wicket
            break;

        case EC26FieldPreset::PowerplayAttack:
            Pos[2]  = FVector(110.f, 1750.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(330.f, 1880.f, 5.f);   // 2nd Slip
            Pos[4]  = FVector(780.f, 1650.f, 5.f);   // Gully
            Pos[5]  = FVector(1900.f, 1050.f, 5.f);  // Point
            Pos[6]  = FVector(1800.f, 300.f, 5.f);   // Cover
            Pos[7]  = FVector(700.f, -1400.f, 5.f);  // Mid Off
            Pos[8]  = FVector(-700.f, -1400.f, 5.f); // Mid On
            Pos[9]  = FVector(-4100.f, -400.f, 5.f); // Deep Mid Wicket (Boundary 1)
            Pos[10] = FVector(-2800.f, 4700.f, 5.f); // Deep Fine Leg (Boundary 2)
            break;

        case EC26FieldPreset::PowerplayDefensive:
            Pos[2]  = FVector(3300.f, 4600.f, 5.f);  // Deep Third Man (Boundary 1)
            Pos[3]  = FVector(2100.f, 1000.f, 5.f);  // Point
            Pos[4]  = FVector(2000.f, 350.f, 5.f);   // Cover
            Pos[5]  = FVector(1600.f, -550.f, 5.f);  // Extra Cover
            Pos[6]  = FVector(800.f, -1350.f, 5.f);  // Mid Off
            Pos[7]  = FVector(-800.f, -1350.f, 5.f); // Mid On
            Pos[8]  = FVector(-1800.f, 200.f, 5.f);  // Mid Wicket
            Pos[9]  = FVector(-2100.f, 1050.f, 5.f); // Square Leg
            Pos[10] = FVector(-4200.f, -700.f, 5.f); // Deep Mid Wicket (Boundary 2)
            break;

        case EC26FieldPreset::PaceAttack:
            Pos[2]  = FVector(100.f, 1750.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(310.f, 1880.f, 5.f);   // 2nd Slip
            Pos[4]  = FVector(520.f, 2010.f, 5.f);   // 3rd Slip
            Pos[5]  = FVector(820.f, 1700.f, 5.f);   // Gully
            Pos[6]  = FVector(1950.f, 1020.f, 5.f);  // Point
            Pos[7]  = FVector(1900.f, 300.f, 5.f);   // Cover
            Pos[8]  = FVector(750.f, -1400.f, 5.f);  // Mid Off
            Pos[9]  = FVector(-750.f, -1400.f, 5.f); // Mid On
            Pos[10] = FVector(-2800.f, 4800.f, 5.f); // Deep Fine Leg
            break;

        case EC26FieldPreset::SpinAttack:
            Pos[2]  = FVector(110.f, 1500.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(620.f, 920.f, 5.f);    // Silly Point
            Pos[4]  = FVector(-650.f, 920.f, 5.f);   // Short Leg
            Pos[5]  = FVector(1900.f, 1000.f, 5.f);  // Point
            Pos[6]  = FVector(1850.f, 250.f, 5.f);   // Cover
            Pos[7]  = FVector(750.f, -1300.f, 5.f);  // Mid Off
            Pos[8]  = FVector(-750.f, -1300.f, 5.f); // Mid On
            Pos[9]  = FVector(-1800.f, 150.f, 5.f);  // Mid Wicket
            Pos[10] = FVector(-4100.f, -500.f, 5.f); // Deep Mid Wicket
            break;

        case EC26FieldPreset::OffsideHeavy:
            Pos[2]  = FVector(110.f, 1800.f, 5.f);   // 1st Slip
            Pos[3]  = FVector(720.f, 1580.f, 5.f);   // Gully
            Pos[4]  = FVector(1850.f, 1350.f, 5.f);  // Backward Point
            Pos[5]  = FVector(2200.f, 750.f, 5.f);   // Cover Point
            Pos[6]  = FVector(1750.f, -500.f, 5.f);  // Extra Cover
            Pos[7]  = FVector(4400.f, -600.f, 5.f);  // Deep Cover
            Pos[8]  = FVector(800.f, -1400.f, 5.f);  // Mid Off
            Pos[9]  = FVector(-800.f, -1400.f, 5.f); // Mid On
            Pos[10] = FVector(-4200.f, -500.f, 5.f); // Deep Mid Wicket
            break;

        case EC26FieldPreset::LegsideHeavy:
            Pos[2]  = FVector(-750.f, -1400.f, 5.f); // Mid On
            Pos[3]  = FVector(-1850.f, 200.f, 5.f);  // Mid Wicket
            Pos[4]  = FVector(-4200.f, -500.f, 5.f); // Deep Mid Wicket
            Pos[5]  = FVector(-2200.f, 1000.f, 5.f); // Square Leg
            Pos[6]  = FVector(-3100.f, 4800.f, 5.f); // Deep Fine Leg
            Pos[7]  = FVector(750.f, -1400.f, 5.f);  // Mid Off
            Pos[8]  = FVector(1900.f, 300.f, 5.f);   // Cover
            Pos[9]  = FVector(2100.f, 1050.f, 5.f);  // Point
            Pos[10] = FVector(4300.f, -600.f, 5.f);  // Deep Cover
            break;

        case EC26FieldPreset::DeathOvers:
            Pos[2]  = FVector(3400.f, 4600.f, 5.f);  // Deep Third Man
            Pos[3]  = FVector(4600.f, 1100.f, 5.f);  // Deep Point
            Pos[4]  = FVector(4500.f, -600.f, 5.f);  // Deep Cover
            Pos[5]  = FVector(1750.f, -4400.f, 5.f); // Long Off
            Pos[6]  = FVector(-4300.f, -600.f, 5.f); // Deep Mid Wicket
            Pos[7]  = FVector(1700.f, 2100.f, 5.f);  // Short Third Man
            Pos[8]  = FVector(1650.f, -400.f, 5.f);  // Extra Cover
            Pos[9]  = FVector(-800.f, -1350.f, 5.f); // Mid On
            Pos[10] = FVector(-2100.f, 1050.f, 5.f); // Square Leg
            break;

        case EC26FieldPreset::ProtectBoundary:
            Pos[2]  = FVector(3400.f, 4600.f, 5.f);  // Deep Third Man
            Pos[3]  = FVector(4600.f, -500.f, 5.f);  // Deep Cover
            Pos[4]  = FVector(1800.f, -4400.f, 5.f); // Long Off
            Pos[5]  = FVector(-1800.f, -4400.f, 5.f);// Long On
            Pos[6]  = FVector(-4400.f, -500.f, 5.f); // Deep Mid Wicket
            Pos[7]  = FVector(2000.f, 1050.f, 5.f);  // Point
            Pos[8]  = FVector(1850.f, 300.f, 5.f);   // Cover
            Pos[9]  = FVector(800.f, -1350.f, 5.f);  // Mid Off
            Pos[10] = FVector(-1850.f, 250.f, 5.f);  // Mid Wicket
            break;

        case EC26FieldPreset::SinglePrevention:
        default:
            Pos[2]  = FVector(550.f, -350.f, 5.f);   // Silly Mid Off
            Pos[3]  = FVector(-550.f, -350.f, 5.f);  // Silly Mid On
            Pos[4]  = FVector(1450.f, 300.f, 5.f);   // Short Cover
            Pos[5]  = FVector(1600.f, 1250.f, 5.f);  // Backward Point
            Pos[6]  = FVector(-1450.f, 300.f, 5.f);  // Short Mid Wicket
            Pos[7]  = FVector(-1750.f, 950.f, 5.f);  // Square Leg
            Pos[8]  = FVector(750.f, -1200.f, 5.f);  // Mid Off
            Pos[9]  = FVector(-750.f, -1200.f, 5.f); // Mid On
            Pos[10] = FVector(-3100.f, 4700.f, 5.f); // Deep Fine Leg
            break;
        }
        // Mirror X for left-handed batsmen
        if (bLeftHandedBatter)
        {
            for (int32 I = 2; I <= 10; ++I)
            {
                Pos[I].X = -Pos[I].X;
            }
        }

        return Pos;
    }

    /**
     * Validates field legality against MCC Cricket Laws (Law 28) and Competition Regulations:
     * - Law 28.4: At most 5 fielders on leg side
     * - Law 28.4: At most 2 fielders behind popping crease on leg side
     * - Competition: Max 2 boundary fielders during powerplay, max 5 in normal overs
     * - Physical: Minimum separation between fielders
     */
    inline bool ValidateFieldLegality(
        const TArray<FVector>& Positions,
        bool bPowerplay,
        bool bLeftHandedBatter,
        FString& OutRuleViolation)
    {
        if (Positions.Num() < 11)
        {
            OutRuleViolation = TEXT("INCOMPLETE FIELDING TEAM");
            return false;
        }

        int32 LegSideCount = 0;
        int32 BehindSquareLegCount = 0;
        int32 BoundaryFielderCount = 0;

        for (int32 I = 2; I <= 10; ++I)
        {
            const FVector& P = Positions[I];

            // Normalize coordinate so -X is Leg side
            const float SideX = bLeftHandedBatter ? -P.X : P.X;

            // Leg side check
            if (SideX < 0.f)
            {
                LegSideCount++;
                // Behind popping crease (Y > 884)
                if (P.Y > C26Field::CreaseY)
                {
                    BehindSquareLegCount++;
                }
            }

            // Circle restriction check
            if (!C26Field::InsideInnerCircle(P))
            {
                BoundaryFielderCount++;
            }

            // Ensure fielder is inside boundary rope
            if (!C26Field::Inside(P))
            {
                OutRuleViolation = FString::Printf(TEXT("FIELDER %d IS OUTSIDE THE BOUNDARY ROPE"), I);
                return false;
            }

            // Separation check with other fielders
            for (int32 J = I + 1; J <= 10; ++J)
            {
                if (FVector::Dist2D(P, Positions[J]) < 180.f)
                {
                    OutRuleViolation = FString::Printf(TEXT("Fielders %d and %d are too close (<1.8m separation required)"), I, J);
                    return false;
                }
            }
        }

        if (LegSideCount > 5)
        {
            OutRuleViolation = FString::Printf(TEXT("Law 28.4: %d fielders on Leg side (max 5 allowed)"), LegSideCount);
            return false;
        }

        if (BehindSquareLegCount > 2)
        {
            OutRuleViolation = FString::Printf(TEXT("Law 28.4: %d fielders behind square on Leg side (max 2 allowed)"), BehindSquareLegCount);
            return false;
        }

        const int32 MaxBoundary = bPowerplay ? 2 : 5;
        if (BoundaryFielderCount > MaxBoundary)
        {
            OutRuleViolation = FString::Printf(TEXT("%s circle restriction: %d outside 30-yd ring (max %d allowed)"),
                bPowerplay ? TEXT("Powerplay") : TEXT("Regular overs"), BoundaryFielderCount, MaxBoundary);
            return false;
        }

        // Wicketkeeper must stay behind stumps
        if (Positions[1].Y < C26Field::WicketY - 10.f)
        {
            OutRuleViolation = TEXT("WICKETKEEPER MUST REMAIN BEHIND STUMPS");
            return false;
        }

        OutRuleViolation.Empty();
        return true;
    }

    /**
     * Preset naming lookup for HUD display
     */
    inline FString GetPresetName(EC26FieldPreset Preset)
    {
        switch (Preset)
        {
        case EC26FieldPreset::Balanced:           return TEXT("BALANCED");
        case EC26FieldPreset::Attacking:          return TEXT("ATTACKING");
        case EC26FieldPreset::Defensive:          return TEXT("DEFENSIVE");
        case EC26FieldPreset::PowerplayAttack:    return TEXT("POWERPLAY ATTACK");
        case EC26FieldPreset::PowerplayDefensive: return TEXT("POWERPLAY DEFENSE");
        case EC26FieldPreset::PaceAttack:         return TEXT("PACE ATTACK");
        case EC26FieldPreset::SpinAttack:         return TEXT("SPIN ATTACK");
        case EC26FieldPreset::OffsideHeavy:       return TEXT("OFF-SIDE HEAVY");
        case EC26FieldPreset::LegsideHeavy:       return TEXT("LEG-SIDE HEAVY");
        case EC26FieldPreset::DeathOvers:         return TEXT("DEATH OVERS");
        case EC26FieldPreset::ProtectBoundary:    return TEXT("PROTECT BOUNDARY");
        case EC26FieldPreset::SinglePrevention:   return TEXT("SINGLE PREVENTION");
        default:                                  return TEXT("CUSTOM");
        }
    }

    /**
     * Evaluates manual catch execution quality based on timing, reach, ball speed and fielder agility.
     * Returns true if catch succeeded, false if dropped.
     */
    inline bool EvaluateCatchQuality(
        float TimingDeltaSec,
        float DistanceToBallCm,
        float BallSpeedCmS,
        float FielderAgility,
        bool bDiving,
        EC26CatchTiming& OutTiming,
        float& OutQuality01)
    {
        const float AbsDt = FMath::Abs(TimingDeltaSec);

        float TimingScore = 0.f;
        if (AbsDt <= 0.11f)
        {
            OutTiming = EC26CatchTiming::Perfect;
            TimingScore = 1.0f - (AbsDt / 0.11f) * 0.15f;
        }
        else if (AbsDt <= 0.24f)
        {
            OutTiming = EC26CatchTiming::Good;
            TimingScore = 0.85f - ((AbsDt - 0.11f) / 0.13f) * 0.35f;
        }
        else if (TimingDeltaSec < -0.24f)
        {
            OutTiming = EC26CatchTiming::Early;
            TimingScore = FMath::Max(0.05f, 0.40f - ((-TimingDeltaSec - 0.24f) / 0.30f) * 0.35f);
        }
        else
        {
            OutTiming = EC26CatchTiming::Late;
            TimingScore = FMath::Max(0.05f, 0.40f - ((TimingDeltaSec - 0.24f) / 0.30f) * 0.35f);
        }

        // Reach penalty: ball within 120cm is easy, 250cm is a stretch
        const float ReachPenalty = FMath::Clamp((DistanceToBallCm - 60.f) / 200.f, 0.f, 0.50f);

        // Speed penalty: high pace drives through the fingers
        const float SpeedPenalty = FMath::Clamp((BallSpeedCmS - 2200.f) / 2800.f, 0.f, 0.35f);

        // Agility bonus: professional athletic reflexes
        const float AgilityFactor = FMath::Clamp(FielderAgility, 0.5f, 1.2f);

        // Diving catch has a tighter window
        const float DiveFactor = bDiving ? 0.85f : 1.0f;

        OutQuality01 = FMath::Clamp((TimingScore - ReachPenalty - SpeedPenalty) * AgilityFactor * DiveFactor, 0.f, 1.f);

        // Catch threshold: >= 0.40 succeeds
        return (OutQuality01 >= 0.40f);
    }

    /**
     * Evaluates manual ground dive outcome.
     * 0 = Clean stop / capture
     * 1 = Parried / deflected knockdown
     * 2 = Complete miss
     */
    inline int32 EvaluateDiveQuality(
        float TimingDeltaSec,
        float DistanceToBallCm,
        float BallSpeedCmS,
        float& OutSpeedDampening)
    {
        const float AbsDt = FMath::Abs(TimingDeltaSec);
        if (AbsDt <= 0.16f && DistanceToBallCm < 280.f)
        {
            // Clean dive stop
            OutSpeedDampening = 0.0f;
            return 0;
        }
        else if (AbsDt <= 0.35f && DistanceToBallCm < 360.f)
        {
            // Knockdown / parry
            OutSpeedDampening = 0.22f;
            return 1;
        }
        else
        {
            // Missed dive
            OutSpeedDampening = 1.0f;
            return 2;
        }
    }

    /**
     * Resolves throwing destination and trajectory physics.
     */
    inline void EvaluateThrowTrajectory(
        const FVector& FielderPos,
        EC26ThrowTarget TargetEnd,
        float Power01,
        FVector& OutTargetStumps,
        FVector& OutThrowVelocity,
        bool& bDirectHit)
    {
        // Stumps target: Bowler's end is Y = -995, Keeper's end is Y = 1006
        const float StumpsY = (TargetEnd == EC26ThrowTarget::KeepersEnd) ? C26Field::WicketY : -995.f;
        OutTargetStumps = FVector(0.f, StumpsY, C26Field::StumpHeight * 0.5f);

        const FVector FlatDelta = OutTargetStumps - FielderPos;
        const float Dist2D = FlatDelta.Size2D();
        const FVector Dir2D = FlatDelta.GetSafeNormal2D();

        // Speed scaled with power: 1800 cm/s up to 3400 cm/s (bullet throw)
        const float ThrowSpeed = FMath::Lerp(1800.f, 3400.f, FMath::Clamp(Power01, 0.2f, 1.0f));

        // Sweet spot is [0.70, 0.88]
        const bool bSweetSpot = (Power01 >= 0.70f && Power01 <= 0.88f);
        const float AccuracySpread = bSweetSpot ? 0.015f : FMath::Abs(Power01 - 0.78f) * 0.14f;

        // Spread angle
        const float SpreadAngleRad = AccuracySpread * (FMath::FRand() > 0.5f ? 1.f : -1.f);
        const FVector RotatedDir = Dir2D.RotateAngleAxis(FMath::RadiansToDegrees(SpreadAngleRad), FVector::UpVector);

        // Parabolic arc elevation
        const float FlightTime = Dist2D / FMath::Max(100.f, ThrowSpeed);
        const float RequiredVz = (OutTargetStumps.Z - FielderPos.Z) / FlightTime + 0.5f * 981.f * FlightTime;

        OutThrowVelocity = RotatedDir * ThrowSpeed + FVector(0.f, 0.f, RequiredVz);

        // Direct hit chance: high when sweet spot, decreases with distance
        bDirectHit = bSweetSpot && (Dist2D < 3800.f);
    }
}
