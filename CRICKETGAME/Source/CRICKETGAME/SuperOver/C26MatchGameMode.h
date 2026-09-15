#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "C26Types.h"
#include "C26FieldingSystem.h"
#include "C26Simulation.h"
#include "C26Commentary.h"
#include "C26CommentaryTypes.h"
#include "C26MatchGameMode.generated.h"
class AC26Athlete;
class AC26Stadium;
class AC26CameraDirector;
class AC26Effects;
class UC26Audio;
class UC26CommentaryDirector;
class UC26PresentationDirector;
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
    UPROPERTY() TObjectPtr<UC26CommentaryDirector> CommentaryDirector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UC26PresentationDirector> PresentationDirector;
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
    FString NonStrikerName() const;
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
    /** Hands the queued stroke's predicted contact to the striker's presentation (never to the simulation). */
    void CommitStrokeContact();
    /** Presentation position of a ball in a fielder's hands between the gather and the throw. */
    FVector HeldBallPosition() const;
    float BowlingMeter() const;

    // ---- Major Gameplay Control Overhaul: Gesture Batting & Bowling ----
    EC26BattingState BattingState = EC26BattingState::Idle;
    EC26BowlingState BowlingState = EC26BowlingState::Idle;
    FC26BouncePrediction BouncePrediction;
    UPROPERTY(EditAnywhere, Category="Cricket Tuning") FC26GestureTuning GestureTuning;

    // ---- batting gesture: one pointer, one attempt -------------------------
    /** Pointer/finger that owns the current gesture. -1 = none. Every other pointer is ignored. */
    int32 GesturePointerId = -1;
    /** Delivery the live gesture belongs to; a stale gesture can never commit a shot on the next ball. */
    uint32 GestureDeliveryId = 0;
    bool bBattingGestureActive = false;
    bool bGestureArmed = false;
    FVector2D BattingGestureStart = FVector2D::ZeroVector;
    FVector2D BattingGestureCurrent = FVector2D::ZeroVector;
    /** Unclamped finger travel, design units. */
    FVector2D BattingPullRaw = FVector2D::ZeroVector;
    double GestureStartRealTime = 0.0;
    double GestureReleaseRealTime = 0.0;
    float BattingGestureHoldTime = 0.f;
    /** Raw pull length in design units, before dead zone and clamp. */
    float BattingPullRawMagnitude = 0.f;
    /** Dead-zone corrected, clamped 0..1. */
    float BattingPullFrac = 0.f;
    /** BattingPullFrac through the aggression curve, 0..1. */
    float BattingAggression = 0.f;
    /** Screen-space pull angle in degrees, 0 = straight up the screen. */
    float BattingPullScreenAngle = 0.f;
    /** Batter-relative aim angle: + = off side, - = leg side, already mirrored for left-handers. */
    float BattingGestureAngle = 0.f;
    float BattingGesturePower = 0.72f;
    float BattingSuitability = 1.f;
    float BattingStrideAuto = 0.f;
    bool bFootworkManual = false;
    /** Live shot the current gesture would play, named by the shared classifier. */
    FString BattingShotCandidate;

    // ---- release timing: one delta, shared by gameplay and the meter --------
    float BattingReleaseDeltaMs = 0.f;
    EC26ReleaseTiming BattingReleaseTiming = EC26ReleaseTiming::NoShot;
    EC26Timing BattingTimingLabel = EC26Timing::Miss;
    float BattingTimingQuality = 0.f;
    /** Seconds of timing error handed to FC26Simulation::Hit; survives a run-up release. */
    float PendingShotError = 0.f;
    bool bPendingShotFromGesture = false;
    float BattingFeedbackUntil = -1.f;
    float BattingCommitAge = -1.f;
    bool bLeftHandedBatter = false;
    /** Last reason a gesture was thrown away, for the debug overlay. */
    FString GestureCancelReason;
    int32 GestureCommitCount = 0;

    // ---- bowling: the player authors the plan, the model executes it -------
    /** Everything the player chooses before the run-up. Persists between balls. */
    UPROPERTY(EditAnywhere, Category="Cricket Tuning") FC26BowlingPlan BowlingPlan;
    /** The current bowler's range and ability; intent is free, execution is not. */
    UPROPERTY(EditAnywhere, Category="Cricket Tuning") FC26BowlerProfile BowlerProfile;
    UPROPERTY(EditAnywhere, Category="Cricket Tuning") FC26BowlingTuning BowlingTuning;
    /** Difficulty-scaled release bar for this delivery, snapped to the animation. */
    FC26ReleaseBar ActiveBar;
    /** Deliveries this bowler may attempt right now, rebuilt each ball. */
    TArray<EC26Delivery> DeliveryLibrary;

    FVector BowlingIntendedPitch = FVector(0.f, 460.f, 0.f);
    FVector BowlingActualPitch = FVector(0.f, 460.f, 0.f);
    float BowlingReleaseErrorMs = 0.f;
    float BowlingExecutionQuality = 1.0f;

    /** Where on the bar the player let go, 0..1. -1 until they do. */
    float ReleaseMeterValue = -1.f;
    EC26ReleaseBand ReleaseBand = EC26ReleaseBand::TooEarly;
    float ReleaseBandQuality = 0.f;
    bool bBowlingNoBall = false;
    /** Measured speed of the ball that was actually bowled, km/h. */
    float LastActualKph = 0.f;
    float LastPlannedKph = 0.f;
    /** Previous delivery's real pitch point, for the tactical ghost marker. */
    FVector LastActualPitch = FVector::ZeroVector;
    bool bHasLastPitch = false;
    /** Where the last ball actually left the hand, and the crease offset it was
        released with. The trajectory preview is built from this rather than a
        guess, so the drawn path and the bowled ball start from the same point. */
    FVector LastBowlingOrigin = FVector(-20.f, -995.f, 196.f);
    bool bHasBowlingOrigin = false;
    float LastCreaseOffset = 0.f;

    // Planning pointers: each control is owned by the finger that grabbed it.
    int32 TargetPointerId = -1, MovementPointerId = -1, PacePointerId = -1, ReleasePointerId = -1;
    FVector2D MovementDragStart = FVector2D::ZeroVector;
    FVector2D MovementDragCurrent = FVector2D::ZeroVector;
    bool bMovementDragging = false;

    /** -C26Debug or the C26Controls exec: development batting-input overlay. */
    bool bDebugControls = false;

    // ---- batting gesture state machine -------------------------------------
    /** True when the batting gesture zone will accept a press right now. */
    bool IsBattingInputLive() const;
    // Bowling control regions, design space. Kept here so the HUD draws exactly
    // what the input layer hit-tests - the two can never drift apart.
    static constexpr float DialCentreX = 1389.f, DialCentreY = 415.f, DialRadius = 45.f;
    // The 8-unit track is centred at Y=659, level with the wicket-side toggle.
    static constexpr float PaceTrackX = 1234.f, PaceTrackW = 310.f, PaceTrackY = 655.f;
    bool IsOnMovementDial(FVector2D D) const
    { return (D - FVector2D(DialCentreX, DialCentreY)).Size() <= DialRadius + 26.f; }
    bool IsOnPaceSlider(FVector2D D) const
    { return D.X >= PaceTrackX - 26.f && D.X <= PaceTrackX + PaceTrackW + 26.f && D.Y >= PaceTrackY - 30.f && D.Y <= PaceTrackY + 30.f; }

    /** Design-space test for the floating-origin batting region. */
    bool IsInBattingGestureZone(FVector2D DesignPos) const;
    bool BeginBattingGesture(int32 PointerId, FVector2D DesignPos);
    void UpdateBattingGesture(int32 PointerId, FVector2D DesignPos);
    void ReleaseBattingGesture(int32 PointerId, FVector2D DesignPos);
    void CancelBattingGesture(const TCHAR* Why);
    /** Recomputes direction/magnitude/candidate from the current finger position. */
    void EvaluateBattingGesture();
    /** Seconds until the ideal release instant. Valid during run-up AND flight. */
    float SecondsToIdealRelease() const;
    FString GetReleaseTimingName() const;

    // ---- bowling controls (all pointer-owned, touch and mouse identical) ----
    /** Rebuilds DeliveryLibrary and the bowler profile for the current bowler. */
    void RefreshBowlerProfile();
    void CycleDelivery(int Direction);
    void SelectDelivery(EC26Delivery Type);
    bool BeginMovementDrag(int32 PointerId, FVector2D DesignPos);
    void UpdateMovementDrag(int32 PointerId, FVector2D DesignPos);
    void EndMovementDrag(int32 PointerId);
    bool BeginPaceDrag(int32 PointerId, FVector2D DesignPos);
    void UpdatePaceDrag(int32 PointerId, FVector2D DesignPos);
    void EndPaceDrag(int32 PointerId);
    void CancelBowlingDrags();
    /** Composes the plan at a clean release: the preview and the ball agree. */
    FC26DeliveryPlan PreviewDelivery() const;
    /** Sampled flight of PreviewDelivery, for the on-pitch trajectory line. */
    void BuildTrajectoryPreview();
    TArray<FVector> TrajectoryPreview;
    int32 TrajectoryPreviewBounce = -1;
    uint32 TrajectoryPreviewHash = 0;
    float PlannedKph() const;
    void PaceRangeKph(float& OutMin, float& OutMax) const;
    bool BatterIsLeftHanded() const;
    FString GetDeliveryName() const;
    FString GetMovementText() const;
    FString GetReleaseBandName() const;
    /** Lateral shift of the release point for the current crease position, cm.
        Zero over the wicket; a signed offset around it. Both the trajectory
        preview and the live release go through this, so they cannot disagree. */
    float CreaseOffsetCm() const;
    /** Where the ball will leave the hand: the end of the run-up, shifted by the
        crease position. Shared by the preview and the live release. */
    FVector ReleaseOriginCm() const;

    /** Run-up: publish the marker from the bowler's LOCKED intent, before any ball exists. */
    void BeginBouncePreview();
    /** Release: hand the marker the measured trajectory so it can correct smoothly. */
    void CalculateBouncePrediction();
    void UpdateBouncePrediction(float Dt);
    bool IsBounceIndicatorVisible() const;
    FVector GetBounceIndicatorLocation() const;
    float GetBounceIndicatorAlpha() const;
    FString GetBounceIndicatorText() const;
    FString GetBattingStateName() const;
    FString GetBowlingStateName() const;
    FString GetDeliveryLengthName() const;
    FString GetDeliveryLineName() const;

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

    // ---- Advanced Gameplay Control Systems ----
    // System 1: Delivery History & Pitch Target Presets
    UPROPERTY(Transient)
    TArray<FC26DeliveryRecord> RecentDeliveries;
    void RecordDeliveryOutcome(const FC26DeliveryRecord& Record);
    void ApplyBowlingPresetTarget(FName PresetTarget);

    // System 2: Field Planning Mode & Tactical Presets
    bool bFieldPlanningMode = false;
    EC26FieldPreset CurrentFieldPreset = EC26FieldPreset::Balanced;
    int32 SelectedFielderIdx = -1;
    FString FieldLegalityWarning;
    bool bFieldIsLegal = true;
    bool bCustomFieldApplied = false;
    bool bInningsBreakPresented = false;
    bool bMatchEndPresented = false;

    void ToggleFieldPlanning();
    void SetFieldPlanning(bool bActive);
    void ApplyFieldPreset(EC26FieldPreset Preset);
    void SelectFielderForReposition(int32 AthleteIndex);
    void MoveFielderToLocation(int32 AthleteIndex, const FVector& NewTurfLocation);
    bool ValidateCurrentField();
    const TArray<FVector>& GetFieldPositions() const { return FieldPositions; }

    // System 3 & 4: Manual Fielding, Diving, Throwing & Catching
    int ActiveFielder = -1, BackupFielder = -1;
    FVector2D ManualFieldingStick = FVector2D::ZeroVector;
    float FieldingAssistLevel = 0.30f;
    bool bDivePromptActive = false;
    bool bDiveRequested = false;
    float DiveCooldown = 0.f;

    void SetManualFielderInput(const FVector2D& Stick);
    void TriggerManualDive();

    bool bCatchOpportunityActive = false;
    float CatchPromptTimer = 0.f;
    float CatchOptimalTime = 0.f;
    EC26CatchTiming LastCatchTiming = EC26CatchTiming::None;
    float LastCatchQuality = 0.f;
    void AttemptManualCatch();

    bool bThrowTargetActive = false;
    EC26ThrowTarget SelectedThrowTarget = EC26ThrowTarget::KeepersEnd;
    float ThrowPowerCharge = 0.f;
    bool bThrowCharging = false;
    void SetThrowTarget(EC26ThrowTarget Target);
    void StartThrowCharge();
    void ReleaseThrowCharge();
    /** Manual-throw decision pause + execution (fielding-side interaction). */
    bool bFieldingDecisionPaused = false;
    void ExecuteFielderThrow();
    void SetThrowStyle(bool bDirectHit);

    // System 6: Match Presentation Callbacks
    void OnPresentationCompleted();
    /** Round 7: presentation-only player reactions staged from the committed outcome (never the reverse). */
    void StageReactions(const C26::DeliveryOutcome& Official,const FVector& Focus);
    /** Fielders who put down a chance / made a stop this delivery, for the reactions. Reset every ball. */
    int32 DroppedBy=-1,StoppedBy=-1;
    void TriggerPresentationForOutcome(const C26::DeliveryOutcome& Outcome);    bool bFiftyCelebrated[3] = { false, false, false };
    bool bCenturyCelebrated[3] = { false, false, false };
    int32 ConsecutiveBoundaries = 0;
    int32 ConsecutiveDots = 0;

    // ---- Broadcast lower-third graphics queue (presentation only; scoring untouched) ----
    /** Single active lower-third. The HUD draws it; expiry is on the match Clock. */
    FString GraphicTitle, GraphicSub;
    FLinearColor GraphicAccent = FLinearColor(1.f, 1.f, 1.f, 1.f);
    float GraphicStartAt = -99.f, GraphicDuration = 0.f;
    /** Push a restrained broadcast lower-third (new batter, milestone, bowler figures, ...). */
    void PushGraphic(const FString& Title, const FString& Sub, const FLinearColor& Accent, float Duration = 2.6f);
    /** Active graphic + fade alpha for the HUD. False when none is live. */
    bool GetActiveGraphic(FString& Title, FString& Sub, FLinearColor& Accent, float& Alpha) const;
    void UpdateBroadcastGraphics(const C26::DeliveryOutcome& Outcome);
    int32 GraphicStriker = -1, GraphicOver = -1;

    // System 5: Batting Timing Feedback Meter
    float LastTimingDeltaMs = 0.f;
    FString LastShotName;
    float LastTimingQualityPct = 0.f;
    float ContactFeedbackTimer = 0.f;
