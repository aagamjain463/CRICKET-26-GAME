#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "C26Types.h"
#include "C26Simulation.h"
#include "C26MatchGameMode.generated.h"
class AC26Athlete;
class AC26Stadium;
class AC26CameraDirector;
class AC26Effects;
class UC26Audio;
class UC26Settings;
class UStaticMeshComponent;
DECLARE_MULTICAST_DELEGATE(FOnC26MatchChanged);

UCLASS()
class CRICKETGAME_API AC26MatchGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AC26MatchGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, Category="Cricket Tuning") FC26Tuning Tuning;
    UPROPERTY(BlueprintReadOnly) EC26Phase Phase=EC26Phase::Menu;
    UPROPERTY() TObjectPtr<UC26Settings> Preferences;
    UPROPERTY() TObjectPtr<UC26Audio> Audio;
    UPROPERTY() TObjectPtr<AC26CameraDirector> Director;
    UPROPERTY() TObjectPtr<AC26Stadium> Venue;
    UPROPERTY() TObjectPtr<AC26Effects> Effects;
    UPROPERTY() TArray<TObjectPtr<AC26Athlete>> Athletes;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BallMesh;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Stumps;
    C26::Match Rules;
    FC26Simulation Simulation;
    FC26AI AI;
    FC26ShotIntent Intent;
    FC26DeliveryPlan Bowling;
    FC26Contact LastContact;
    FOnC26MatchChanged OnMatchChanged;
    FString Callout,Detail,TossText;
    float PhaseTime=0,Clock=0,ReleaseQuality=.5f;
    int PlayerTeam=0,FirstBattingTeam=0;
    bool PlayerBatsFirst=true,UseToss=false,SettingsOpen=false,ControlsOpen=false,Paused=false;
    bool Running=false,Returning=false,ReleaseLocked=false,ShotQueued=false,AutoPlay=false;
    float RunProgress=0,Footwork=0;
    int CompletedRuns=0,RequestedRuns=0;
    bool PlayerBatting() const;
    int BattingTeam() const;
    FString TeamName(int Team) const;
    FString TeamShort(int Team) const;
    FString BatterName() const;
    FString BowlerName() const;
    void StartMatch();
    void Menu();
    void UIAction(FName Action);
    void StartDelivery();
    void BowlRelease();
    void Shot(const FC26ShotIntent& NewIntent);
    void Run();
    void CancelRun();
    void Skip();
    void AimPitch(float Line,float Length);
    void DebugOutcome(FString Type);
    float TimingCountdown() const;
    float BowlingMeter() const;
private:
    C26::DeliveryOutcome Pending;
    uint32 DeliveryId=0;
    int ActiveFielder=-1,BackupFielder=-1,RunnerAId=0,RunnerBId=1;
    float RunVelocity=0,CatchClock=-1;
    bool ThrowReleased=false;
    TArray<FC26BallState> FieldForecast;
    float ShotInputTime=0,AITiming=0,FieldDecisionClock=0,ThrowClock=-1,ThrowDuration=0;
    FVector Intercept,ThrowFrom,ThrowTo,RunFromA,RunFromB,RunToA,RunToB;
    int ThrowRunner=0;
    bool Important=false,Resolved=false,Smoke=false,Capture=false;
    int SmokeMatches=0,SmokeBoundaries=0,SmokeWickets=0,SmokeReplays=0,SmokeExtras=0;
    float SmokeWatchdog=0;
    int CaptureIndex=0;
    float CaptureHold=0,CaptureWait=0;
    FString ProbeName;
    bool ProbeCaptured=false;
    void UpdateCapture(float Dt);
    TArray<FVector> FieldPositions;
    void ChangePhase(EC26Phase NewPhase);
    void PrepareDelivery();
    void ReleaseBall();
    void UpdateDelivery(float Dt);
    void UpdateFielding(float Dt);
    void UpdateRunning(float Dt);
    void Collect(int Fielder,bool Catch);
    void Resolve();
    void AfterPresentation();
    void BuildMatchActors();
    void UpdateBallVisual();
    void BreakWicket(float Y);
    void ResetStumps();
    void Haptic(float Strength);
    /** Dust and turf response for one ball's worth of contact events. */
    void Spark(const FVector& At,bool Struck);
    bool FootPlanted=false;
};
