#pragma once
#include "CoreMinimal.h"
#include "C26Types.h"

/** Original sports poses in athlete centimetres: forward, right, up.
 * Timing markers are shared with the release authority. No animation owns a result.
 */
namespace C26Motion
{
    inline FVector Rig(float Forward, float Right, float Up) { return FVector(-Right, Forward, Up); }

    struct FPacePose
    {
        float Time;
        FVector LeftFoot,RightFoot,LeftHand,RightHand;
        float HipDrop,Turn,Lean,Side;
        FVector HipShift;
    };
    inline FPacePose Pace(float Time,float Ankle)
    {
        // Authentic fast bowler biomechanics:
        // Approach stride -> airborne gather bound -> back-foot contact & coil ->
        // front-foot plant & knee brace -> overhead release & trunk flexion ->
        // cross-body follow-through wrap -> 2 deceleration recovery strides -> live fielding stance.
        const FPacePose Keys[]={
            {0.f,   {-14,-9,0},  {24,10,14},   {24,-8,140},  {20,10,136}, -2, -16, 18, -4,  {0,0,0}},
            {.16f,  {20,-10,26}, {-22,10,20},  {18,-12,168}, {-14,14,152}, 14, -36, 10, -8,  {6,-2,0}},
            {.31f,  {58,-11,24}, {-26,11,0},   {38,-18,192}, {-34,18,128}, -4, -44, 8, -12, {-14,0,0}},
            {.48f,  {82,-12,0},  {-28,12,12},  {22,-22,138}, {-16,18,190}, -8, -18, 26, -14, {18,-2,0}},
            {.62f,  {78,-12,0},  {-42,14,28},  {12,-22,115}, {18,16,218},  -6,  6,   44, -12, {34,-4,0}},
            {.82f,  {48,-11,0},  {-6,14,22},   {-14,-24,110},{42,-18,120}, -12, 26,  52,  4,  {48,-6,0}},
            {1.08f, {-28,-10,18},{48,12,0},    {-20,-22,106},{16,-24,82},  -8,  22,  36,  6,  {58,-10,0}},
            {1.40f, {38,-10,0},  {-20,12,16},  {-10,-20,112},{20,-10,105}, -6,  10,  22,  4,  {66,-16,0}},
            {1.80f, {18,-12,0},  {8,12,0},     {22,-18,112}, {22,18,112},  -4,  2,   10,  0,  {70,-20,0}},
            {2.20f, {10,-12,0},  {0,12,0},     {24,-16,115}, {24,16,115},  -3,  0,   6,   0,  {70,-20,0}}
        };
        int I=0;while(I+1<int(UE_ARRAY_COUNT(Keys))-1&&Time>Keys[I+1].Time)++I;
        const auto& A=Keys[I];const auto& B=Keys[I+1];
        float T=FMath::Clamp((Time-A.Time)/(B.Time-A.Time),0.f,1.f);
        const float LinearT=T;
        T=T*T*(3.f-2.f*T);
        FPacePose R={Time,FMath::Lerp(A.LeftFoot,B.LeftFoot,T),FMath::Lerp(A.RightFoot,B.RightFoot,T),FMath::Lerp(A.LeftHand,B.LeftHand,T),FMath::Lerp(A.RightHand,B.RightHand,T),
            FMath::Lerp(A.HipDrop,B.HipDrop,T),FMath::Lerp(A.Turn,B.Turn,T),FMath::Lerp(A.Lean,B.Lean,T),FMath::Lerp(A.Side,B.Side,T),
            FMath::Lerp(A.HipShift,B.HipShift,T)};
        // Continuous hand velocity through the action, with the authored release key preserved.
        // Feet keep their support interpolation so the front-foot plant stays on the turf.
        const auto& Prev=Keys[FMath::Max(0,I-1)];
        const auto& Next=Keys[FMath::Min(int(UE_ARRAY_COUNT(Keys))-1,I+2)];
        const float Span=B.Time-A.Time;
        auto Arc=[&](const FVector& P,const FVector& X,const FVector& Y,const FVector& N)
        {
            return FMath::CubicInterp(X,(Y-P)*(Span/(B.Time-Prev.Time)),
                Y,(N-X)*(Span/(Next.Time-A.Time)),LinearT);
        };
        R.LeftHand=Arc(Prev.LeftHand,A.LeftHand,B.LeftHand,Next.LeftHand);
        R.RightHand=Arc(Prev.RightHand,A.RightHand,B.RightHand,Next.RightHand);
        R.LeftFoot.Z+=Ankle;R.RightFoot.Z+=Ankle;return R;
    }
    /** Where the bowling arm sits on its own circle, in radians, measured from straight up over
     * the shoulder: negative is behind the body, positive is in front of it.
     *
     * A pace action is one continuous revolution of that circle -- hanging, back, up, over the
     * top, then down and across the body -- and keying the ANGLE instead of a hand position is
     * what guarantees it stays overarm. A point generated from an angle on a circle of radius
     * ArmSpan is reachable by construction, so the arm stays straight and the action stays over the top.
     */
    inline float BowlArm(float Time)
    {
        // Release is 14 degrees past vertical where the fast bowler lets go at full extension,
        // pinned to C26Field::ReleasePoseTime (0.62s).
        const float Keys[][2]={
            {0.f,-155.f},{.18f,-110.f},{.34f,-48.f},{.48f,-8.f},
            {.62f,14.f},{.82f,72.f},{1.08f,135.f},{1.40f,170.f},{1.80f,175.f},{2.20f,180.f}};
        const int N=int(UE_ARRAY_COUNT(Keys));
        int I=0;while(I+1<N-1&&Time>Keys[I+1][0])++I;
        const float Span=FMath::Max(1e-4f,Keys[I+1][0]-Keys[I][0]);
        float T=FMath::Clamp((Time-Keys[I][0])/Span,0.f,1.f);
        T=T*T*(3.f-2.f*T);
        return FMath::DegreesToRadians(FMath::Lerp(Keys[I][1],Keys[I+1][1],T));
    }
    /** One foot through one stride: a ground-locked stance, a folded-then-reaching swing, and the
     * ankle pitch that belongs to each moment of it. Pitch is in degrees, positive = toe down
     * (the drive off the ball of the foot), negative = toe up (the heel strike, and the clearance
     * a swing leg needs to miss the turf).
     */
    struct FStride{FVector Foot;float Pitch;};
    inline FStride Stride(float Phase,float Speed,float Side,float Ankle,bool PaceBowler)
    {
        const float Cycle=FMath::Frac(Phase/(2.f*PI)+(Side>0?.5f:0.f));
        const float Span=FMath::Clamp(Speed*.155f,20.f,PaceBowler?112.f:100.f);
        // The support foot travels back relative to the pelvis while remaining on the ground.
        const float Support=.60f;
        const float Drive=PaceBowler?34.f:26.f;
        FStride Out;
        if(Cycle<Support)
        {
            const float T=Cycle/Support;
            Out.Foot=FVector(FMath::Lerp(Span*.5f,-Span*.5f,T),Side,Ankle);
            // Land on the heel, roll flat through mid-stance, leave off the toe.
            Out.Pitch=T<.17f?FMath::Lerp(-12.f,0.f,T/.17f)
                :T<.58f?0.f:FMath::Lerp(0.f,Drive,(T-.58f)/.42f);
        }
        else
        {
            const float T=(Cycle-Support)/(1.f-Support);
            // The swing leg snaps under the hip and then reaches out ahead of it.
            const float Reach=FMath::InterpEaseInOut(0.f,1.f,T,1.65f);
            const float Lift=FMath::Sin(FMath::Pow(T,.82f)*PI)*(PaceBowler?37.f:28.f);
            Out.Foot=FVector(FMath::Lerp(-Span*.5f,Span*.5f,Reach),Side,Ankle+Lift);
            Out.Pitch=T<.24f?FMath::Lerp(Drive,-13.f,T/.24f):FMath::Lerp(-13.f,-9.f,(T-.24f)/.76f);
        }
        return Out;
    }
    inline FVector RunningFoot(float Phase,float Speed,float Side,float Ankle,bool PaceBowler)
    {return Stride(Phase,Speed,Side,Ankle,PaceBowler).Foot; }
    /** What the pelvis does while the legs do the above. */
    struct FCarry{float Bob,Roll,Yaw;};
    inline FCarry Carry(float Phase,float Speed)
    {
        const float Power=FMath::Clamp(Speed/560.f,.22f,1.f);
        FCarry C;
        C.Bob=(FMath::Cos(2.f*Phase)-1.f)*1.9f*Power;
        C.Roll=FMath::Sin(Phase)*3.1f*Power;
        C.Yaw=FMath::Sin(Phase)*7.2f*Power;
        return C;
    }
    /** Resting motion. */
    struct FRest{float Breath,Weight,Sway;};
    inline FRest Rest(float Time,int32 Seed)
    {
        const float Phase=float(Seed%17)*.37f;
        const float Rate=1.f+float(Seed%7)*.021f;
        FRest R;
        R.Breath=FMath::Sin(Time*1.55f*Rate+Phase);
        R.Weight=FMath::Sin(Time*.63f*Rate+Phase*1.7f);
        R.Sway=FMath::Sin(Time*1.7f*Rate+Phase*.5f);
        return R;
    }

