#pragma once
#include "CoreMinimal.h"
#include "C26Types.generated.h"

class UCurveFloat;

UENUM(BlueprintType)
enum class EC26Phase : uint8 { Menu, Intro, Ready, RunUp, Delivery, InPlay, Reaction, Replay, Presentation, Interval, Result };
// Delivery library. The first six values are the original set and keep their
// indices; the variations are appended so saved plans stay valid.
UENUM(BlueprintType)
enum class EC26Delivery : uint8
{
    Pace, Yorker, Bouncer, Slower, Outswing, Inswing,
    // ---- seam / swing variations ----
    ReverseOut, ReverseIn, OffCutter, LegCutter, SlowerCutter,
    // ---- finger spin ----
    OffBreak, ArmBall, TopSpinner, Doosra,
    // ---- wrist spin ----
    LegBreak, Googly, Flipper,
    MAX_None UMETA(Hidden)
};

// How a delivery moves. Swing acts in the air BEFORE the bounce; seam and spin
// act off the pitch AFTER it. Keeping them apart is what stops a cutter from
// being an in-air swinger wearing a different name.
UENUM(BlueprintType)
enum class EC26Movement : uint8 { None, Swing, ReverseSwing, Seam, Spin };

// Bowler archetypes gate which deliveries are offered at all.
UENUM(BlueprintType)
enum class EC26BowlerKind : uint8 { Fast, FastMedium, Medium, OffSpin, LegSpin };
UENUM(BlueprintType)
enum class EC26Timing : uint8 { Perfect, Good, Early, Late, Edge, Miss };
UENUM(BlueprintType)
enum class EC26Action : uint8 { Ready, Batting, Bowling, Running, Pickup, Throw, Catch, Celebrate, Disappointed, SignalFour, SignalSix, SignalOut, SignalWide, Dive, Slide, BatRaise, GloveTap, Handshake, FistPump, Discuss, TossFlip };
UENUM(BlueprintType)
enum class EC26Role : uint8 { Batter, Bowler, Fielder, Keeper, Umpire };

// Control scheme architecture: GesturePro (touch/mouse pull-and-release) vs Legacy (button tap)
UENUM(BlueprintType)
enum class EC26ControlScheme : uint8 { GesturePro, Legacy };

// Tactical fielding presets
UENUM(BlueprintType)
enum class EC26FieldPreset : uint8
{
    Balanced,
    Attacking,
    Defensive,
    PowerplayAttack,
    PowerplayDefensive,
    PaceAttack,
    SpinAttack,
    OffsideHeavy,
    LegsideHeavy,
    DeathOvers,
    ProtectBoundary,
    SinglePrevention
};

// Fielding state machine
UENUM(BlueprintType)
enum class EC26FieldingState : uint8
{
    Idle,
    BallInPlay,
    Chasing,
    Intercepting,
    Gathering,
    ThrowPreparing,
    Throwing,
    CatchOpportunity,
    CatchAttempt,
    DiveAttempt,
    Recovery
};

// Manual throw target selection
UENUM(BlueprintType)
enum class EC26ThrowTarget : uint8
{
    BowlersEnd,
    KeepersEnd
};

// Catch timing classification
UENUM(BlueprintType)
enum class EC26CatchTiming : uint8
{
    None,
    Perfect,
    Good,
    Early,
    Late,
    Missed
};

// Explicit batting gesture state machine. One pointer-down/up cycle walks
// Idle -> ReadyForShot -> Pulling -> Armed -> Released -> ShotCommitted and
// produces exactly ONE batting attempt; anything else cancels back to Idle.
UENUM(BlueprintType)
enum class EC26BattingState : uint8
{
    Idle,           // between deliveries: no batting input accepted
    ReadyForShot,   // run-up has begun, gesture zone is live, nothing pressed
    Pulling,        // pointer is down but still inside the dead zone
    Armed,          // pointer is down outside the dead zone: direction + power live
    Released,       // pointer lifted this frame; the shot has been handed over
    ShotCommitted,  // the simulation owns the attempt
    Contact,        // bat/ball resolution window
    FollowThrough,
    Recovery
};

// Release-timing bands. Derived once per shot from the SAME millisecond delta
// the simulation is given, so the meter can never disagree with the result.
UENUM(BlueprintType)
enum class EC26ReleaseTiming : uint8 { VeryEarly, Early, Good, Perfect, Late, VeryLate, NoShot };

