#pragma once
#include "CoreMinimal.h"
#include "C26Types.generated.h"

UENUM(BlueprintType)
enum class EC26Phase : uint8 { Menu, Intro, Ready, RunUp, Delivery, InPlay, Reaction, Replay, Interval, Result };
UENUM(BlueprintType)
enum class EC26Delivery : uint8 { Pace, Yorker, Bouncer, Slower, Outswing, Inswing };
UENUM(BlueprintType)
enum class EC26Timing : uint8 { Perfect, Good, Early, Late, Edge, Miss };
UENUM(BlueprintType)
enum class EC26Action : uint8 { Ready, Batting, Bowling, Running, Pickup, Throw, Catch, Celebrate, Disappointed, SignalFour, SignalSix, SignalOut, SignalWide };
UENUM(BlueprintType)
enum class EC26Role : uint8 { Batter, Bowler, Fielder, Keeper, Umpire };

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
    constexpr float RunUpDuration = 3.05f;
    constexpr float ReleasePoseTime = .31226f;
    constexpr float BatContactPoseTime = .24f;
    constexpr float RadiusX = 6550.f;
    constexpr float RadiusY = 7200.f;
    inline FVector RopePoint(float A) { return FVector(RadiusX*FMath::Cos(A),RadiusY*FMath::Sin(A),4.5f); }
    inline bool Inside(const FVector& P) { return FMath::Square(P.X/RadiusX)+FMath::Square(P.Y/RadiusY)<1.f; }
}
DECLARE_LOG_CATEGORY_EXTERN(LogC26, Log, All);