    /** Batting shot classification and kinematics. */
    enum class EC26Shot : uint8
    {
        ForwardDefence,
        BackFootDefence,
        CoverDrive,
        StraightDrive,
        OnDrive,
        Pull,
        Hook,
        SquareCut,
        LateCut,
        Sweep,
        Flick,
        LegGlance
    };

    inline EC26Shot ClassifyShot(bool Defending, float ShotAngle, float ContactZ, float StrideIntent, bool Loft)
    {
        if(Defending)
        {
            if(StrideIntent < -0.18f || ContactZ > 85.f)
                return EC26Shot::BackFootDefence;
            return EC26Shot::ForwardDefence;
        }
        // Sweep shot: low ball swept to leg side with forward or neutral stride
        if(ShotAngle < -35.f && ContactZ < 75.f && StrideIntent >= -0.25f)
            return EC26Shot::Sweep;
        // High / short pitch balls: Pull / Hook / Cut
        if(ContactZ > 105.f || StrideIntent < -0.4f)
        {
            if(ShotAngle > 40.f)
                return EC26Shot::SquareCut;
            if(ShotAngle > 0.f)
                return EC26Shot::SquareCut;
            return ContactZ > 145.f ? EC26Shot::Hook : EC26Shot::Pull;
        }
        if(ShotAngle > 78.f)
            return EC26Shot::LateCut;
        if(ShotAngle > 55.f)
            return EC26Shot::SquareCut;
        if(ShotAngle >= 15.f)
            return EC26Shot::CoverDrive;
        if(ShotAngle >= -15.f)
            return EC26Shot::StraightDrive;
        if(ShotAngle >= -34.f)
            return EC26Shot::OnDrive;
        if(ShotAngle < -75.f)
            return EC26Shot::LegGlance;
        if(ContactZ > 70.f)
            return EC26Shot::Pull;
        return EC26Shot::Flick;
    }