private:
    C26::DeliveryOutcome Pending;
    uint32 DeliveryId=0;
    int RunnerAId=0,RunnerBId=1;
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
        breaks every camera angle that looks past the wicket. */
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
    /** Seam-axis state for believable ball rotation; presentation only. */
    FVector BallSeamAxis = FVector(0.f, 1.f, 0.f);
    float BallSeamWobble = 0.f;
    void BreakWicket(float WicketY);
    void ResetStumps();
    void Haptic(float Strength);
    FC26CommentaryContext MakeCommentaryContext() const;
    FC26CommentaryEvent MakeCommentaryEvent(ECommentaryEventType Type, int32 Runs=0, bool bBoundary=false, bool bSix=false, bool bWicket=false, uint8 Dismissal=0) const;
    /** Dust and turf response for one ball's worth of contact events. */
    void Spark(const FVector& At,bool Struck);    /** Momentary time pinch on a well-struck ball, and the real-time stamp it ends at. */
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
    /** Round 7 reaction lab (-C26ReactLab): real AI delivery, then committed outcomes forced through Resolve. */
    void UpdateReactLab(float Dt);
    bool ReactLab=false;
    int32 ReactStage=0,ReactFailures=0;
    double ReactStarted=0;
    EC26Phase ReactPrevPhase=EC26Phase::Menu;
    TMap<int32,TArray<FVector>> ReactPrevBones;
    TMap<int32,FName> ReactPrevState;
    TSet<FString> ReactShots;
    TWeakObjectPtr<class ACameraActor> ReactCamera;
    bool GoldenGate=false,GateCollected=false,GateThrown=false,GateNoScreens=false,GateSuite=false;
    bool GateSawFour=false,GateSawSix=false,GateSawWicket=false;
    bool GateGestureArmed=false,GateGestureLogged=false,GateMarkerRunUpChecked=false,GateGesturePressed=false;
    int GateMatches=0;
    void UpdateProductionGate(float Dt);
    // ---- C26BatLab: scripted batting playtest through the real input path ----
    void UpdateBatLab(float Dt);
    // ---- C26BowlLab: scripted bowling playtest through the real input path ----
    // The lab IS the bowler: it presses the carousel, drags the pitch target,
    // turns the movement dial, moves the pace slider and releases on the bar,
    // all through AC26PlayerController's real pointer routing. Nothing here
    // reaches into BowlingPlan to "help" it; the plan is read back afterwards.
    void UpdateBowlLab(float Dt);
    bool BowlLab=false,BowlLabShots=false,BowlLabQuietFail=false;
    bool BowlLabPressed=false,BowlLabReleased=false,BowlLabSecondFinger=false,BowlLabResultLogged=false;
    int BowlLabCase=0,BowlLabPhase=0,BowlLabFailures=0,BowlLabPlayed=0;
    double BowlLabStarted=0;
    FString BowlLabDirectory;
    TSet<FString> BowlLabShotKeys;
    /** Which side the lab is currently bowling for; the two named bowlers are
        opposite arms, so switching arms means starting a fresh innings. */
    int BowlLabTeam=-1;
    /** Release point the ball actually left the hand from, captured at release. */
    FVector BowlLabOrigin=FVector::ZeroVector;
    /** Release origin of the previous delivery, for the around-the-wicket check. */
    FVector BowlLabPrevOrigin=FVector::ZeroVector;
    FVector BowlLabPrevPitch=FVector::ZeroVector;
    bool BowlLabHasPrev=false;
    int BowlLabLedgerBefore=0;
    int BowlLabExtrasBefore=0,BowlLabLegalBefore=0;
    /** Rules epoch the current case was bowled in, so a restart between cases
        cannot make the scorecard deltas look like a regression. */
    uint32 BowlLabEpoch=0;
    /** Meter value (0..1) this case releases at; the lab waits for it to arrive. */
    float BowlLabReleaseAt=0.f;
    /** The plan the lab actually asked for, so intent can be compared to result. */
    float BowlLabWantLine=0.f,BowlLabWantLength=0.f,BowlLabWantPace=0.f,BowlLabWantMag=0.f;
    EC26Delivery BowlLabWantType=EC26Delivery::Pace;
    /** Per-delivery capture, filled at the release frame and read on the way out. */
    float BowlLabMeter=0.f,BowlLabQuality=0.f,BowlLabKph=0.f,BowlLabSwing=0.f,BowlLabDeviation=0.f,BowlLabOnset=0.f;
    FVector BowlLabPitch=FVector::ZeroVector,BowlLabIntent=FVector::ZeroVector;
    EC26ReleaseBand BowlLabBand=EC26ReleaseBand::TooEarly;
    EC26Delivery BowlLabType=EC26Delivery::Pace;
    bool BowlLabNoBall=false;
    bool BowlLabHadLastPitch=false;
    FVector BowlLabLastPitch=FVector::ZeroVector;
    void Check2(bool Passed,const TCHAR* Message);
    bool BatLab=false,LabShots=false,LabPressed=false,LabReleased=false,LabSecondFinger=false,LabQuietFail=false,LabSawBounce=false,LabResultLogged=false;
    int LabCase=0,LabPhase=0,LabFailures=0,LabPlayed=0;
    double LabStarted=0;
    float LabAim=0,LabMag=0,LabAgg=0,LabDelta=0,LabPower=0,LabMidAim=0,LabPressAim=0;
    FString LabCandidate,LabTiming,LabGestureLog,LabDirectory;
    TSet<FString> LabShotKeys;
    int GateStage=0,GateFailures=0;
    uint32 GateEpoch=0;
    double GateStarted=0;
    FString GateDirectory;
    TSet<FString> GateShots;
    TArray<float> GateFrameTimes;
    /** Game-thread cost of posing all athletes (clip selection, evaluation, bat control) per live-ball frame. */
    TArray<float> GateAnimateTimes;
#endif
};
