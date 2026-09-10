#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "C26Types.h"
#include "C26Simulation.h"
#include "C26Commentary.h"
#include "C26MatchGameMode.generated.h"
class AC26Athlete;
class AC26Stadium;
class AC26CameraDirector;
class AC26Effects;
class UC26Audio;
class UC26Settings;
class UStaticMeshComponent;
DECLARE_MULTICAST_DELEGATE(FOnC26MatchChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnC26CricketEvent,FName,const FVector&);

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
    FC26DeliveryPlan LockedBowling;
    FC26Contact LastContact;
    FOnC26MatchChanged OnMatchChanged;
    FOnC26CricketEvent OnCricketEvent;
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
    // ---- front-end flow (presentation only; gameplay truth untouched) ----
    // MenuScreen: 0 Home,1 Play,2 Teams,3 Matchup,4 Toss,5 Squad,6 Career,
    // 7 Leaderboards,8 Multiplayer,9 Training,10 World,11 Settings,12 Help,13 Store.
    // Reference sidebar surfaces 0,5,6,7,8,13,11; 1-4 are the Play sub-flow; 9-10 are
    // orphaned roadmap screens (still reachable by UIAction, no nav button).
    int MenuScreen=0;
    float ScreenEnteredAt=0,ScreenFade=1;
    int TossStage=0; float TossClock=0;
    bool TossPlayerWon=false,TossPlayerChoseBat=true,TossResolved=false,TossAIChoiceBat=true;
    FString ResolvedTossText;
    FName PendingConfirm=NAME_None;
    FString ToastText; float ToastUntil=0;
    int SettingsTab=0;
    FName LastAction=NAME_None; float LastActionAt=-99.f;
    void SetScreen(int S);
    void GoBack();
    void Toast(const FString& S);
private:
    C26::DeliveryOutcome Pending;
    uint32 DeliveryId=0;
    int ActiveFielder=-1,BackupFielder=-1,RunnerAId=0,RunnerBId=1;
    float RunVelocity=0,CatchClock=-1;
    bool ThrowReleased=false;
    bool BallReleased=false,ResettingMatch=false,KeeperTake=false;
    float KeeperTakeClock=-1.f,MissTakeAge=0.f;
    FVector MissTakeTarget=FVector::ZeroVector,GatherPoint=FVector::ZeroVector;
    float BrokenWicketY=0.f,StumpClock=-1.f;
    TArray<FC26BallState> FieldForecast;
    float ShotInputTime=0,AITiming=0,FieldDecisionClock=0,ThrowClock=-1,ThrowDuration=0;
    FVector Intercept,ThrowFrom,ThrowTo,RunFromA,RunFromB,RunToA,RunToB;
    int ThrowRunner=0;
    bool Important=false,Resolved=false,Smoke=false,Capture=false;
    int SmokeMatches=0,SmokeBoundaries=0,SmokeWickets=0,SmokeReplays=0,SmokeExtras=0;
    float SmokeWatchdog=0;
    int CaptureIndex=0;
    float CaptureHold=0,CaptureWait=0;
    bool CaptureShotsNavFired=false;
    bool ClearPauseNext=false;
    FString ProbeName;
    bool ProbeCaptured=false;
    void UpdateCapture(float Dt);
    TArray<FVector> FieldPositions;
    /** How far the ring has walked in with the bowler this delivery, in centimetres. Reset with
        the rest of the transient delivery state; the set positions themselves never move. */
    float FieldCreep=0.f;
    /** Fielders walk in as the bowler runs in and turn to follow the ball once it is struck.
        Without it nine of the eleven players stand perfectly still through every delivery, which
        is the single loudest tell that a cricket scene is a prototype. */
    void UpdateFieldPresence(float Dt);
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
    FC26CommentaryContext MakeCommentaryContext() const;
    /** Dust and turf response for one ball's worth of contact events. */
    void Spark(const FVector& At,bool Struck);
    /** Momentary time pinch on a well-struck ball, and the real-time stamp it ends at. */
    void HitStop(float Quality);
    void ClearHitStop();
    double HitStopUntil=0;
    bool FootPlanted=false;
    /** Last bowler Y that produced a footstep; distance-based so steps match stride, not the clock. */
    float LastStepY=0;
    /** -C26Debug: persistent release/contact markers plus a ball trail. Dev only, never in shipping. */
    bool bDebugTrace=false;
    FVector DebugPrevBall=FVector::ZeroVector;
#if !UE_BUILD_SHIPPING
    // Opt-in integration probe: uses real input commands, simulation, fielding and scoring.
    void UpdateGoldenGate(float Dt);
    bool GoldenGate=false,GateCollected=false,GateThrown=false,GateNoScreens=false,GateSuite=false;
    bool GateSawFour=false,GateSawSix=false,GateSawWicket=false;
    int GateMatches=0;
    void UpdateProductionGate(float Dt);
    int GateStage=0,GateFailures=0;
    uint32 GateEpoch=0;
    double GateStarted=0;
    FString GateDirectory;
    TSet<FString> GateShots;
    TArray<float> GateFrameTimes;
#endif
};