// Explicit bowling state machine. Planning is free-form until START RUN-UP;
// from there the plan is locked and only the release meter is live.
UENUM(BlueprintType)
enum class EC26BowlingState : uint8
{
    Idle,
    Planning,          // delivery / target / movement / pace are all editable
    TargetDrag,        // a pointer is dragging the pitch target
    MovementDrag,      // a pointer is setting swing direction + amount
    PaceDrag,          // a pointer is setting effort
    Ready,             // plan complete, waiting for START RUN-UP
    RunUp,             // approach; the release bar is filling
    ReleaseWindow,     // the marker is inside the live part of the bar
    Released,          // release committed, ball leaving the hand
    BallInFlight,
    Completed
};

// Release-bar bands, ordered along the bar. NO BALL sits directly beside
// PERFECT: the closer you release to the edge, the better the execution and the
// worse the consequence of overshooting.
UENUM(BlueprintType)
enum class EC26ReleaseBand : uint8 { TooEarly, Early, Good, Perfect, NoBall };

// Delivery length recognition categories
UENUM(BlueprintType)
enum class EC26DeliveryLength : uint8 { Yorker, Full, GoodLength, Short, Bouncer };

// Delivery line recognition categories
UENUM(BlueprintType)
enum class EC26DeliveryLine : uint8 { WideOff, OutsideOff, OffStump, MiddleStump, LegStump, DownLeg };

// Delivery history record for pitch map and tactical bowling analysis
USTRUCT(BlueprintType)
struct FC26DeliveryRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector IntendedPitch = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ActualPitch = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedKph = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26Delivery DeliveryType = EC26Delivery::Pace;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RunsConceded = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWicket = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBoundary = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDot = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bExtra = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString OutcomeText;
};

USTRUCT(BlueprintType)
struct FC26DeliveryPlan
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26Delivery Type = EC26Delivery::Pace;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Line = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Length = 460;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 3250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Swing = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Seam = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Bounce = .56f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool NoBall = false;
    /** Seconds after release before lateral air movement begins. Conventional
        swing starts immediately; reverse swing starts late, which is exactly
        what makes it hard to play. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SwingOnset = 0.f;
    /** Post-bounce lateral acceleration, cm/s^2. Positive deviates toward the
        off side. This is what a cutter or a spinner does; Swing never is. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Deviation = 0.f;
    /** What the batter should read this as, for HUD and commentary. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26Movement Movement = EC26Movement::None;
};

/**
 * The bowler's INTENT, authored entirely by the player before the run-up.
 * FC26DeliveryPlan is the executed result; this is the plan it is composed from.
 * Everything here is continuous - presets only write into it.
 */
USTRUCT(BlueprintType)
struct FC26BowlingPlan
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26Delivery Type = EC26Delivery::Pace;
    /** Intended pitch point, world cm. Line: - = off side of a right-hander. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TargetLine = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TargetLength = 460.f;
    /** Requested movement direction, batter-relative: +1 away to the off side,
        -1 in toward the pads. Mirrored for handedness when it is executed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MovementDirection = 1.f;
    /** How much movement is being ATTEMPTED, 0..1. Skill decides what lands. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MovementMagnitude = 0.55f;
    /** Requested pace as a fraction of this bowler's own range, 0..1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PaceNormalized = 0.62f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAroundWicket = false;
};

/** Per-bowler ability and speed range. Intent is free; execution is not. */
USTRUCT(BlueprintType)
struct FC26BowlerProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26BowlerKind Kind = EC26BowlerKind::FastMedium;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLeftArm = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinSpeedKph = 124.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSpeedKph = 146.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = 0.72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SwingSkill = 0.70f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReverseSkill = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CutterSkill = 0.60f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SlowerSkill = 0.62f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpinSkill = 0.20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Control = 0.70f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stamina = 1.0f;
};

/**
 * Release-bar geometry, all as fractions of the bar, all tunable. The bands run
 * TooEarly -> Early -> Good -> Perfect -> NoBall in order, so PerfectEnd is
 * literally the no-ball line: releasing at 0.999 of Perfect is the best
 * delivery available and 1.001 is an overstep.
 */