    struct FBattingStroke
    {
        EC26Shot Shot;
        FVector Grip;
        FVector Dir;
        FVector LeftFoot;
        FVector RightFoot;
        FVector PoleL;
        FVector PoleR;
        FVector HipShift;
        float Crouch;
        float LeanForward;
        float LeanRight;
        float TurnRight;
        float PitchL;
        float PitchR;
    };

    inline FBattingStroke SolveBattingStroke(
        float ActionTime,
        float ContactTime,
        bool Defending,
        float ShotAngle,
        bool Loft,
        float StrideIntent,
        float FootworkIntent,
        const FVector& Contact,
        float AnkleZ,
        float MiddleDrop)
    {
        const EC26Shot Shot = ClassifyShot(Defending, ShotAngle, Contact.Z, StrideIntent, Loft);
        const float Length = (Shot == EC26Shot::ForwardDefence || Shot == EC26Shot::BackFootDefence) ? 0.62f : (Loft ? 0.82f : 0.72f);
        const float Swing = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(ActionTime / ContactTime, 0.f, 1.f));
        const float Follow = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((ActionTime - ContactTime) / FMath::Max(1e-4f, Length - ContactTime), 0.f, 1.f));
        const float RecoveryT = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((ActionTime - Length) / 0.45f, 0.f, 1.f));

        const float Rad = FMath::DegreesToRadians(ShotAngle);
        const FVector Away = Rig(FMath::Cos(Rad), FMath::Sin(Rad), 0.f).GetSafeNormal();

        FBattingStroke Out;
        Out.Shot = Shot;
        Out.PitchL = 0.f;
        Out.PitchR = 0.f;

        switch(Shot)
        {
        case EC26Shot::ForwardDefence:
        {
            // Front-foot defensive block: solid forward stride, bent knee, head over ball, vertical soft-handed blade
            Out.LeftFoot = Rig(18.f + Swing * 20.f + FootworkIntent * 4.f, -4.f, AnkleZ);
            Out.RightFoot = Rig(-15.f, 12.f, AnkleZ + Swing * 6.f);
            Out.PitchR = Swing * 24.f;
            Out.Crouch = -17.f - Swing * 7.f;
            Out.HipShift = Rig(Swing * 18.f, -Swing * 3.f, 0.f);
            Out.LeanForward = 22.f + Swing * 7.f;
            Out.TurnRight = 44.f - Swing * 6.f;
            Out.LeanRight = 4.f;

            const FVector Lift = Rig(-6.f, 17.f, 116.f);
            const FVector Meet = Contact + Rig(0.f, 0.f, 1.f) * MiddleDrop;
            Out.Grip = FMath::Lerp(Lift, Meet, Swing);
            const FVector Held = Rig(-0.14f, 0.08f, 0.98f).GetSafeNormal();
            Out.Dir = FMath::Lerp(Rig(-0.25f, 0.12f, 0.96f), Held, Swing).GetSafeNormal();

            // High defensive elbow
            Out.PoleL = Rig(0.38f, -0.88f, 0.42f);
            Out.PoleR = Rig(-0.40f, 0.45f, -0.35f);
            break;
        }
        case EC26Shot::BackFootDefence:
        {
            // Back-foot defence: step back and across, stand tall, soft hands absorbing under eyes
            Out.RightFoot = Rig(-18.f - Swing * 4.f, 15.f, AnkleZ);
            Out.LeftFoot = Rig(4.f, -4.f, AnkleZ + Swing * 4.f);
            Out.PitchL = Swing * 18.f;
            Out.Crouch = -9.f;
            Out.HipShift = Rig(-Swing * 8.f, Swing * 6.f, 0.f);
            Out.LeanForward = 12.f;
            Out.TurnRight = 45.f;
            Out.LeanRight = 2.f;

            const FVector Lift = Rig(-10.f, 18.f, 120.f);
            const FVector Meet = Contact + Rig(0.f, 0.f, 1.f) * MiddleDrop;
            Out.Grip = FMath::Lerp(Lift, Meet, Swing);
            Out.Dir = Rig(-0.08f, 0.04f, 0.99f).GetSafeNormal();

            Out.PoleL = Rig(-0.25f, -0.70f, 0.20f);
            Out.PoleR = Rig(-0.25f, 0.70f, 0.20f);
            break;
        }
        case EC26Shot::CoverDrive:
        {
            // Textbook cover drive: big forward-out stride, deep knee bend, chicken-wing high left elbow, flowing high finish
            Out.LeftFoot = Rig(18.f + Swing * 26.f + FootworkIntent * 5.f, -6.f - Swing * 5.f, AnkleZ);
            Out.RightFoot = Rig(-14.f, 11.f, AnkleZ + Swing * 8.f);
            Out.PitchR = Swing * 28.f;
            Out.Crouch = -16.f - Swing * 10.f + Follow * 4.f;
            Out.HipShift = Rig(Swing * 24.f - Follow * 4.f, -Swing * 5.f, 0.f);
            Out.LeanForward = 22.f + Swing * 9.f - Follow * 6.f;
            Out.TurnRight = 46.f - Swing * 16.f - Follow * 8.f;
            Out.LeanRight = 4.f - Swing * 7.f;

            const FVector Lift = Rig(-12.f, 20.f, 124.f);
            const FVector Meet = Contact + Rig(0.f, 0.f, 1.f) * MiddleDrop;
            const FVector Wrap = Rig(18.f, -14.f, 172.f + (Loft ? 16.f : 0.f)) + Away * 16.f;
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.45f, 0.32f, -0.84f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = (-Away * 0.35f + Rig(0.38f, -0.22f, 0.89f)).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            // Signature high left elbow (chicken wing)
            Out.PoleL = Rig(0.45f, -0.95f, 0.48f);
            Out.PoleR = Rig(-0.38f, 0.55f, -0.25f);
            break;
        }
        case EC26Shot::StraightDrive:
        case EC26Shot::OnDrive:
        {
            // Pristine straight drive: step straight down pitch, presentation of full vertical blade, high finish pointing to sight screen
            const bool IsOn = Shot == EC26Shot::OnDrive;
            Out.LeftFoot = Rig(18.f + Swing * 26.f + FootworkIntent * 4.f, IsOn ? -4.f : -1.f, AnkleZ);
            Out.RightFoot = Rig(-14.f, 10.f, AnkleZ + Swing * 7.f);
            Out.PitchR = Swing * 26.f;
            Out.Crouch = -16.f - Swing * 9.f + Follow * 4.f;
            Out.HipShift = Rig(Swing * 22.f - Follow * 3.f, IsOn ? -Swing * 3.f : 0.f, 0.f);
            Out.LeanForward = 24.f + Swing * 7.f - Follow * 5.f;
            Out.TurnRight = 44.f - Swing * (IsOn ? 18.f : 10.f) - Follow * 6.f;
            Out.LeanRight = 3.f;

            const FVector Lift = Rig(-10.f, 18.f, 120.f);
            const FVector Meet = Contact + Rig(0.f, 0.f, 1.f) * MiddleDrop;
            const FVector Wrap = Rig(26.f, IsOn ? -6.f : 0.f, 168.f + (Loft ? 18.f : 0.f));
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.40f, 0.25f, -0.88f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = Rig(0.42f, IsOn ? -0.15f : 0.04f, 0.90f).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            Out.PoleL = Rig(0.38f, -0.80f, 0.52f);
            Out.PoleR = Rig(-0.35f, 0.48f, -0.28f);
            break;
        }
        case EC26Shot::Pull:
        case EC26Shot::Hook:
        {
            // Dynamic swivel pull shot: weight deep on back foot, chest rotating 90 degrees around to square leg, horizontal wrist roll, neck wrap
            Out.RightFoot = Rig(-18.f - Swing * 8.f, 14.f, AnkleZ);
            Out.LeftFoot = Rig(8.f - Swing * 14.f, -10.f, AnkleZ + Swing * 12.f);
            Out.PitchL = Swing * 25.f;
            Out.Crouch = -14.f + Swing * 4.f;
            Out.HipShift = Rig(-Swing * 12.f, Swing * 6.f, 0.f);
            Out.LeanForward = 16.f - Swing * 6.f;
            Out.TurnRight = 48.f - Swing * 55.f - Follow * 28.f;
            Out.LeanRight = 4.f + Swing * 8.f;

            const FVector Lift = Rig(-18.f, 24.f, 138.f);
            const FVector Meet = Contact + Rig(-0.20f, 0.65f, 0.72f).GetSafeNormal() * MiddleDrop;
            const FVector Wrap = Rig(-8.f, -24.f, 150.f);
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.55f, 0.40f, -0.73f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = Rig(-0.65f, -0.50f, 0.57f).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            // Wide horizontal elbows
            Out.PoleL = Rig(-0.25f, -0.85f, 0.20f);
            Out.PoleR = Rig(0.35f, 0.85f, 0.25f);
            break;
        }
        case EC26Shot::Sweep:
        {
            // Low sweeping broom: front stride far forward, back knee drops right down to the turf, low horizontal arc
            Out.LeftFoot = Rig(24.f + Swing * 28.f + FootworkIntent * 4.f, -6.f, AnkleZ);
            Out.RightFoot = Rig(-18.f, 6.f, AnkleZ - 6.f); // Back knee collapses down to turf!
            Out.Crouch = -26.f - Swing * 14.f; // Pelvis drops dramatically!
            Out.HipShift = Rig(Swing * 16.f, -Swing * 4.f, 0.f);
            Out.LeanForward = 25.f + Swing * 12.f;
            Out.TurnRight = 44.f - Swing * 35.f - Follow * 15.f;
            Out.LeanRight = 6.f - Swing * 10.f;

            const FVector Lift = Rig(-14.f, 18.f, 118.f);
            const FVector Meet = Contact + Rig(0.12f, -0.65f, 0.75f).GetSafeNormal() * (MiddleDrop * 0.88f);
            const FVector Wrap = Rig(10.f, -28.f, 88.f);
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.35f, 0.20f, -0.91f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = Rig(0.20f, -0.92f, 0.32f).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            Out.PoleL = Rig(0.55f, -0.75f, -0.20f);
            Out.PoleR = Rig(-0.25f, 0.65f, -0.25f);
            break;
        }
        case EC26Shot::SquareCut:
        case EC26Shot::LateCut:
        {
            // Authoritative cut shot: back foot across to off stump, standing tall, high hands slashing through point
            const bool IsLate = Shot == EC26Shot::LateCut;
            Out.RightFoot = Rig(-15.f, 16.f + Swing * 6.f, AnkleZ);
            Out.LeftFoot = Rig(6.f, 0.f, AnkleZ + Swing * 4.f);
            Out.Crouch = -8.f;
            Out.HipShift = Rig(-Swing * 8.f, Swing * 10.f, 0.f);
            Out.LeanForward = 12.f;
            Out.LeanRight = -8.f - Swing * 7.f;
            Out.TurnRight = 50.f + Swing * (IsLate ? 12.f : 6.f);

            const FVector Lift = Rig(-16.f, 26.f, 138.f);
            const FVector Meet = Contact + (Rig(0.f, 0.f, 0.5f) + Away * 0.65f).GetSafeNormal() * MiddleDrop;
            const FVector Wrap = Rig(18.f, 26.f, 122.f);
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.55f, 0.35f, -0.76f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = Rig(0.35f, 0.85f, 0.38f).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            Out.PoleL = Rig(0.15f, -0.65f, 0.40f);
            Out.PoleR = Rig(0.40f, 0.75f, 0.30f);
            break;
        }
        case EC26Shot::Flick:
        case EC26Shot::LegGlance:
        {
            // Wristy flick / glance: balanced base, vertical swing with supple wrist roll turning blade towards leg side
            Out.LeftFoot = Rig(18.f + Swing * 14.f + FootworkIntent * 3.f, -4.f, AnkleZ);
            Out.RightFoot = Rig(-14.f, 10.f, AnkleZ + Swing * 5.f);
            Out.PitchR = Swing * 20.f;
            Out.Crouch = -16.f - Swing * 6.f;
            Out.HipShift = Rig(Swing * 14.f, -Swing * 2.f, 0.f);
            Out.LeanForward = 20.f + Swing * 5.f;
            Out.TurnRight = 44.f - Swing * 20.f;
            Out.LeanRight = 4.f;

            const FVector Lift = Rig(-8.f, 18.f, 120.f);
            const FVector Meet = Contact + Rig(0.f, 0.f, 1.f) * MiddleDrop;
            const FVector Wrap = Rig(14.f, -22.f, 142.f);
            Out.Grip = Swing < 1.f ? FMath::Lerp(Lift, Meet, Swing) : FMath::Lerp(Meet, Wrap, Follow);

            const FVector Cocked = Rig(-0.35f, 0.20f, -0.91f).GetSafeNormal();
            const FVector Held = (Out.Grip - Contact).GetSafeNormal(UE_SMALL_NUMBER, Rig(0, 0, 1));
            const FVector Through = Rig(0.18f, -0.68f, 0.70f).GetSafeNormal();
            Out.Dir = (Swing < 1.f ? FMath::Lerp(Cocked, Held, Swing) : FMath::Lerp(Held, Through, Follow)).GetSafeNormal();

            Out.PoleL = Rig(0.30f, -0.75f, 0.35f);
            Out.PoleR = Rig(-0.25f, 0.50f, -0.20f);
            break;
        }
        }

        // Seamless poise recovery: if recovering after follow-through, smoothly ease back toward stance
        if(RecoveryT > 0.f)
        {
            const FVector ReadyGrip = Rig(10.f, 14.f, 85.f);
            const FVector ReadyDir = Rig(0.10f, 0.05f, 0.993f).GetSafeNormal();
            const FVector ReadyFL = Rig(17.f + FootworkIntent * 4.f, -4.f + StrideIntent * 3.f, AnkleZ);
            const FVector ReadyFR = Rig(-16.f, 13.f, AnkleZ);

            Out.Grip = FMath::Lerp(Out.Grip, ReadyGrip, RecoveryT);
            Out.Dir = FMath::Lerp(Out.Dir, ReadyDir, RecoveryT).GetSafeNormal();
            Out.LeftFoot = FMath::Lerp(Out.LeftFoot, ReadyFL, RecoveryT);
            Out.RightFoot = FMath::Lerp(Out.RightFoot, ReadyFR, RecoveryT);
            Out.HipShift = FMath::Lerp(Out.HipShift, FVector::ZeroVector, RecoveryT);
            Out.Crouch = FMath::Lerp(Out.Crouch, -19.f, RecoveryT);
            Out.LeanForward = FMath::Lerp(Out.LeanForward, 25.f, RecoveryT);
            Out.TurnRight = FMath::Lerp(Out.TurnRight, 48.f, RecoveryT);
            Out.LeanRight = FMath::Lerp(Out.LeanRight, 5.f, RecoveryT);
            Out.PitchL = FMath::Lerp(Out.PitchL, 0.f, RecoveryT);
            Out.PitchR = FMath::Lerp(Out.PitchR, 0.f, RecoveryT);
        }

        return Out;
    }

    /** Authentic fielding zones, posture classifications, and action kinematics */
    enum class EC26FieldZone : uint8
    {
        Keeper,
        Slip,
        Ring,
        Deep
    };

    struct FFielderPose
    {
        FVector LeftFoot = FVector::ZeroVector;
        FVector RightFoot = FVector::ZeroVector;
        FVector LeftHand = FVector::ZeroVector;
        FVector RightHand = FVector::ZeroVector;
        FVector PoleL = FVector::ZeroVector;
        FVector PoleR = FVector::ZeroVector;
        FVector HipShift = FVector::ZeroVector;
        float Crouch = 0.f;
        float LeanForward = 0.f;
        float LeanRight = 0.f;
        float TurnRight = 0.f;
        float PitchL = 0.f;
        float PitchR = 0.f;
        float FingerCurl = 0.38f;
    };

    inline EC26FieldZone ClassifyFieldZone(EC26Role Role, const FVector& WorldPos)
    {
        if(Role == EC26Role::Keeper) return EC26FieldZone::Keeper;
        const float DistSq2D = WorldPos.X * WorldPos.X + WorldPos.Y * WorldPos.Y;
        // Slips & Gully: behind batsman (positive Y in match frame), close to wicket
        if(WorldPos.Y > 600.f && FMath::Abs(WorldPos.X) < 1800.f && DistSq2D < 3200.f * 3200.f)
        {
            return EC26FieldZone::Slip;
        }
        // Infield Ring: within 38 meters of pitch center
        if(DistSq2D < 3800.f * 3800.f)
        {
            return EC26FieldZone::Ring;
        }
        return EC26FieldZone::Deep;
    }

    /** Authentic, position-aware fielding ready stance (Cricket 24 broadcast inspired).
     * Eliminates stiff T-poses: hands resting naturally within arm reach, elbows relaxed,
     * position-specific crouch and athletic ready weighting.
     */
    inline FFielderPose SolveFielderReady(
        float MotionTime,
        int32 SquadNumber,
        EC26Role Role,
        const FVector& WorldPos,
        float AnkleZ)
    {
        FFielderPose Out;
        const EC26FieldZone Zone = ClassifyFieldZone(Role, WorldPos);
        const float Breath = FMath::Sin(MotionTime * 2.1f + SquadNumber * 1.37f) * 1.5f;
        const float Sway = FMath::Sin(MotionTime * 0.95f + SquadNumber * 2.14f) * 1.8f;

        switch(Zone)
        {
        case EC26FieldZone::Keeper:
        {
            // Low athletic squat behind the stumps, gloves together at shin height
            Out.Crouch = -44.f + Breath * 0.7f;
            Out.LeanForward = 27.f + Breath * 0.4f;
            Out.TurnRight = 0.f;
            Out.LeanRight = Sway * 0.5f;
            Out.LeftFoot = Rig(4.f, -22.f, AnkleZ);
            Out.RightFoot = Rig(4.f, 22.f, AnkleZ);
            Out.LeftHand = Rig(24.f + Sway * 0.4f, -10.f, 48.f + Out.Crouch * 0.2f);
            Out.RightHand = Rig(24.f - Sway * 0.4f, 10.f, 48.f + Out.Crouch * 0.2f);
            Out.PoleL = Rig(0.10f, -0.85f, 0.20f);
            Out.PoleR = Rig(0.10f, 0.85f, 0.20f);
            Out.FingerCurl = 0.25f;
            break;
        }
        case EC26FieldZone::Slip:
        {
            // Deep spring-loaded crouch, ready to react to an edge: hands cupped low in front of knees
            Out.Crouch = -27.f + Breath * 0.6f;
            Out.LeanForward = 30.f + Breath * 0.5f;
            Out.TurnRight = -4.f + Sway * 0.8f;
            Out.LeanRight = Sway * 0.7f;
            Out.LeftFoot = Rig(6.f, -21.f, AnkleZ);
            Out.RightFoot = Rig(-4.f, 21.f, AnkleZ);
            Out.LeftHand = Rig(18.f + Sway * 0.6f, -13.f, 62.f + Out.Crouch * 0.3f);
            Out.RightHand = Rig(18.f - Sway * 0.6f, 13.f, 62.f + Out.Crouch * 0.3f);
            Out.PoleL = Rig(-0.15f, -0.82f, 0.10f);
            Out.PoleR = Rig(-0.15f, 0.82f, 0.10f);
            Out.FingerCurl = 0.30f;
            break;
        }
        case EC26FieldZone::Ring:
        {
            // Athletic ready stance: knees flexed, chest over toes, hands relaxed at thighs/hips
            // Hands positioned comfortably at waist/thigh (Forward=14, Up=78) with soft 35 deg elbow bend
            Out.Crouch = -14.f + Breath * 0.5f;
            Out.LeanForward = 16.f + Breath * 0.4f;
            Out.TurnRight = Sway * 1.2f;
            Out.LeanRight = Sway * 0.8f;
            Out.LeftFoot = Rig(5.f, -18.f, AnkleZ);
            Out.RightFoot = Rig(-5.f, 18.f, AnkleZ);
            Out.LeftHand = Rig(14.f + Sway * 0.8f, -17.f, 78.f + Out.Crouch * 0.4f);
            Out.RightHand = Rig(14.f - Sway * 0.8f, 17.f, 78.f + Out.Crouch * 0.4f);
            Out.PoleL = Rig(-0.30f, -0.85f, -0.22f);
            Out.PoleR = Rig(-0.30f, 0.85f, -0.22f);
            Out.FingerCurl = 0.40f;
            break;
        }
        case EC26FieldZone::Deep:
        {
            // Upright relaxed poise on boundary, hands hanging naturally by sides
            Out.Crouch = -4.f + Breath * 0.4f;
            Out.LeanForward = 7.f;
            Out.TurnRight = Sway * 1.5f;
            Out.LeanRight = Sway * 1.0f;
            Out.LeftFoot = Rig(3.f, -17.f, AnkleZ);
            Out.RightFoot = Rig(-3.f, 17.f, AnkleZ);
            Out.LeftHand = Rig(6.f + Sway * 0.5f, -22.f, 90.f);
            Out.RightHand = Rig(6.f - Sway * 0.5f, 22.f, 90.f);
            Out.PoleL = Rig(-0.35f, -0.80f, -0.32f);
            Out.PoleR = Rig(-0.35f, 0.80f, -0.32f);
            Out.FingerCurl = 0.35f;
            break;
        }
        }
        return Out;
    }

    /** Smooth ground gather / pickup biomechanics */
    inline FFielderPose SolveFielderPickup(
        float ActionTime,
        const FVector& TakeTarget,
        float AnkleZ,
        float ShoulderZ,
        float PalmReach)
    {
        FFielderPose Out;
        const float Gather = FMath::SmoothStep(0.f, 0.20f, ActionTime);
        const float Recover = FMath::SmoothStep(0.24f, 0.53f, ActionTime);
        const float Low = FMath::Clamp((92.f - TakeTarget.Z) / 84.f, 0.f, 1.f) * Gather;

        Out.Crouch = FMath::Lerp(-7.f, -70.f, Low) * (1.f - Recover * 0.62f);
        Out.LeanForward = 18.f + Low * 68.f - Recover * 54.f;
        Out.HipShift = Rig(Low * 10.f * (1.f - Recover), 0.f, 0.f);
        Out.LeftFoot = Rig(26.f + Low * 32.f, -19.f, AnkleZ);
        Out.RightFoot = Rig(-20.f - Low * 6.f, 20.f, AnkleZ);
        Out.PitchR = Low * 18.f;

        FVector SolvedTake = FMath::Lerp(Rig(28.f, 0.f, 95.f), TakeTarget, Gather);
        SolvedTake = FMath::Lerp(SolvedTake, Rig(32.f, 0.f, 116.f), Recover);

        const FVector Anchor = Rig(0.f, 0.f, ShoulderZ + Out.Crouch);
        const FVector Reach = (SolvedTake - Anchor).GetSafeNormal(UE_SMALL_NUMBER, Rig(1.f, 0.f, 0.f));
        const FVector Wrists = SolvedTake - Reach * PalmReach;

        Out.LeftHand = Wrists + Rig(0.f, -5.f, 0.f);
        Out.RightHand = Wrists + Rig(0.f, 5.f, 0.f);
        Out.PoleL = Rig(0.2f, -0.8f, 0.1f);
        Out.PoleR = Rig(0.2f, 0.8f, 0.1f);
        Out.FingerCurl = FMath::Lerp(0.20f, 0.40f, Recover);
        return Out;
    }

    /** Authentic cricket overarm throw: crow-hop stride, torso uncoil, high overarm release, diagonal follow-through wrap */
    inline FFielderPose SolveFielderThrow(
        float ActionTime,
        float AnkleZ,
        float ShoulderZ)
    {
        FFielderPose Out;
        // The throw release is strictly anchored at ActionTime = 0.20s (ThrowClock = 0.73s).
        const float ReleaseTime = 0.20f;
        const float TotalTime = 0.50f;

        if(ActionTime <= ReleaseTime)
        {
            // Phase 1: Crow-hop gather & high overarm windup
            const float P = FMath::Clamp(ActionTime / ReleaseTime, 0.f, 1.f);
            Out.LeftFoot = Rig(12.f + P * 24.f, -16.f, AnkleZ);
            Out.RightFoot = Rig(-18.f + P * 6.f, 18.f, AnkleZ);
            Out.PitchR = P * 24.f; // Back foot driving off toe

            Out.Crouch = -8.f - P * 6.f;
            Out.HipShift = Rig(P * 12.f, 0.f, 0.f);
            Out.TurnRight = FMath::Lerp(-32.f, 16.f, P);
            Out.LeanForward = 8.f + P * 14.f;
            Out.LeanRight = -P * 4.f;

            // Non-throwing left arm sights toward target
            Out.LeftHand = FMath::Lerp(Rig(24.f, -22.f, 126.f), Rig(32.f, -16.f, 138.f), P);
            Out.PoleL = Rig(0.25f, -0.85f, 0.15f);

            // Right arm cocks back and whips up to release point
            const FVector Cocked = Rig(-18.f, 22.f, 142.f);
            const FVector HighRelease = Rig(38.f, 18.f, 212.f);
            Out.RightHand = FMath::Lerp(Cocked, HighRelease, P * P);
            Out.PoleR = FMath::Lerp(Rig(-0.35f, 0.90f, 0.40f), Rig(0.20f, 0.80f, 0.50f), P);
            Out.FingerCurl = FMath::Lerp(0.55f, 0.25f, P);
        }
        else
        {
            // Phase 2: Dynamic follow-through, arm wrapping across left hip, trunk flexion
            const float F = FMath::Clamp((ActionTime - ReleaseTime) / (TotalTime - ReleaseTime), 0.f, 1.f);
            Out.LeftFoot = Rig(36.f, -16.f, AnkleZ);
            Out.RightFoot = Rig(-12.f + F * 46.f, 16.f, AnkleZ); // Right foot swings through
            Out.PitchL = (1.f - F) * 10.f;
            Out.PitchR = F * 18.f;

            Out.Crouch = FMath::Lerp(-14.f, -10.f, F);
            Out.HipShift = Rig(12.f + F * 10.f, 0.f, 0.f);
            Out.TurnRight = FMath::Lerp(16.f, 32.f, F);
            Out.LeanForward = FMath::Lerp(22.f, 38.f, FMath::Sin(F * PI * 0.5f));
            Out.LeanRight = FMath::Lerp(-4.f, 4.f, F);

            // Left arm tucked against left ribs
            Out.LeftHand = Rig(10.f, -18.f, 102.f);
            Out.PoleL = Rig(-0.35f, -0.75f, -0.2f);

            // Right arm sweeps across body down toward left hip
            const FVector HighRelease = Rig(38.f, 18.f, 212.f);
            const FVector FollowFinish = Rig(16.f, -22.f, 74.f);
            Out.RightHand = FMath::Lerp(HighRelease, FollowFinish, FMath::SmoothStep(0.f, 1.f, F));
            Out.PoleR = Rig(0.40f, 0.60f, -0.30f);
            Out.FingerCurl = 0.45f;
        }

        return Out;
    }

    /** Authentic catching biomechanics with impact recoil */
    inline FFielderPose SolveFielderCatch(
        float ActionTime,
        const FVector& TakeTarget,
        float AnkleZ,
        float ShoulderZ,
        float PalmReach)
    {
        FFielderPose Out;
        const float Gather = FMath::SmoothStep(0.f, 0.18f, ActionTime);
        const float Recover = FMath::SmoothStep(0.18f, 0.42f, ActionTime);

        Out.Crouch = TakeTarget.Z < 60.f ? -62.f : TakeTarget.Z < 100.f ? -36.f : -8.f;
        Out.LeanForward = TakeTarget.Z < 80.f ? 42.f : 12.f;
        Out.LeftFoot = Rig(8.f, -23.f, AnkleZ);
        Out.RightFoot = Rig(-5.f, 23.f, AnkleZ);

        FVector SolvedTake = FMath::Lerp(Rig(28.f, 0.f, 95.f), TakeTarget, Gather);
        SolvedTake = FMath::Lerp(SolvedTake, Rig(32.f, 0.f, 116.f), Recover);

        const FVector Anchor = Rig(0.f, 0.f, ShoulderZ + Out.Crouch);
        const FVector Reach = (SolvedTake - Anchor).GetSafeNormal(UE_SMALL_NUMBER, Rig(1.f, 0.f, 0.f));
        const FVector Wrists = SolvedTake - Reach * PalmReach;

        Out.LeftHand = Wrists + Rig(0.f, -5.f, 0.f);
        Out.RightHand = Wrists + Rig(0.f, 5.f, 0.f);
        Out.PoleL = Rig(0.1f, -0.8f, 0.2f);
        Out.PoleR = Rig(0.1f, 0.8f, 0.2f);
        Out.FingerCurl = FMath::Lerp(0.20f, 0.40f, Recover);
        return Out;
    }
}