USTRUCT(BlueprintType)
struct FC26ReleaseBar
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float EarlyStart = 0.42f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GoodStart = 0.62f;
    /** Deliberately narrow: at Normal the live bar runs to a no-ball line of
        0.957, so 0.90 leaves the PERFECT band 5.7% of the bar wide. A smaller
        Perfect band is the difficulty knob - the top reward is a tighter target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PerfectStart = 0.90f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoBallStart = 0.945f;
    /** Widen (Easy) or narrow (Hard) Good+Perfect around the no-ball line. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DifficultyWidth = 1.f;
};

/** Data-driven bowling tuning. One place, not scattered through the code. */
USTRUCT(BlueprintType)
struct FC26BowlingTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FC26ReleaseBar Bar;
    /** Peak in-air lateral acceleration at magnitude 1 and full skill, cm/s^2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSwingAccel = 230.f;
    /** Peak in-air lateral acceleration for a REVERSE swing, cm/s^2. Much larger
        than MaxSwingAccel because reverse swing only starts working late in the
        flight, so its displacement integrates over a fraction of the window a
        conventional swing gets. Tuned so a full-effort reverse swing bends the
        ball about as far as a full-effort conventional one - the difference is
        WHEN it bends, which is the whole point of the delivery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxReverseAccel = 1080.f;
    /** Peak post-bounce lateral acceleration for a cutter, cm/s^2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxCutterAccel = 900.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSpinAccel = 1500.f;
    /** Fraction of THIS delivery's flight that passes before reverse swing
        starts working. It is a fraction, not a number of seconds: a fraction
        means "the last third of the ball's journey" at any pace and any length,
        which is what makes reverse swing late for every delivery rather than
        for one particular speed. FC26Simulation resolves it against the real
        flight time at release. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReverseOnsetFrac = 0.45f;
    /** Line/length scatter, cm, at zero release quality and zero accuracy. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxLineError = 62.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxLengthError = 96.f;
    /** Extra scatter for bowling at the top of the speed range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HighEffortErrorPenalty = 0.55f;
    /** Pace lost to a ragged release, as a fraction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxPaceLoss = 0.16f;
    /** Overs of ball age before reverse swing becomes available. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReverseBallAgeBalls = 4.f;
    /** Development override so reverse can be tested from ball one. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowReverseAlways = false;
    /** Lateral shift of the release point when bowling AROUND the wicket, cm.
        Over the wicket is the neutral position; around swings the release across
        the stumps, which changes the angle of attack and the shape of the
        trajectory without moving where the ball is aimed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AroundWicketOffsetCm = 62.f;
};

USTRUCT(BlueprintType)
struct FC26ShotIntent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Angle = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power = .72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Footwork = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stride = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Loft = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Defend = false;
};

USTRUCT(BlueprintType)
struct FC26Tuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BallRadius = 3.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Gravity = 981.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GrassDeceleration = 145.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FielderSpeed = 710.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RunnerSpeed = 690.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PerfectWindow = .025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GoodWindow = .083f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ContactWindow = .19f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimingAssist = .045f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BatPower = 1.f;
};

// Data-driven gesture tuning and prediction parameters
USTRUCT(BlueprintType)
struct FC26GestureTuning
{
    GENERATED_BODY()
    // ---- pull geometry (1600x900 design units, resolution independent) ----
    /** Movement below this radius is treated as an accidental twitch: no direction, no power. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GestureDeadZone = 16.f;
    /** Pull distance that reads as full commitment. The finger may travel further; magnitude stays <= 1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GestureMaxRadius = 210.f;
    /** Scales raw finger travel before normalisation. >1 = a shorter drag reaches full power. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PullSensitivity = 1.0f;
    /** Angular gain applied to the pull direction before it becomes an aim angle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DirectionSensitivity = 1.0f;
    /** Normalised magnitude below which a release is a controlled push rather than a stroke. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinimumCommitMagnitude = 0.06f;
    /** Optional aggression curve: X = normalised pull 0..1, Y = aggression 0..1. Null uses the built-in bands. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UCurveFloat> MagnitudeCurve = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float InputBufferSeconds = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinPower = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxPower = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BowlingMaxPullDistance = 180.f;

    // ---- batting gesture region, design units ----
    /** Floating-origin touch region. The origin appears wherever the finger lands inside it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D GestureZoneMin = FVector2D(430.f, 168.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D GestureZoneMax = FVector2D(1584.f, 884.f);

    // ---- release-timing windows (ms) before difficulty scaling ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PerfectWindowMs = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GoodWindowMs = 83.f;
    /** Beyond this the release reads VERY EARLY / VERY LATE rather than EARLY / LATE.
        Must stay below Tuning.ContactWindow (190 ms) or those two bands can never occur. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VeryEarlyLateMs = 130.f;

    // ---- bounce marker presentation ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MarkerOpacity = 0.92f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MarkerScale = 1.0f;
    /** Seconds the marker takes to fade out AFTER the ball has bounced. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MarkerFadeDuration = 0.30f;
    /** Fraction of the flight over which the intended point is corrected to the real bounce point. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MarkerPredictionBlendSpeed = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MarkerUncertainty = 0.f;

    // ---- gesture intent extras ----
    /** Tiny pull = soft defensive hands. Design units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefendPullMax = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxAimAngle = 135.f;
};

// Projected bounce indicator data
USTRUCT(BlueprintType)
struct FC26BouncePrediction
{
    GENERATED_BODY()
    /** Where the bowler's LOCKED plan intends to pitch. Known before the ball is released. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector IntendedLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TrueLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector DisplayLocation = FVector::ZeroVector;
    /** Displayed point at the instant the ball was released; the correction lerps away from it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector CorrectionStart = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BounceTime = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AppearTime = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FadeStartTime = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FadeEndTime = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float UncertaintyRadius = 16.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26DeliveryLength LengthCategory = EC26DeliveryLength::GoodLength;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EC26DeliveryLine LineCategory = EC26DeliveryLine::OffStump;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVisible = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Alpha = 0.f;
    /** True while the marker still shows the bowler's INTENT rather than the measured trajectory. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFromIntent = true;
    /** Latched by the real ball-bounce event; the only thing that starts the fade. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBounced = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FadeClock = 0.f;
    /** Contact time of the locked plan, projected at run-up start so timing exists before release. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectedContactTime = 0.5f;
};

namespace C26Field
{
    // Authoritative physical reference: centimetres, Z=0 is the playable turf.
    // Athlete rig import correction is 0.48 (about 182 cm); never inflate actors.
    constexpr float SurfaceZ = 0.f;
    constexpr float PitchWidth = 305.f;
    constexpr float PitchStripLength = 2360.f;
    constexpr float StumpHeight = 71.1f;
    constexpr float WicketWidth = 22.86f;
    constexpr float StumpDiameter = 3.8f;
    constexpr float BallDiameter = 7.2f;
    constexpr float BatLength = 83.f;
    constexpr float WicketY = 1006.f;
    constexpr float CreaseY = 884.f;
    constexpr float ContactY = 848.f;
    constexpr float RunUpDuration = 3.25f;
    constexpr float ReleasePoseTime = .62f;
    constexpr float BatContactPoseTime = .24f;
    constexpr float RadiusX = 6550.f;
    constexpr float RadiusY = 7200.f;
    constexpr float InnerCircleRadius = 2740.f;
    inline FVector RopePoint(float A) { return FVector(RadiusX*FMath::Cos(A),RadiusY*FMath::Sin(A),4.5f); }
    inline bool Inside(const FVector& P) { return FMath::Square(P.X/RadiusX)+FMath::Square(P.Y/RadiusY)<1.f; }
    inline bool InsideInnerCircle(const FVector& P) { return (P.X * P.X + P.Y * P.Y) <= (InnerCircleRadius * InnerCircleRadius); }
}
inline bool C26ValidTransition(EC26Phase From,EC26Phase To)
{
    if(From==To||To==EC26Phase::Menu)return true;
    switch(From)
    {
    case EC26Phase::Intro:return To==EC26Phase::Ready||To==EC26Phase::Presentation;
    case EC26Phase::Ready:return To==EC26Phase::RunUp||To==EC26Phase::Presentation;
    case EC26Phase::RunUp:return To==EC26Phase::Delivery;
    case EC26Phase::Delivery:return To==EC26Phase::InPlay||To==EC26Phase::Reaction;
    case EC26Phase::InPlay:return To==EC26Phase::Reaction;
    case EC26Phase::Reaction:return To==EC26Phase::Replay||To==EC26Phase::Presentation||To==EC26Phase::Ready||To==EC26Phase::Interval||To==EC26Phase::Result;
    case EC26Phase::Replay:return To==EC26Phase::Presentation||To==EC26Phase::Ready||To==EC26Phase::Interval||To==EC26Phase::Result;
    case EC26Phase::Presentation:return To==EC26Phase::Ready||To==EC26Phase::Interval||To==EC26Phase::Result||To==EC26Phase::Presentation||To==EC26Phase::Reaction||To==EC26Phase::Replay;
    case EC26Phase::Interval:return To==EC26Phase::Ready||To==EC26Phase::Presentation;
    case EC26Phase::Result:return To==EC26Phase::Presentation;
    default:return false;
    }
}
DECLARE_LOG_CATEGORY_EXTERN(LogC26, Log, All);
