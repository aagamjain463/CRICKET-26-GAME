#include "C26MatchGameMode.h"
#include "Curves/CurveFloat.h"
#include "C26Athlete.h"
#include "C26CameraDirector.h"
#include "C26Stadium.h"
#include "C26Effects.h"
#include "C26Audio.h"
#include "C26CommentaryDirector.h"
#include "C26Settings.h"
#include "C26PlayerController.h"
#include "C26HUD.h"
#include "C26Delivery.h"
#include "C26Controls.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "UnrealClient.h"

namespace
{
// One ordered visual-acceptance beat per presentation state. Batting: -1 any, 1 player batting,
// 0 player bowling. Overlay: 0 none, 1 settings, 2 controls, 3 paused. Screen: menu screen
// index, -1 any. Nav: optional UIAction fired once when the beat is first seen.
struct FC26Beat{EC26Phase Phase;int32 Batting;int32 Overlay;float After;const TCHAR* Name;float Timeout;int32 Screen;const TCHAR* Nav;int32 Toss;};
const FC26Beat GC26Beats[]={
    {EC26Phase::Menu,-1,0,2.5f,TEXT("01_home"),30.f,0,nullptr,0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("02_play"),30.f,1,TEXT("nav_play"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("03_teams"),30.f,2,TEXT("nav_teams"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("04_matchup"),30.f,3,TEXT("nav_matchup"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("05_toss"),30.f,4,TEXT("nav_toss"),0},
    {EC26Phase::Menu,-1,0,2.6f,TEXT("06_tossresult"),30.f,4,TEXT("tossflip"),2},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("07_myteam"),30.f,5,TEXT("nav_myteam"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("07b_career"),30.f,6,TEXT("nav_career"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("07c_leaderboards"),30.f,7,TEXT("nav_tour"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("07d_multiplayer"),30.f,8,TEXT("nav_online"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("07e_store"),30.f,13,TEXT("nav_store"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("08_world"),30.f,10,TEXT("nav_world"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("09_settings"),30.f,11,TEXT("nav_settings"),0},
    {EC26Phase::Menu,-1,0,1.2f,TEXT("10_help"),30.f,12,TEXT("nav_help"),0},
    {EC26Phase::Intro,-1,0,1.4f,TEXT("11_intro_flyover"),60.f,-1,TEXT("tossquick"),0},
    {EC26Phase::Intro,-1,0,4.2f,TEXT("12_intro_batter"),60.f,-1,nullptr,0},
    {EC26Phase::Ready,1,0,.5f,TEXT("13_ready_batting"),60.f,-1,nullptr,0},
    {EC26Phase::RunUp,1,0,1.9f,TEXT("14_runup_batting"),60.f,-1,nullptr,0},
    {EC26Phase::Delivery,1,0,.3f,TEXT("15_delivery_batting"),60.f,-1,nullptr,0},
    {EC26Phase::InPlay,-1,0,.45f,TEXT("16_in_play"),60.f,-1,nullptr,0},
    {EC26Phase::InPlay,-1,0,1.6f,TEXT("17_fielding"),60.f,-1,nullptr,0},
    {EC26Phase::Reaction,-1,0,.7f,TEXT("18_reaction"),60.f,-1,nullptr,0},
    {EC26Phase::Replay,-1,0,1.2f,TEXT("19_replay"),60.f,-1,nullptr,0},
    {EC26Phase::Interval,-1,0,.9f,TEXT("20_interval"),220.f,-1,nullptr,0},
    {EC26Phase::Ready,0,0,.5f,TEXT("21_ready_bowling"),220.f,-1,nullptr,0},
    {EC26Phase::Ready,0,3,.6f,TEXT("22_pause"),220.f,-1,nullptr,0},
    {EC26Phase::RunUp,0,0,1.5f,TEXT("23_runup_bowling"),60.f,-1,nullptr,0},
    {EC26Phase::Delivery,0,0,.3f,TEXT("24_delivery_bowling"),60.f,-1,nullptr,0},
    {EC26Phase::Result,-1,0,1.2f,TEXT("25_result"),340.f,-1,nullptr,0},
};
}

AC26MatchGameMode::AC26MatchGameMode()
{
    PrimaryActorTick.bCanEverTick=true;DefaultPawnClass=nullptr;
    PlayerControllerClass=AC26PlayerController::StaticClass();HUDClass=AC26HUD::StaticClass();
    Audio=CreateDefaultSubobject<UC26Audio>(TEXT("MatchAudio"));CommentaryDirector=CreateDefaultSubobject<UC26CommentaryDirector>(TEXT("CommentaryDirector"));
}
void AC26MatchGameMode::BeginPlay()
{
    Super::BeginPlay();Preferences=UC26Settings::Load();Preferences->Apply();
    TActorIterator<AC26Stadium> It(GetWorld());if(It)Venue=*It;
    if(!Venue)Venue=GetWorld()->SpawnActor<AC26Stadium>();
    Venue->SetQuality(Preferences->Quality);
    Director=GetWorld()->SpawnActor<AC26CameraDirector>();
    if(auto* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(Director);
    BuildMatchActors();Audio->Master=Preferences->SoundVolume;Audio->Initialize();
    Rules.Reset();AI.Reset(FMath::Rand());ChangePhase(EC26Phase::Menu);
    Smoke=FParse::Param(FCommandLine::Get(),TEXT("C26Smoke"));
    if(Smoke&&FParse::Param(FCommandLine::Get(),TEXT("C26SmokeToss")))UseToss=true;
    bDebugTrace=FParse::Param(FCommandLine::Get(),TEXT("C26Debug"));
    if(Smoke){AutoPlay=true;Preferences->Difficulty=1;StartMatch();UE_LOG(LogC26,Display,TEXT("C26_SMOKE_BEGIN: ten complete autonomous matches"));}
    Capture=FParse::Param(FCommandLine::Get(),TEXT("C26Shots"));
    if(Capture)
    {
        AutoPlay=true;Preferences->Difficulty=1;PlayerBatsFirst=true;UseToss=false;
        Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        UE_LOG(LogC26,Display,TEXT("C26_SHOTS_BEGIN: %d visual acceptance beats"),int(UE_ARRAY_COUNT(GC26Beats)));
    }
#if !UE_BUILD_SHIPPING
    GoldenGate=FParse::Param(FCommandLine::Get(),TEXT("C26GoldenGate"));
    if(GoldenGate)
    {
        AutoPlay=false;Capture=Smoke=false;PlayerTeam=0;PlayerBatsFirst=true;UseToss=false;
        Preferences->Difficulty=1;Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        GateDirectory=FPaths::ProjectDir()/TEXT("Artifacts/GoldenGate");
        FParse::Value(FCommandLine::Get(),TEXT("C26GateDir="),GateDirectory);
        GateNoScreens=FParse::Param(FCommandLine::Get(),TEXT("C26GateNoScreens"));
        GateSuite=FParse::Param(FCommandLine::Get(),TEXT("C26GateSuite"));
        int GateFPS=0;
        if(FParse::Value(FCommandLine::Get(),TEXT("C26GateFPS="),GateFPS)&&GateFPS>=15&&GateFPS<=120)
        {
            // Fixed simulation cadence for regression only; not a performance measurement.
            FApp::SetFixedDeltaTime(1.0/GateFPS);FApp::SetUseFixedTimeStep(true);
            UE_LOG(LogC26,Display,TEXT("C26_GATE_FIXED_FPS %d"),GateFPS);
        }
        GateStarted=FPlatformTime::Seconds();StartMatch();Skip();GateEpoch=Rules.Epoch;
        GateGestureArmed=true;GateGestureLogged=false;GateGesturePressed=false;GateMarkerRunUpChecked=false;
        UE_LOG(LogC26,Display,TEXT("C26_GATE_BEGIN drive -> miss -> restart -> drive; epoch=%u"),GateEpoch);
    }
    BatLab=FParse::Param(FCommandLine::Get(),TEXT("C26BatLab"));
    if(BatLab)
    {
        // Scripted batting playtest: real match, real input routing, no autopilot.
        AutoPlay=false;Capture=Smoke=false;GoldenGate=false;PlayerTeam=0;PlayerBatsFirst=true;UseToss=false;
        Preferences->Difficulty=1;Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        Preferences->ControlScheme=0;
        bDebugControls=true;
        LabShots=FParse::Param(FCommandLine::Get(),TEXT("C26BatLabShots"));
        LabDirectory=FPaths::ProjectDir()/TEXT("Artifacts/BatLab");
        FParse::Value(FCommandLine::Get(),TEXT("C26BatLabDir="),LabDirectory);
        LabStarted=FPlatformTime::Seconds();
        StartMatch();Skip();
        UE_LOG(LogC26,Display,TEXT("C26_LAB_BEGIN_SUITE scripted batting playtest"));
    }
    BowlLab=FParse::Param(FCommandLine::Get(),TEXT("C26BowlLab"));
    if(BowlLab)
    {
        // Scripted bowling playtest: real match, real input routing, no autopilot.
        // PlayerBatsFirst=false puts the PLAYER in the field first, which is the
        // only way PlayerBatting() is false and the bowling controls are live.
        AutoPlay=false;Capture=Smoke=false;GoldenGate=false;BatLab=false;
        PlayerTeam=0;PlayerBatsFirst=false;UseToss=false;
        Preferences->Difficulty=1;Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        Preferences->ControlScheme=0;
        bDebugControls=true;
        BowlLabShots=FParse::Param(FCommandLine::Get(),TEXT("C26BowlLabShots"));
        BowlLabDirectory=FPaths::ProjectDir()/TEXT("Artifacts/BowlLab");
        FParse::Value(FCommandLine::Get(),TEXT("C26BowlLabDir="),BowlLabDirectory);
        // Fixed simulation cadence so a release lands on the same meter value on
        // every machine; this is a correctness harness, not a timing benchmark.
        int BowlFPS=0;
        if(FParse::Value(FCommandLine::Get(),TEXT("C26BowlLabFPS="),BowlFPS)&&BowlFPS>=15&&BowlFPS<=120)
        {
            FApp::SetFixedDeltaTime(1.0/BowlFPS);FApp::SetUseFixedTimeStep(true);
            UE_LOG(LogC26,Display,TEXT("C26_BOWLLAB_FIXED_FPS %d"),BowlFPS);
        }
        // Reverse swing is conditional by design; the lab needs it reachable from
        // ball one to be able to test it at all.
        if(FParse::Param(FCommandLine::Get(),TEXT("C26ReverseAlways")))
        {
            BowlingTuning.bAllowReverseAlways=true;
            UE_LOG(LogC26,Display,TEXT("C26_BOWLLAB_REVERSE_OVERRIDE on"));
        }
        BowlLabStarted=FPlatformTime::Seconds();
        StartMatch();Skip();
        UE_LOG(LogC26,Display,TEXT("C26_BOWLLAB_BEGIN_SUITE scripted bowling playtest"));
    }
    if(FParse::Value(FCommandLine::Get(),TEXT("C26Probe="),ProbeName))
    {
        AutoPlay=false;Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        StartMatch();Skip();
    }
#endif
}
int AC26MatchGameMode::BattingTeam() const{return Rules.Current==0?FirstBattingTeam:1-FirstBattingTeam;}
bool AC26MatchGameMode::PlayerBatting() const{return BattingTeam()==PlayerTeam;}
FString AC26MatchGameMode::TeamName(int Team)const{return Team==0?TEXT("MUMBAI METEORS"):TEXT("MELBOURNE EMBERS");}
FString AC26MatchGameMode::TeamShort(int Team)const{return Team==0?TEXT("MET"):TEXT("EMB");}
FString AC26MatchGameMode::BatterName()const
{
    static const TCHAR* Names[2][3]={{TEXT("A. RAO"),TEXT("K. DESAI"),TEXT("R. MEHRA")},{TEXT("J. HART"),TEXT("L. REED"),TEXT("M. VALE")}};
    return Names[BattingTeam()][FMath::Clamp(Rules.Now().Striker,0,2)];
}
FString AC26MatchGameMode::NonStrikerName()const
{
    static const TCHAR* Names[2][3]={{TEXT("A. RAO"),TEXT("K. DESAI"),TEXT("R. MEHRA")},{TEXT("J. HART"),TEXT("L. REED"),TEXT("M. VALE")}};
    return Names[BattingTeam()][FMath::Clamp(Rules.Now().NonStriker,0,2)];
}
FString AC26MatchGameMode::BowlerName()const{return BattingTeam()==0?TEXT("N. ARCHER"):TEXT("V. SEN");}
FC26CommentaryContext AC26MatchGameMode::MakeCommentaryContext() const
{
    // Snapshot of authoritative state for the commentary observers.
    FC26CommentaryContext C;
    C.BallsRemaining = Rules.BallsRemaining();
    C.RunsRequired = Rules.RunsRequired();
    C.Target = Rules.Target();
    C.Score = Rules.Now().Runs;
    C.Wickets = Rules.Now().Wickets;
    C.InningsNumber = Rules.Current;
    C.DeliveryType = (uint8)(Phase == EC26Phase::Ready ? Bowling.Type : LockedBowling.Type);
    C.TimingResult = (uint8)LastContact.Timing;
    C.ContactQuality = LastContact.Quality;
    C.bFinalBall = Rules.BallsRemaining() == 1;
    C.bPressure = Rules.Current == 1 && Rules.RunsRequired() > 0
        && Rules.RunsRequired() <= Rules.BallsRemaining() * 2 + 2;
    C.ConsecutiveBoundaries = Audio ? Audio->GetConsecutiveBoundaries() : 0;
    C.ConsecutiveDots = Audio ? Audio->GetConsecutiveDots() : 0;
    C.DeliveryId = DeliveryId;
    return C;
}
FC26CommentaryEvent AC26MatchGameMode::MakeCommentaryEvent(ECommentaryEventType Type, int32 Runs, bool bBoundary, bool bSix, bool bWicket, uint8 Dismissal) const
{
    FC26CommentaryEvent E;
    E.EventType = Type;
    E.BatterName = BatterName();
    E.BowlerName = BowlerName();
    E.RunsScored = Runs;
    E.bIsBoundary = bBoundary;
    E.bIsSix = bSix;
    E.bIsWicket = bWicket;
    E.DismissalType = Dismissal;
    E.DeliveryType = (uint8)(Phase == EC26Phase::Ready ? Bowling.Type : LockedBowling.Type);
    E.DeliveryLine = LockedBowling.Line;
    E.DeliveryLength = LockedBowling.Length;
    E.TimingQuality = (float)LastContact.Timing;
    E.ContactQuality = LastContact.Quality;
    E.CurrentScore = Rules.Now().Runs;
    E.Wickets = Rules.Now().Wickets;
    E.InningsNumber = Rules.Current;
    E.CurrentOver = 0;
    E.BallNumber = Rules.Now().LegalBalls;
    E.Target = Rules.Target();
    E.RunsRequired = Rules.RunsRequired();
    E.BallsRemaining = Rules.BallsRemaining();
    E.RequiredRunRate = (Rules.Current == 1 && Rules.BallsRemaining() > 0) ? ((float)Rules.RunsRequired() / (float)Rules.BallsRemaining() * 6.f) : 0.f;
    E.CurrentRunRate = Rules.Now().LegalBalls > 0 ? ((float)Rules.Now().Runs / (float)Rules.Now().LegalBalls * 6.f) : 0.f;
    E.bIsFreeHit = Rules.Now().FreeHit;
    E.bIsSuperOver = true;
    E.bIsFinalOver = true;
    E.bIsFinalBall = Rules.BallsRemaining() == 1;
    E.bIsMatchWinningEvent = (Rules.Current == 1 && Rules.Now().Runs >= Rules.Target());
    E.BatterRuns = Rules.Now().BatterRuns[FMath::Clamp(Rules.Now().Striker, 0, 2)];
    E.BatterBalls = Rules.Now().BatterBalls[FMath::Clamp(Rules.Now().Striker, 0, 2)];
    E.ConsecutiveBoundaries = Audio ? Audio->GetConsecutiveBoundaries() : 0;
    E.ConsecutiveDots = Audio ? Audio->GetConsecutiveDots() : 0;
    E.DeliveryId = DeliveryId;
    if (CommentaryDirector)
    {
        E.MatchPressure = CommentaryDirector->CalculatePressure(E);
    }
    return E;
}
void AC26MatchGameMode::BuildMatchActors()
{
    FieldPositions=FC26AI::Field();
    for(int I=0;I<14;++I)Athletes.Add(GetWorld()->SpawnActor<AC26Athlete>());
    FirstBattingTeam=0;
    for(int I=0;I<11;++I){Athletes[I]->Configure(I==0?EC26Role::Bowler:I==1?EC26Role::Keeper:EC26Role::Fielder,1,I+1);Athletes[I]->ResetAt(FieldPositions[I],(FVector(0,850,0)-FieldPositions[I]).Rotation().Yaw);}
    Athletes[11]->Configure(EC26Role::Batter,0,7);Athletes[11]->ResetAt(FVector(-38,900,5),-90);
    Athletes[12]->Configure(EC26Role::Batter,0,18);Athletes[12]->ResetAt(FVector(-80,-865,5),90);
    Athletes[13]->Configure(EC26Role::Umpire,0,0);Athletes[13]->ResetAt(FVector(105,-1390,5),90);
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* BallHeroMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Equipment/SM_C26_Ball_Hero.SM_C26_Ball_Hero"));
    auto* StumpMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Equipment/SM_C26_Stump_Single.SM_C26_Stump_Single"));
    auto* BailMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Equipment/SM_C26_Bail_Single.SM_C26_Bail_Single"));
    auto* White=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_White.M_White"));
    AActor* Props=GetWorld()->SpawnActor<AActor>();Props->SetRootComponent(NewObject<USceneComponent>(Props));Props->GetRootComponent()->RegisterComponent();
    BallMesh=NewObject<UStaticMeshComponent>(Props,TEXT("WhiteCricketBall"));BallMesh->SetupAttachment(Props->GetRootComponent());
    BallMesh->SetStaticMesh(BallHeroMesh?BallHeroMesh:Sphere);BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);BallMesh->RegisterComponent();
    BallMesh->SetMobility(EComponentMobility::Movable);BallMesh->SetCastShadow(true);
    auto* Wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Willow.M_Willow"));
    if(!Wood)Wood=White;
    // Physics stays at the real 3.6 cm radius. The render is a deliberate 1.6x readability cheat:
    // a true-size ball is ~3 px on a phone at broadcast distance. Every commercial cricket game
    // does the same; anything bigger starts reading as tennis.
    if(BallHeroMesh) BallMesh->SetWorldScale3D(FVector(1.6f));
    else BallMesh->SetWorldScale3D(FVector(Tuning.BallRadius*2/100.f*1.6f));
    auto* BallMaterial=UMaterialInstanceDynamic::Create(White,this);BallMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.88,.88,.81));BallMaterial->SetScalarParameterValue(TEXT("Glow"),0.f);BallMaterial->SetScalarParameterValue(TEXT("Roughness"),.38f);BallMesh->SetMaterial(0,BallMaterial);
    for(int End=0;End<2;++End)for(int I=0;I<5;++I)
    {
        auto* S=NewObject<UStaticMeshComponent>(Props);S->SetupAttachment(Props->GetRootComponent());
        UStaticMesh* ChosenMesh=I<3?(StumpMesh?StumpMesh:Cylinder):(BailMesh?BailMesh:Cylinder);
        S->SetStaticMesh(ChosenMesh);
        if(!StumpMesh)S->SetMaterial(0,Wood);
        S->SetCollisionEnabled(ECollisionEnabled::NoCollision);S->RegisterComponent();Stumps.Add(S);
    }
    ResetStumps();Director->ReplayProps=Stumps;Director->SetAthletes(Athletes[0],Athletes[11]);
    Effects=GetWorld()->SpawnActor<AC26Effects>();
    Simulation.Ball.Position=FVector(0,0,-100);UpdateBallVisual();
}
void AC26MatchGameMode::HitStop(float Quality)
{
    // A very short pinch of time on a clean strike. This is what gives a bat impact weight; any
    // longer than about a tenth of a second and it stops reading as force and starts reading as a
    // frame hitch. Timed on real seconds so the dilation cannot extend its own duration.
    if(Quality<.72f||Smoke)return;
    const float Scale=Quality>.88f?.30f:.48f;
    UGameplayStatics::SetGlobalTimeDilation(this,Scale);
    HitStopUntil=GetWorld()->GetRealTimeSeconds()+(Quality>.88f?.075f:.055f);
}
void AC26MatchGameMode::ClearHitStop()
{
    // Must be idempotent and must run on every reset path: a match left in slow motion is exactly
    // the kind of stale state that makes a second Play Again feel broken.
    if(HitStopUntil<=0)return;
    HitStopUntil=0;UGameplayStatics::SetGlobalTimeDilation(this,1.f);
}
void AC26MatchGameMode::Spark(const FVector& At,bool Struck)
{
    if(!Effects)return;
    // On the strip a bounce lifts dry prepared dust; out in the field it scuffs damp turf instead.
    const bool OnPitch=FMath::Abs(At.X)<190.f&&FMath::Abs(At.Y)<1150.f;
    const float Speed=FMath::Clamp(Simulation.Ball.Velocity.Size()/3400.f,0.f,1.f);
    if(OnPitch)Effects->PitchDust(At,Speed);
    else Effects->TurfScuff(At,Simulation.Ball.Velocity,Speed*(Struck?1.f:.6f));
}
void AC26MatchGameMode::ResetStumps()
{
    StumpClock=-1;BrokenWicketY=0;
    auto* StumpMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Equipment/SM_C26_Stump_Single.SM_C26_Stump_Single"));
    for(int End=0;End<2;++End)for(int I=0;I<5;++I)
    {
        auto S=Stumps[End*5+I];const float Y=End?C26Field::WicketY:-C26Field::WicketY;
        const float Spacing=(C26Field::WicketWidth-C26Field::StumpDiameter)*.5f;
        S->SetMobility(EComponentMobility::Movable);S->SetCastShadow(true);
        if(StumpMesh)
        {
            if(I<3){S->SetWorldLocation(FVector((I-1)*Spacing,Y,C26Field::SurfaceZ));S->SetWorldRotation(FRotator::ZeroRotator);S->SetWorldScale3D(FVector(1.f));}
            else{S->SetWorldLocation(FVector((I==3?-1:1)*Spacing*.5f,Y,C26Field::StumpHeight));S->SetWorldRotation(FRotator::ZeroRotator);S->SetWorldScale3D(FVector(1.f));}
        }
        else
        {
            if(I<3){S->SetWorldLocation(FVector((I-1)*Spacing,Y,C26Field::SurfaceZ+C26Field::StumpHeight*.5f));S->SetWorldRotation(FRotator::ZeroRotator);S->SetWorldScale3D(FVector(.038,.038,C26Field::StumpHeight/100.f));}
            else{S->SetWorldLocation(FVector((I==3?-1:1)*Spacing*.5f,Y,C26Field::StumpHeight+.8f));S->SetWorldRotation(FRotator(0,0,90));S->SetWorldScale3D(FVector(.022,.022,.1095));}
        }
    }
}
void AC26MatchGameMode::BreakWicket(float Y)
{
    StumpClock=0;BrokenWicketY=Y;
    const int E=Y>0?5:0;
    Stumps[E+3]->SetWorldLocation(FVector(-14,Y+21,67));Stumps[E+3]->SetWorldRotation(FRotator(24,38,40));
    Stumps[E+4]->SetWorldLocation(FVector(18,Y+35,48));Stumps[E+4]->SetWorldRotation(FRotator(55,-28,20));
    Stumps[E+1]->SetWorldRotation(FRotator(-8,0,0));Audio->CueAt(TEXT("stump_hit"),FVector(0,Y,45.f),.9f);Haptic(.6f);
}
void AC26MatchGameMode::ChangePhase(EC26Phase NewPhase)
{
    if(!ResettingMatch&&!C26ValidTransition(Phase,NewPhase))
    {
        UE_LOG(LogC26,Error,TEXT("C26_INVALID_TRANSITION %d -> %d delivery=%u"),int(Phase),int(NewPhase),DeliveryId);
        return;
    }
    UE_LOG(LogC26,Log,TEXT("Match %u phase %d -> %d | delivery %u | innings %d | ball %d | score %d/%d"),Rules.Epoch,int(Phase),int(NewPhase),DeliveryId,Rules.Current+1,Rules.Now().LegalBalls,Rules.Now().Runs,Rules.Now().Wickets);
    Phase=NewPhase;PhaseTime=0;OnMatchChanged.Broadcast();
}
void AC26MatchGameMode::StartMatch()
{
    ResettingMatch=true;
    ClearHitStop();
    Rules.Reset();Simulation.Reset();Simulation.Tuning=Tuning;AI.Reset(260026+Rules.Epoch*917+FMath::RandRange(0,999));
    Director->Reset();Audio->Reset();ResetStumps();Paused=SettingsOpen=ControlsOpen=false;
    Running=Returning=ReleaseLocked=ShotQueued=Resolved=false;ThrowClock=-1;RequestedRuns=CompletedRuns=0;RunProgress=0;Intent={};Footwork=0;
    FirstBattingTeam=PlayerBatsFirst?PlayerTeam:1-PlayerTeam;
    if(TossResolved){FirstBattingTeam=PlayerBatsFirst?PlayerTeam:1-PlayerTeam;TossText=ResolvedTossText;}
    else if(UseToss){const bool Won=AI.Random.FRand()>.5f;FirstBattingTeam=Won?PlayerTeam:1-PlayerTeam;TossText=TeamShort(FirstBattingTeam)+TEXT(" WIN THE TOSS  /  BAT FIRST");}
    else TossText=TeamShort(FirstBattingTeam)+TEXT(" BAT FIRST  /  SIX BALLS. TWO WICKETS.");
    Callout=TEXT("ONE OVER. ALL TO PLAY FOR.");Detail=TEXT("ECLIPSE OVAL  /  NIGHT SESSION");
    Audio->NotifyMatchStart();if(CommentaryDirector)CommentaryDirector->OnMatchStart();
    PrepareDelivery();ChangePhase(EC26Phase::Intro);SmokeWatchdog=0;ResettingMatch=false;
}
void AC26MatchGameMode::Menu()
{
    ClearHitStop();
    Director->Restore(Athletes);Director->Reset();Simulation.Reset();Audio->Reset();Paused=false;SettingsOpen=false;ChangePhase(EC26Phase::Menu);
    MenuScreen=0;ScreenEnteredAt=Clock;ScreenFade=0;TossStage=0;TossResolved=false;PendingConfirm=NAME_None;SettingsTab=0;
}
void AC26MatchGameMode::SetScreen(int S)
{
    if(MenuScreen==S){ScreenFade=FMath::Min(1.f,ScreenFade+.5f);return;}
    MenuScreen=S;ScreenEnteredAt=Clock;ScreenFade=Preferences&&Preferences->ReducedMotion?1.f:0.f;PendingConfirm=NAME_None;
}
void AC26MatchGameMode::GoBack()
{
    // Predictable back stack: flow screens return to their parent, hubs to Home.
    if(MenuScreen==2)SetScreen(1);
    else if(MenuScreen==3)SetScreen(2);
    else if(MenuScreen==4)SetScreen(3);
    else if(MenuScreen==11||MenuScreen==12)SetScreen(0);
    else if(MenuScreen!=0)SetScreen(0);
}
void AC26MatchGameMode::Toast(const FString& S){ToastText=S;ToastUntil=Clock+2.2f;}
void AC26MatchGameMode::PrepareDelivery()
{
    Simulation.Reset();Simulation.Tuning=Tuning;Director->Reset();ResetStumps();
    // GesturePro transient state resets every ball; the human bowler's chosen
    // target persists across balls (only a fresh match re-centres it).
    bBattingGestureActive=false;bGestureArmed=false;GesturePointerId=-1;GestureDeliveryId=0;
    BattingState=EC26BattingState::Idle;GestureCancelReason.Reset();
    BouncePrediction=FC26BouncePrediction();
    BattingGestureAngle=0.f;BattingGesturePower=GestureTuning.MinPower;BattingPullFrac=0.f;
    BattingAggression=0.f;BattingPullRaw=FVector2D::ZeroVector;BattingPullRawMagnitude=0.f;
    BattingPullScreenAngle=0.f;BattingShotCandidate.Reset();
    BattingReleaseDeltaMs=0.f;BattingSuitability=1.f;BattingTimingQuality=0.f;
    BattingReleaseTiming=EC26ReleaseTiming::NoShot;PendingShotError=0.f;bPendingShotFromGesture=false;
    BattingTimingLabel=EC26Timing::Miss;BattingFeedbackUntil=-1.f;BattingCommitAge=-1.f;
    BattingGestureHoldTime=0.f;bFootworkManual=false;
    // Striker handedness drives the direction mirror; the preference lets a player test both.
    bLeftHandedBatter=Preferences&&Preferences->LeftHandedBatter;
    CancelBowlingDrags();
    ReleaseMeterValue=-1.f;ReleaseBand=EC26ReleaseBand::TooEarly;ReleaseBandQuality=0.f;bBowlingNoBall=false;
    ReleasePointerId=-1;LastActualKph=0.f;TrajectoryPreviewHash=0;
    if(!PlayerBatting()&&!AutoPlay)BowlingState=EC26BowlingState::Planning;
    Director->SetFieldingTarget(FVector::ZeroVector,false,false);
    Pending={};Pending.Epoch=Rules.Epoch;Pending.Id=++DeliveryId;
    Resolved=Important=ShotQueued=ReleaseLocked=Running=Returning=false;RunProgress=0;RequestedRuns=CompletedRuns=0;
    ActiveFielder=BackupFielder=-1;ThrowClock=CatchClock=-1;ThrowReleased=false;RunVelocity=0;
    BallReleased=KeeperTake=false;KeeperTakeClock=-1;MissTakeAge=0;MissTakeTarget=GatherPoint=FVector::ZeroVector;
    FieldDecisionClock=0;FieldForecast.Reset();LastContact={};Callout.Empty();Detail.Empty();FieldCreep=0.f;
    FootPlanted=false;ClearHitStop();if(Effects)Effects->Clear();
    Intent.Footwork=Footwork;RunnerAId=Rules.Now().Striker;RunnerBId=Rules.Now().NonStriker;
    FieldPositions=FC26AI::Field(AI.History.OffsideBias>.3f);
    for(int I=0;I<11;++I)
    {
        Athletes[I]->Configure(I==0?EC26Role::Bowler:I==1?EC26Role::Keeper:EC26Role::Fielder,1-BattingTeam(),I+1);
        Athletes[I]->ResetAt(FieldPositions[I],(FVector(0,850,0)-FieldPositions[I]).Rotation().Yaw);
    }
    Athletes[11]->Configure(EC26Role::Batter,BattingTeam(),7);Athletes[11]->ResetAt(FVector(-38+Footwork*35,900,5),-90);
    Athletes[11]->NonStriker=false;Athletes[12]->NonStriker=true;
    Athletes[12]->Configure(EC26Role::Batter,BattingTeam(),18);Athletes[12]->ResetAt(FVector(-145,-885,5),90);
    Athletes[13]->SetAction(EC26Action::Ready);Director->SetAthletes(Athletes[0],Athletes[11]);
    const bool FreshMatch=Rules.Current==0&&Rules.Now().LegalBalls==0&&Rules.Now().Ledger.empty();
    Bowling=PlayerBatting()||AutoPlay?AI.Bowl(Rules,Preferences->Difficulty):FC26DeliveryPlan();
    if(!PlayerBatting()&&!AutoPlay)
    {
        // Quick flow (PART 37): the whole plan - type, target, movement, pace -
        // survives to the next ball so an experienced player only tweaks what
        // they want. A fresh match starts from a sensible stock ball.
        if(FreshMatch)BowlingPlan=FC26BowlingPlan();
        RefreshBowlerProfile();
        BowlingIntendedPitch=FVector(BowlingPlan.TargetLine,BowlingPlan.TargetLength,8.f);
        Bowling=PreviewDelivery();
        TrajectoryPreviewHash=0;
    }
    Simulation.Ball.Position=Athletes[0]->HandPosition();UpdateBallVisual();
    Audio->SetTension(Rules.Current==1?1.f-float(Rules.BallsRemaining())/7.f:.15f,Preferences->SoundVolume);
    ChangePhase(EC26Phase::Ready);
    if(Rules.BallsRemaining()==1)Audio->Cue(TEXT("final_ball_pulse"),.23f);
    // Commentary observes the ready state exactly once per delivery.
    if(Rules.Current==1&&Rules.Now().LegalBalls==0&&Rules.Now().Ledger.empty())
    {
        Audio->NotifyChaseStart();
        if(CommentaryDirector)CommentaryDirector->OnChaseStart(MakeCommentaryEvent(ECommentaryEventType::ChaseStart));
    }
    if(Rules.BallsRemaining()==1)
    {
        Audio->NotifyFinalBallPre();
        if(CommentaryDirector)CommentaryDirector->OnPreBall(MakeCommentaryEvent(ECommentaryEventType::FinalBall));
    }
    else
    {
        Audio->NotifyPreBall(MakeCommentaryContext());
        if(CommentaryDirector)CommentaryDirector->OnPreBall(MakeCommentaryEvent(ECommentaryEventType::PreBall));
    }
}
void AC26MatchGameMode::StartDelivery()
{
    if(Phase!=EC26Phase::Ready||Paused||SettingsOpen||ControlsOpen||Rules.Now().Closed)return;
    CancelBowlingDrags();
    LockedBowling=Bowling;
    if(!PlayerBatting()&&!AutoPlay)
    {
        // Lock the plan the player authored; from here only their release can
        // change the ball. The preview they were reading IS this delivery.
        LockedBowling=PreviewDelivery();
        LastPlannedKph=PlannedKph();
        // Snap the release bar to the ACTUAL bowling animation: the no-ball line
        // is the front-foot plant frame, not an arbitrary percentage, so pushing
        // for a later release really is pushing toward overstepping.
        FC26ReleaseBar Bar=BowlingTuning.Bar;
        Bar.NoBallStart=(C26Field::RunUpDuration-.14f)/C26Field::RunUpDuration;
        ActiveBar=C26Delivery::ScaledBar(Bar,Preferences?Preferences->Difficulty:1);
        ReleaseMeterValue=-1.f;ReleaseBand=EC26ReleaseBand::TooEarly;ReleaseBandQuality=0.f;bBowlingNoBall=false;
    }
    ReleaseQuality=PlayerBatting()||AutoPlay?AI.Random.FRandRange(-.62f,.62f):0.f;
    // The batting gesture zone and the pitch marker both go live HERE, on the
    // authoritative run-up transition, not on a timer and not at ball release.
    if(PlayerBatting()&&BattingState==EC26BattingState::Idle)BattingState=EC26BattingState::ReadyForShot;
    BeginBouncePreview();
    if(!PlayerBatting()&&!AutoPlay)BowlingState=EC26BowlingState::RunUp;
    Athletes[0]->SetAction(EC26Action::Running);
    Athletes[0]->DeliveryStyle=Bowling.Type;
    LastStepY=-2700.f; // Bowler's mark; footsteps fall every ~95 cm of ground covered from here.
    ChangePhase(EC26Phase::RunUp);Audio->SetTension(Rules.BallsRemaining()==1?1.f:.7f,Preferences->SoundVolume);
    OnCricketEvent.Broadcast(TEXT("DeliveryStarted"),Athletes[0]->GetActorLocation());
}
float AC26MatchGameMode::BowlingMeter()const{return FMath::Clamp(PhaseTime/C26Field::RunUpDuration,0.f,1.f);}
void AC26MatchGameMode::BowlRelease()
{
    // ONE RELEASE = ONE DELIVERY. ReleaseLocked is the guard: a second tap, a
    // second finger or a repeated touch event after this point does nothing.
    if(Phase!=EC26Phase::RunUp||ReleaseLocked||PlayerBatting()||AutoPlay)return;
    ReleaseLocked=true;
    ReleaseMeterValue=BowlingMeter();
    ReleaseBand=C26Delivery::BandAt(ReleaseMeterValue,ActiveBar);
    ReleaseBandQuality=C26Delivery::QualityAt(ReleaseMeterValue,ActiveBar);
    // A no-ball is the player's own decision to push past the line, never a roll.
    bBowlingNoBall=(ReleaseBand==EC26ReleaseBand::NoBall);
    // Legacy scalar kept for the AI-facing error path and the debug readout.
    ReleaseQuality=C26Delivery::SignedError(ReleaseMeterValue,ActiveBar)*6.f;
    BowlingReleaseErrorMs=C26Delivery::SignedError(ReleaseMeterValue,ActiveBar)*C26Field::RunUpDuration*1000.f;
    BowlingExecutionQuality=ReleaseBandQuality;
    BowlingState=EC26BowlingState::Released;
    Haptic(bBowlingNoBall?.42f:.16f);
    Audio->Cue(bBowlingNoBall?TEXT("ui_result_sting"):TEXT("ui_button_click"),bBowlingNoBall?.35f:.22f);
    UE_LOG(LogC26,Log,TEXT("C26_BOWL_RELEASE meter=%.3f band=%s quality=%.2f noball=%d bar=[%.2f %.2f %.2f %.2f]"),
        ReleaseMeterValue,C26Delivery::BandName(ReleaseBand),ReleaseBandQuality,bBowlingNoBall?1:0,
        ActiveBar.EarlyStart,ActiveBar.GoodStart,ActiveBar.PerfectStart,ActiveBar.NoBallStart);
}
void AC26MatchGameMode::ReleaseBall()
{
    if(Phase!=EC26Phase::RunUp||BallReleased)return;
    BallReleased=true;
    if(!PlayerBatting()&&!AutoPlay)
    {
        if(!ReleaseLocked)
        {
            // No input at all: the bowler lets go anyway, raggedly. Standing
            // still is a bad delivery, not an automatic overstep - a no-ball has
            // to be something the player chose by pushing the bar too far.
            ReleaseLocked=true;
            ReleaseMeterValue=ActiveBar.GoodStart*0.92f;
            ReleaseBand=C26Delivery::BandAt(ReleaseMeterValue,ActiveBar);
            ReleaseBandQuality=C26Delivery::QualityAt(ReleaseMeterValue,ActiveBar);
            bBowlingNoBall=false;
            BowlingExecutionQuality=ReleaseBandQuality;
        }
        // THE delivery: the player's plan, their execution and this bowler's
        // ability, resolved once. Deterministic per delivery, never frame noise.
        const float Roll=C26Controls::MarkerJitterDir(DeliveryId*2654435761u).X*0.5f+0.5f;
        Bowling=C26Delivery::Compose(BowlingPlan,BowlerProfile,BowlingTuning,
            ReleaseBandQuality,C26Delivery::SignedError(ReleaseMeterValue,ActiveBar),
            bBowlingNoBall,BatterIsLeftHanded(),Preferences?Preferences->Difficulty:1,Roll);
        Detail=FString(C26Delivery::BandName(ReleaseBand));
        LastActualKph=C26Delivery::UnitsToKph(Bowling.Speed);
    }
    else
    {
        Bowling=C26Delivery::Execute(LockedBowling,ReleaseQuality);
        Detail=C26Delivery::ReleaseName(ReleaseQuality);
        LastActualKph=C26Delivery::UnitsToKph(Bowling.Speed);
    }
    Athletes[0]->SetAction(EC26Action::Bowling,false);Athletes[0]->ActionTime=C26Field::ReleasePoseTime;Athletes[0]->Animate(0);
    FVector Origin=Athletes[0]->HandPosition();
    // Crease position: bowling around the wicket delivers from the other side of
    // the stumps. The ball is still aimed at the chosen target; what changes is
    // the angle it arrives on, which is the whole point of changing the crease.
    Origin.X+=CreaseOffsetCm();
    LastBowlingOrigin=Origin;bHasBowlingOrigin=true;LastCreaseOffset=CreaseOffsetCm();
    if(bDebugTrace)DrawDebugSphere(GetWorld(),Origin,9.f,12,FColor::Green,false,8.f,0,1.2f);
    Simulation.Release(Bowling,Origin);Pending.NoBall=Bowling.NoBall;Director->MarkRelease();
    // GesturePro: the marker prediction and the bowling intent snapshot both
    // derive from this exact release, so they can never disagree with physics.
    CalculateBouncePrediction();
    {
        // Intended vs actual, so the accuracy model can be read rather than felt.
        BowlingIntendedPitch=FVector(BowlingPlan.TargetLine,BowlingPlan.TargetLength,8.f);
        FVector ActualPos;float ActualTime;
        if(Simulation.GetBouncePrediction(ActualPos,ActualTime))BowlingActualPitch=FVector(ActualPos.X,ActualPos.Y,8.f);
        else BowlingActualPitch=FVector(Simulation.ContactPosition.X,Simulation.ContactPosition.Y,8.f);
        LastActualPitch=BowlingActualPitch;bHasLastPitch=true;
    }
    if(!PlayerBatting()&&!AutoPlay)BowlingState=EC26BowlingState::BallInFlight;
    Audio->CueAt(TEXT("ball_release"),Origin,.20f);OnCricketEvent.Broadcast(TEXT("BallReleased"),Origin);
    // Read a future point on this SAME trajectory; the keeper moves before the take.
    const float TakeY=FieldPositions[1].Y-32.f;
    MissTakeAge=Simulation.ContactTime+FMath::Max(0.f,(TakeY-C26Field::ContactY)/(Bowling.Speed*.9f));
    MissTakeTarget=Simulation.Predict(MissTakeAge).Position;
    if(!PlayerBatting()||AutoPlay){Intent=AI.Bat(Bowling,Rules,Preferences->Difficulty);AITiming=AI.TimingError(Preferences->Difficulty);}
    if(ShotQueued)
    {
        // A shot released during the run-up keeps the delta it was released with;
        // it is simply a very early swing measured against the real contact time.
        if(bPendingShotFromGesture)ShotInputTime=Simulation.ContactTime-.10f+PendingShotError;
        else if(ShotInputTime<0.f)ShotInputTime=0.f;
        Athletes[11]->SetShotContact(Simulation.ContactPosition,Intent.Angle,Intent.Loft);
    }
    ChangePhase(EC26Phase::Delivery);
}
float AC26MatchGameMode::TimingCountdown()const{return Simulation.ContactTime-Simulation.Ball.Age;}
void AC26MatchGameMode::Shot(const FC26ShotIntent& NewIntent)
{
    if(Paused||SettingsOpen||ControlsOpen||(Phase!=EC26Phase::Delivery&&Phase!=EC26Phase::RunUp)||ShotQueued||(!PlayerBatting()&&!AutoPlay))return;
    ShotQueued=true;Intent=NewIntent;Intent.Footwork=Footwork;
    if(BattingState==EC26BattingState::ReadyForShot||BattingState==EC26BattingState::Pulling
        ||BattingState==EC26BattingState::Armed||BattingState==EC26BattingState::Released)
        BattingState=EC26BattingState::ShotCommitted;
    // One timing number. The gesture's measured release delta IS the simulation's
    // error, so the meter can never report something the bat did not do.
    if(Phase==EC26Phase::Delivery)
        ShotInputTime=bPendingShotFromGesture?(Simulation.ContactTime-.10f+PendingShotError):Simulation.Ball.Age;
    else
        ShotInputTime=-1.f; // resolved against the real contact time in ReleaseBall()
    if(Phase==EC26Phase::Delivery)Athletes[11]->SetShotContact(Simulation.ContactPosition,Intent.Angle,Intent.Loft);
}
void AC26MatchGameMode::AimPitch(float Line,float Length)
{
    // Continuous target: anywhere legal on the strip, never a preset grid.
    if(Phase!=EC26Phase::Ready||PlayerBatting()||Paused)return;
    BowlingPlan.TargetLine=FMath::Clamp(Line,-135.f,135.f);
    BowlingPlan.TargetLength=FMath::Clamp(Length,30.f,840.f);
    Bowling.Line=BowlingPlan.TargetLine;Bowling.Length=BowlingPlan.TargetLength;
    BowlingIntendedPitch=FVector(BowlingPlan.TargetLine,BowlingPlan.TargetLength,8.f);
    BowlingState=EC26BowlingState::TargetDrag;
    TrajectoryPreviewHash=0;
}
void AC26MatchGameMode::UpdateDelivery(float Dt)
{
    if(KeeperTakeClock>=0)
    {
        KeeperTakeClock+=Dt;
        Athletes[1]->ActionTime=.18f+KeeperTakeClock;Athletes[1]->Animate(0);
        Simulation.Ball.Position=Athletes[1]->ReceivingPosition();
        if(KeeperTakeClock>.34f)Resolve();
        return;
    }
    if(!MissTakeTarget.IsZero())
    {
        auto* Keeper=Athletes[1].Get();
        FVector Stance=Keeper->GetActorLocation();
        Stance.X=FMath::FInterpConstantTo(Stance.X,MissTakeTarget.X,Dt,260.f);
        Keeper->SetActorLocation(Stance);Keeper->ContactTarget=MissTakeTarget;
        if(Simulation.Ball.Age>MissTakeAge-.28f)Keeper->SetAction(EC26Action::Catch,false);
    }
    const bool AIAtBat=!PlayerBatting()||AutoPlay;
    if(AIAtBat&&!ShotQueued&&Simulation.Ball.Age>=Simulation.ContactTime-.10f+AITiming)
    {
        ShotQueued=true;ShotInputTime=Simulation.Ball.Age;Athletes[11]->SetShotContact(Simulation.ContactPosition,Intent.Angle,Intent.Loft);
    }
    // Stop precisely at the contact plane before applying a rebound; no jump to a distant bat.
    const float Remaining=Simulation.ContactTime-Simulation.Ball.Age;
    if(!Simulation.CrossedContact&&Remaining>=0&&Remaining<=Dt)
    {
        Simulation.Step(Remaining);
        if(Simulation.BounceEvent){Audio->CueAt(TEXT("ball_bounce"),Simulation.Ball.Position,.45f);Spark(Simulation.Ball.Position,false);}
        Simulation.CrossedContact=true;
        Dt-=Remaining;
        if(ShotQueued)
        {
            float Error=ShotInputTime-(Simulation.ContactTime-.10f);
            LastContact=Simulation.Hit(Intent,Error,Preferences->Difficulty,AI.Random);
            if(LastContact.Timing!=EC26Timing::Miss)
            {
                Athletes[11]->ContactTarget=Simulation.Ball.Position;Athletes[11]->ShotAngle=LastContact.FaceAngle;
                Athletes[11]->ActionTime=C26Field::BatContactPoseTime;Athletes[11]->Animate(0);
                // Anchor the replay and the shot cameras to the real moment of contact.
                Director->MarkContact(LastContact.Quality,Intent.Loft,Simulation.Ball.Position);
                OnCricketEvent.Broadcast(TEXT("BatContact"),Simulation.Ball.Position);
                // Four distinct contact voices: sweet middle, solid good,
                // dull mistime, thin edge. Never a bat sound on a miss.
                FName BatSound=TEXT("bat_mistimed");
                float BatVol=FMath::Lerp(.55f,1.f,LastContact.Quality);
                if(LastContact.Timing==EC26Timing::Edge)BatSound=TEXT("bat_edge");
                else if(Intent.Defend){BatSound=TEXT("bat_defensive");BatVol*=.85f;}
                else if(LastContact.Timing==EC26Timing::Perfect)BatSound=TEXT("bat_sweet_spot");
                else if(LastContact.Timing==EC26Timing::Good){BatSound=TEXT("bat_defensive");BatVol=FMath::Lerp(.6f,.9f,LastContact.Quality);}
                Audio->CueAt(BatSound,Simulation.Ball.Position,BatVol);
                // Delivery/shot analysis observes the true contact quality.
                {
                    FC26CommentaryContext DCtx=MakeCommentaryContext();
                    DCtx.ContactQuality=LastContact.Quality;
                    DCtx.TimingResult=(uint8)LastContact.Timing;
                    DCtx.bMistimed=LastContact.Timing==EC26Timing::Early||LastContact.Timing==EC26Timing::Late||LastContact.Quality<.45f;
                    DCtx.bBeaten=false;
                    DCtx.bEdge=LastContact.Timing==EC26Timing::Edge;
                    Audio->NotifyDelivery(DCtx);
                }
                // Small crowd swell under the bat sound: the ground rises as the ball travels, well
                // before any boundary call decides the outcome. Scaled by quality, never a six roar.
                Audio->Cue(TEXT("crowd_anticipation"),FMath::Lerp(.18f,.42f,LastContact.Quality));
                // The ground reacts to the strike, not to the scoreboard. A well-middled ball has
                // people up before anyone knows where it finished, which is most of why a boundary
                // in a real stadium feels inevitable half a second before it is one.
                Venue->React(FMath::Lerp(.10f,.62f,LastContact.Quality));
                if(bDebugTrace)DrawDebugSphere(GetWorld(),Simulation.Ball.Position,11.f,12,FColor(255,196,64),false,8.f,0,1.6f);
                Haptic(LastContact.Timing==EC26Timing::Perfect?.45f:.2f);HitStop(LastContact.Quality);AI.History.OffsideBias=FMath::Lerp(AI.History.OffsideBias,Intent.Angle>0?1.f:-1.f,.3f);
                Detail=LastContact.Shot;ChangePhase(EC26Phase::InPlay);return;
            }
        }
    }
    const float ToTake=FMath::Max(0.f,MissTakeAge-Simulation.Ball.Age);
    Simulation.Step(MissTakeAge>0?FMath::Min(Dt,ToTake):Dt);
    if(Simulation.BounceEvent){Audio->CueAt(TEXT("ball_bounce"),Simulation.Ball.Position,.4f);Spark(Simulation.Ball.Position,false);}
    if(Simulation.StumpEvent&&Effects)Effects->StumpBurst(Simulation.Ball.Position);
    if(Simulation.StumpEvent)
    {
        if(Pending.NoBall||Rules.Now().FreeHit){Simulation.Ball.Active=false;BreakWicket(C26Field::WicketY);Resolve();}
        else{Pending.Wicket=C26::Dismissal::Bowled;BreakWicket(C26Field::WicketY);Resolve();}return;
    }
    if((MissTakeAge>0&&Simulation.Ball.Age>=MissTakeAge-.001f)||Simulation.Ball.Age>2.f)
    {
        if(FMath::Abs(Simulation.ContactPosition.X)>90&&!Pending.NoBall)Pending.WideRuns=1;
        Athletes[1]->ContactTarget=Simulation.Ball.Position;Athletes[1]->SetAction(EC26Action::Catch);
        Athletes[1]->ActionTime=.18f;Athletes[1]->Animate(0);
        KeeperTakeClock=0;KeeperTake=true;Simulation.Ball.Active=false;
        Audio->CueAt(TEXT("keeper_catch"),Simulation.Ball.Position,.6f);OnCricketEvent.Broadcast(TEXT("KeeperTake"),Simulation.Ball.Position);
    }
}
void AC26MatchGameMode::Run()
{
    if(Phase!=EC26Phase::InPlay||Resolved||Paused||SettingsOpen||ControlsOpen||PhaseTime<.30f)return;
    RequestedRuns=FMath::Min(3,RequestedRuns+1);
    if(!Running)
    {
        RunFromA=Athletes[11]->GetActorLocation();RunFromB=Athletes[12]->GetActorLocation();
        RunToA=FVector(-45,RunFromA.Y>0?-C26Field::CreaseY:C26Field::CreaseY,5);
        RunToB=FVector(35,RunFromB.Y>0?-C26Field::CreaseY:C26Field::CreaseY,5);
        Running=true;Returning=false;RunProgress=0;RunVelocity=0;
        Athletes[11]->SetActorRotation((RunToA-RunFromA).Rotation());Athletes[12]->SetActorRotation((RunToB-RunFromB).Rotation());
        Athletes[11]->SetAction(EC26Action::Running);Athletes[12]->SetAction(EC26Action::Running);
        Athletes[11]->MoveSpeed=Tuning.RunnerSpeed;Athletes[12]->MoveSpeed=Tuning.RunnerSpeed;
    }
}
void AC26MatchGameMode::CancelRun()
{
    RequestedRuns=0;
    if(Running&&RunProgress<.45f&&!Returning)
    {
        Returning=true;RunVelocity*=.3f;
        Athletes[11]->SetActorRotation((RunFromA-RunToA).Rotation());
        Athletes[12]->SetActorRotation((RunFromB-RunToB).Rotation());
    }
}
void AC26MatchGameMode::UpdateRunning(float Dt)
{
    if(!Running)return;
    RunVelocity=FMath::FInterpConstantTo(RunVelocity,Tuning.RunnerSpeed,Dt,1000.f);
    RunProgress+=Dt*RunVelocity/FMath::Max(1.f,FVector::Dist2D(RunFromA,RunToA))*(Returning?-1.f:1.f);
    Athletes[11]->MoveSpeed=RunVelocity;Athletes[12]->MoveSpeed=RunVelocity;
    Athletes[11]->SetActorLocation(FMath::Lerp(RunFromA,RunToA,FMath::Clamp(RunProgress,0.f,1.f)));
    Athletes[12]->SetActorLocation(FMath::Lerp(RunFromB,RunToB,FMath::Clamp(RunProgress,0.f,1.f)));
    if(RunProgress<=0&&Returning)
    {Running=false;Returning=false;Athletes[11]->SetAction(EC26Action::Ready);Athletes[12]->SetAction(EC26Action::Ready);Athletes[11]->MoveSpeed=Athletes[12]->MoveSpeed=0;}
    if(RunProgress>=1)
    {
        ++CompletedRuns;Pending.CompletedRuns=CompletedRuns;
        if(Simulation.Ball.Struck)Pending.BatRuns=CompletedRuns;else Pending.Byes=CompletedRuns;
        Running=false;RequestedRuns=FMath::Max(0,RequestedRuns-1);
        Athletes[11]->SetAction(EC26Action::Ready);Athletes[12]->SetAction(EC26Action::Ready);
        Athletes[11]->MoveSpeed=0;Athletes[12]->MoveSpeed=0;
        if(Rules.Current==1&&Rules.Now().Runs+CompletedRuns+(Pending.NoBall?1:0)>=Rules.Target()){Resolve();return;}
        if(RequestedRuns>0){--RequestedRuns;Run();}
    }
}
void AC26MatchGameMode::Collect(int Fielder,bool Catch)
{
    ActiveFielder=Fielder;Simulation.Ball.Active=false;
    if(Effects&&!Catch)Effects->TurfScuff(Simulation.Ball.Position,Simulation.Ball.Velocity,.55f);
    GatherPoint=Simulation.Ball.Position;
    if(!Catch)
    {
        // Square the gather up. The athlete's hands finish about 46 cm in front of his root when
        // he bends through a pickup, so unless he is standing that far behind the ball and facing
        // it, the arm IK runs out of reach and the ball has to be teleported into his gloves --
        // exactly the fake this project refuses to ship. He is already decelerating here and the
        // correction is under half a metre, so it costs nothing visually and buys a real gather.
        auto* F=Athletes[Fielder].Get();
        FVector Approach=GatherPoint-F->GetActorLocation();Approach.Z=0;
        if(Approach.SizeSquared()<25.f)Approach=F->GetActorForwardVector()*10.f;
        Approach=Approach.GetSafeNormal();
        F->SetActorRotation(Approach.Rotation());
        FVector Stand=GatherPoint-Approach*46.f;Stand.Z=F->GetActorLocation().Z;
        F->SetActorLocation(Stand);
    }
    Athletes[Fielder]->ContactTarget=GatherPoint;
    Athletes[Fielder]->SetAction(Catch?EC26Action::Catch:EC26Action::Pickup);
    Athletes[Fielder]->MoveSpeed=0;Athletes[Fielder]->Animate(0);
    if(Catch)
    {
        Athletes[Fielder]->ActionTime=.18f;Athletes[Fielder]->Animate(0);
        Audio->CueAt(TEXT("keeper_catch"),GatherPoint,.6f);Haptic(.24f);
        OnCricketEvent.Broadcast(TEXT("Catch"),GatherPoint);
    }
    if(Catch&&!Pending.NoBall&&!Rules.Now().FreeHit){CatchClock=0;return;}
    const FVector EndA=Returning?RunFromA:RunToA,EndB=Returning?RunFromB:RunToB;
    ThrowClock=0;ThrowReleased=false;
    ThrowRunner=Running?(FVector::Dist2D(Athletes[Fielder]->GetActorLocation(),EndA)<FVector::Dist2D(Athletes[Fielder]->GetActorLocation(),EndB)?0:1):0;
    ThrowTo=Running?(ThrowRunner==0?EndA:EndB):FVector(0,C26Field::WicketY,0);
    ThrowTo.X=0;ThrowTo.Y=ThrowTo.Y>0?C26Field::WicketY:-C26Field::WicketY;ThrowTo.Z=42;
}
void AC26MatchGameMode::UpdateFieldPresence(float Dt)
{
    if(Athletes.Num()<12)return;
    const FVector Striker=Athletes[11]->GetActorLocation();
    const bool WalkingIn=Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery;
    if(WalkingIn)
    {
        // The ring walks in with the bowler and pulls up as he loads. Cricket's most recognisable
        // piece of off-the-ball movement, and about 190 cm of ground each: enough to read clearly
        // from the batting camera, small enough that it never disturbs a saved field placement.
        const float Ease=Phase==EC26Phase::Delivery?FMath::Max(0.f,1.f-PhaseTime*5.f):1.f;
        const float Step=175.f*Ease*Dt;
        FieldCreep=FMath::Min(FieldCreep+Step,190.f);
        for(int I=2;I<11;++I)
        {
            auto* F=Athletes[I].Get();
            FVector In=Striker-FieldPositions[I];In.Z=0;
            if(In.IsNearlyZero())continue;
            In=In.GetSafeNormal();
            FVector Want=FieldPositions[I]+In*FieldCreep;Want.Z=F->GetActorLocation().Z;
            F->SetActorLocation(Want);
            F->SetActorRotation(In.Rotation());
            F->MoveSpeed=Step>0.f?175.f*Ease:0.f;
            F->SetAction(F->MoveSpeed>18.f?EC26Action::Running:EC26Action::Ready,false);
        }
        return;
    }
    if(Phase!=EC26Phase::InPlay)return;
    // Everyone who is not chasing still turns and follows the ball. A fielder frozen square to the
    // wicket while the ball flies past him is what makes a crowd of athletes read as scenery.
    const FVector Focus=Simulation.Ball.Active?Simulation.Ball.Position:LastContact.ContactPoint;
    if(Focus.IsZero())return;
    for(int I=1;I<11;++I)
    {
        if(I==ActiveFielder||I==BackupFielder)continue;
        auto* F=Athletes[I].Get();
        if(F->Action!=EC26Action::Ready)continue;
        FVector To=Focus-F->GetActorLocation();To.Z=0;
        if(To.SizeSquared()<400.f)continue;
        F->SetActorRotation(FMath::RInterpTo(F->GetActorRotation(),To.Rotation(),Dt,4.5f));
    }
}
void AC26MatchGameMode::UpdateFielding(float Dt)
{
    if(CatchClock>=0)
    {
        CatchClock+=Dt;Athletes[ActiveFielder]->ActionTime=.18f+CatchClock;Athletes[ActiveFielder]->Animate(0);
        Simulation.Ball.Position=Athletes[ActiveFielder]->ReceivingPosition();
        if(CatchClock>.32f){Pending.Wicket=C26::Dismissal::Caught;CatchClock=-1;Resolve();}
        return;
    }
    if(ThrowClock>=0)
    {
        ThrowClock+=Dt;
        if(ThrowClock<.53f)
        {
            auto* F=Athletes[ActiveFielder].Get();F->ActionTime=ThrowClock;F->Animate(0);
            if(ThrowClock<.20f)
            {
                // Close the last few centimetres against the posed hands rather than against a
                // guess. A fixed stand-off cannot be right for every ball height: the reach a
                // gather actually has depends on how far the athlete gets down, and when it comes
                // up short the two-bone IK clamps silently and the ball has to be teleported into
                // his gloves. Steering on the measured error means he takes the extra half step,
                // which is what a fielder does and what the camera expects to see.
                FVector Error=GatherPoint-F->ReceivingPosition();Error.Z=0;
                if(!Error.IsNearlyZero())F->AddActorWorldOffset(Error.GetClampedToMaxSize(210.f*Dt));
            }
            if(ThrowClock>=.20f)
            {
                Simulation.Ball.Position=F->ReceivingPosition();
                if(ThrowClock-Dt<.20f){Audio->CueAt(TEXT("fielder_gather"),GatherPoint,.55f);OnCricketEvent.Broadcast(TEXT("Pickup"),GatherPoint);}
            }
            return;
        }
        if(Athletes[ActiveFielder]->Action!=EC26Action::Throw)Athletes[ActiveFielder]->SetAction(EC26Action::Throw);
        const FRotator Aim=(ThrowTo-Athletes[ActiveFielder]->GetActorLocation()).Rotation();
        Athletes[ActiveFielder]->SetActorRotation(FMath::RInterpTo(Athletes[ActiveFielder]->GetActorRotation(),FRotator(0,Aim.Yaw,0),Dt,12.f));
        Athletes[ActiveFielder]->ActionTime=FMath::Min(.5f,ThrowClock-.53f);Athletes[ActiveFielder]->Animate(0);
        if(ThrowClock<.73f){Simulation.Ball.Position=Athletes[ActiveFielder]->HandPosition();return;}
        if(!ThrowReleased)
        {
            Athletes[ActiveFielder]->ActionTime=.2f;Athletes[ActiveFielder]->Animate(0);
            ThrowFrom=Athletes[ActiveFielder]->HandPosition();ThrowDuration=FMath::Max(.18f,FVector::Dist(ThrowFrom,ThrowTo)/2600.f);ThrowReleased=true;ThrowClock=.73f;
            OnCricketEvent.Broadcast(TEXT("ThrowReleased"),ThrowFrom);Audio->CueAt(TEXT("ball_release"),ThrowFrom,.14f);
        }
        const float T=FMath::Clamp((ThrowClock-.73f)/ThrowDuration,0.f,1.f);
        Simulation.Ball.Position=FMath::Lerp(ThrowFrom,ThrowTo,T)+FVector(0,0,.5f*Tuning.Gravity*ThrowDuration*ThrowDuration*T*(1.f-T));
        // Cover the receiving wicket with an athlete before the return arrives.
        const int Receiver=ThrowTo.Y>0?1:0;
        if(Receiver!=ActiveFielder)
        {
            auto* R=Athletes[Receiver].Get();
            const FVector Mark=ThrowTo+FVector(0,ThrowTo.Y>0?30.f:-30.f,-37.f);
            R->SetActorLocation(FMath::VInterpConstantTo(R->GetActorLocation(),Mark,Dt,850.f));
            R->SetActorRotation(FRotator(0,ThrowTo.Y>0?-90.f:90.f,0));R->ContactTarget=ThrowTo;
            if(T>.6f)R->SetAction(EC26Action::Catch,false);
        }
        if(T>=1)
        {
            const float EndSign=ThrowTo.Y>0?1.f:-1.f;
            const bool Grounded=Athletes[ThrowRunner==0?11:12]->GetActorLocation().Y*EndSign>=C26Field::CreaseY-6.f;
            if(Running&&!Grounded)
            {
                Pending.Wicket=C26::Dismissal::RunOut;Pending.DismissedBatter=ThrowRunner==0?RunnerAId:RunnerBId;Pending.CrossedOnRunOut=RunProgress>.5f;
                BreakWicket(ThrowTo.Y);
            }
            Audio->CueAt(TEXT("keeper_catch"),ThrowTo,.45f);Resolve();
        }return;
    }
    Simulation.Step(Dt);
    if(Simulation.StumpEvent)
    {
        if(Effects)Effects->StumpBurst(Simulation.Ball.Position);
        if(Simulation.Ball.Position.Y>0&&!Pending.NoBall&&!Rules.Now().FreeHit)Pending.Wicket=C26::Dismissal::Bowled;
        BreakWicket(Simulation.Ball.Position.Y);Resolve();return;
    }
    if(Simulation.BounceEvent){Audio->CueAt(TEXT("ball_bounce"),Simulation.Ball.Position,.28f);Spark(Simulation.Ball.Position,true);}
    if(Simulation.BoundaryEvent)
    {
        Pending.Rope=Simulation.Ball.PostHitBounce?C26::Boundary::Four:C26::Boundary::Six;
        Pending.BatRuns=Pending.Rope==C26::Boundary::Six?6:4;Pending.CompletedRuns=0;
        ++AI.History.Boundaries;Resolve();return;
    }
    FieldDecisionClock-=Dt;
    if(FieldDecisionClock<=0)
    {
        FieldDecisionClock=.2f;float Best=BIG_NUMBER;int Chosen=-1;FVector Target=Simulation.PredictLanding();
        Simulation.Forecast(FieldForecast,5.1f,.125f);
        float CurrentCost=BIG_NUMBER;FVector CurrentTarget=Target;
        for(int I=0;I<11;++I)
        {
            for(const auto& P:FieldForecast)
            {
                if(P.Position.Z>190||!C26Field::Inside(P.Position))continue;
                const float T=P.Age-Simulation.Ball.Age;
                const float Needed=FVector::Dist2D(Athletes[I]->GetActorLocation(),P.Position)/Tuning.FielderSpeed+.24f;
                if(Needed<T)
                {
                    if(I==ActiveFielder){CurrentCost=T;CurrentTarget=P.Position;}
                    if(T<Best){Best=T;Chosen=I;Target=P.Position;}
                    break;
                }
            }
        }
        if(ActiveFielder>=0&&CurrentCost<=Best+.3f){Chosen=ActiveFielder;Target=CurrentTarget;}
        if(Chosen<0)
        {
            for(int I=0;I<11;++I){float D=FVector::DistSquared2D(Athletes[I]->GetActorLocation(),Target);if(D<Best){Best=D;Chosen=I;}}
        }
        ActiveFielder=Chosen;Intercept=Target;Intercept.Z=5;
        BackupFielder=-1;float BackupDistance=BIG_NUMBER;
        for(int I=2;I<11;++I)if(I!=ActiveFielder)
        {const float D=FVector::DistSquared2D(Athletes[I]->GetActorLocation(),Intercept);if(D<BackupDistance){BackupDistance=D;BackupFielder=I;}}
        Director->SetFieldingTarget(Intercept,ActiveFielder>=0,Running);
    }
    for(int I=0;I<11;++I)
    {
        auto F=Athletes[I];
        if((I==ActiveFielder||I==BackupFielder)&&PhaseTime>.24f)
        {
            FVector Target=Intercept;
            if(I==BackupFielder)
            {Target+=(Intercept-FVector(0,C26Field::ContactY,0)).GetSafeNormal2D()*550.f;Target=Target.GetClampedToMaxSize(6250.f);}
            // Inside the last couple of metres the forecast point is stale -- the ball has already
            // arrived or been deflected. Track the live ball so the gather is an arrival, not a
            // stop beside it.
            else if(FVector::Dist2D(F->GetActorLocation(),Simulation.Ball.Position)<210.f)
            {Target=Simulation.Ball.Position;Target.Z=Intercept.Z;}
            FVector Delta=Target-F->GetActorLocation();Delta.Z=0;
            const float Speed=FMath::Min(Tuning.FielderSpeed*(I==ActiveFielder?1.f:.7f),FMath::Sqrt(2.f*900.f*Delta.Size()));
            F->MoveSpeed=FMath::FInterpConstantTo(F->MoveSpeed,Speed,Dt,1050.f);
            if(Delta.Size()>28){F->SetActorRotation(FMath::RInterpTo(F->GetActorRotation(),Delta.Rotation(),Dt,9.f));F->AddActorWorldOffset(Delta.GetClampedToMaxSize(F->MoveSpeed*Dt));F->SetAction(EC26Action::Running,false);}
            else{F->SetAction(Simulation.Ball.Position.Z>55?EC26Action::Catch:EC26Action::Ready,false);F->MoveSpeed=0;}
            if(I==BackupFielder)continue;
            const float D=FVector::Dist2D(F->GetActorLocation(),Simulation.Ball.Position);
            F->ContactTarget=Simulation.Ball.Position;
            if(D<43&&Simulation.Ball.Position.Z<190&&Simulation.Ball.Velocity.Z<40)
            {
                const bool Catch=!Simulation.Ball.PostHitBounce&&Simulation.Ball.Position.Z>32;
                if(Catch&&AI.Random.FRand()>(Preferences->Difficulty==0?.82f:.94f))
                {Simulation.Ball.Velocity*=.35f;Simulation.Ball.PostHitBounce=true;Detail=TEXT("PUT DOWN!");}
                else{Collect(I,Catch);return;}
            }
        }
        else if(F->Action==EC26Action::Running)F->SetAction(EC26Action::Ready);
    }
    if((!PlayerBatting()||AutoPlay)&&!Running&&CompletedRuns<3)
    {
        float Recovery=ActiveFielder>=0?FVector::Dist2D(Athletes[ActiveFielder]->GetActorLocation(),Simulation.Ball.Position)/Tuning.FielderSpeed:4;
        const float Return=FVector::Dist2D(Simulation.Ball.Position,FVector(0,884,0))/2400.f;
        if(Recovery+Return>3.f&&Simulation.Ball.Age>.8f)Run();
    }
    if(Simulation.Ball.Age>13.f){UE_LOG(LogC26,Warning,TEXT("Delivery %u field timeout: settling live ball at %s"),DeliveryId,*Simulation.Ball.Position.ToString());Resolve();}
}
void AC26MatchGameMode::Resolve()
{
    if(Resolved)return;Resolved=true;Running=false;Simulation.Ball.Active=false;
    for(auto A:Athletes)if(A->Action==EC26Action::Running){A->MoveSpeed=0;A->SetAction(EC26Action::Ready);}
    const auto Result=Rules.Apply(Pending);
    if(Result!=C26::Commit::Accepted)
    {
        UE_LOG(LogC26,Error,TEXT("Rejected outcome commit=%d phase=%d delivery=%u epoch=%u innings=%d ball=%d"),int(Result),int(Phase),Pending.Id,Pending.Epoch,Rules.Current,Rules.Now().LegalBalls);
        return;
    }
    const auto& Official=Rules.Now().Ledger.back();
    if(Official.Wicket!=C26::Dismissal::None)
    {
        Callout=TEXT("WICKET");Detail=Official.Wicket==C26::Dismissal::Bowled?TEXT("BOWLED"):Official.Wicket==C26::Dismissal::Caught?TEXT("CAUGHT"):TEXT("RUN OUT");
        Important=true;++SmokeWickets;Athletes[13]->SetAction(EC26Action::SignalOut);
        Athletes[11]->SetAction(EC26Action::Disappointed);for(int I=0;I<11;++I)Athletes[I]->SetAction(EC26Action::Celebrate);
    }
    else if(Official.Rope!=C26::Boundary::None)
    {
        bool Six=Official.Rope==C26::Boundary::Six;Callout=Six?TEXT("SIX"):TEXT("FOUR");Detail=LastContact.Shot;
        Important=true;++SmokeBoundaries;
        Athletes[13]->SetAction(Six?EC26Action::SignalSix:EC26Action::SignalFour);Athletes[11]->SetAction(EC26Action::Celebrate);
    }
    else if(Official.NoBall){Callout=TEXT("NO BALL");Detail=TEXT("FREE HIT NEXT DELIVERY");++SmokeExtras;Athletes[13]->SetAction(EC26Action::SignalWide);}
    else if(Official.WideRuns){Callout=TEXT("WIDE");Detail=TEXT("EXTRA RUN  /  BALL DOES NOT COUNT");++SmokeExtras;Athletes[13]->SetAction(EC26Action::SignalWide);}
    else{int Runs=Official.BatRuns+Official.Byes+Official.LegByes;Callout=Runs?FString::Printf(TEXT("%d %s"),Runs,Runs==1?TEXT("RUN"):TEXT("RUNS")):TEXT("DOT BALL");Detail=Runs?TEXT("GOOD RUNNING BETWEEN THE WICKETS"):TEXT("PRESSURE BUILDS");}
    // Tell the director what it is covering so the reaction and replay cut to the right subject.
    const FVector Focus=Official.Wicket==C26::Dismissal::Bowled?FVector(0,C26Field::WicketY,45)
        :Official.Wicket==C26::Dismissal::RunOut?FVector(0,ThrowTo.Y,45)
        :Official.Wicket==C26::Dismissal::Caught&&Athletes.IsValidIndex(ActiveFielder)&&ActiveFielder>=0?Athletes[ActiveFielder]->GetActorLocation()+FVector(0,0,120)
        :Simulation.Ball.Position;
    Director->MarkOutcome(FName(*Callout),Focus);
    // Single authoritative commentary call per delivery: wicket XOR result.
    // Crowd reactions are owned by the CrowdDirector inside these notifies.
    {
        FC26CommentaryContext RCtx=MakeCommentaryContext();
        RCtx.RunsScored=Official.BatRuns+Official.Byes+Official.LegByes;
        RCtx.bFour=Official.Rope==C26::Boundary::Four;
        RCtx.bSix=Official.Rope==C26::Boundary::Six;
        RCtx.bWicket=Official.Wicket!=C26::Dismissal::None;
        RCtx.WicketType=Official.Wicket==C26::Dismissal::Bowled?1:Official.Wicket==C26::Dismissal::Caught?2:Official.Wicket==C26::Dismissal::RunOut?3:0;
        RCtx.bEdge=LastContact.Timing==EC26Timing::Edge;
        RCtx.bMatchWinning=Rules.Winner!=C26::Result::Playing;
        if(RCtx.bWicket)
        {
            Audio->NotifyWicket(RCtx);
            if(CommentaryDirector)CommentaryDirector->OnWicket(MakeCommentaryEvent(ECommentaryEventType::Wicket,0,false,false,true,RCtx.WicketType));
        }
        else
        {
            Audio->NotifyResult(RCtx);
            if(CommentaryDirector)
            {
                ECommentaryEventType EvType = RCtx.bSix ? ECommentaryEventType::Six : (RCtx.bFour ? ECommentaryEventType::Four : (RCtx.RunsScored <= 0 ? ECommentaryEventType::Dot : ECommentaryEventType::Single));
                CommentaryDirector->OnBallCompleted(MakeCommentaryEvent(EvType, RCtx.RunsScored, RCtx.bFour || RCtx.bSix, RCtx.bSix, false, 0));
            }
        }
        Audio->NoteBallCompleted(RCtx.RunsScored,RCtx.bWicket,RCtx.bFour||RCtx.bSix);
    }
    if(!Important&&Official.Wicket==C26::Dismissal::None&&Official.BatRuns==0)
        Athletes[11]->SetAction(EC26Action::Disappointed);
    else if(Official.Rope!=C26::Boundary::None)Athletes[0]->SetAction(EC26Action::Disappointed);
    if(Official.Wicket!=C26::Dismissal::None)OnCricketEvent.Broadcast(TEXT("Wicket"),Focus);
    else if(Official.Rope!=C26::Boundary::None)OnCricketEvent.Broadcast(TEXT("Boundary"),Focus);
    if(Rules.Now().Closed)OnCricketEvent.Broadcast(TEXT("OverComplete"),Focus);
    Venue->React(Important?1.f:.22f);OnMatchChanged.Broadcast();ChangePhase(EC26Phase::Reaction);
}
void AC26MatchGameMode::AfterPresentation()
{
    if(Rules.Winner!=C26::Result::Playing)
    {
        const int Winner=Rules.Winner==C26::Result::FirstTeam?FirstBattingTeam:1-FirstBattingTeam;
        Callout=Rules.Winner==C26::Result::Tie?TEXT("MATCH TIED"):Winner==PlayerTeam?TEXT("VICTORY"):TEXT("DEFEAT");
        Detail=Rules.Winner==C26::Result::Tie?TEXT("LEVEL AFTER TWO SUPER OVERS"):TeamName(Winner)+TEXT(" WIN THE SUPER OVER");
        Audio->Cue(TEXT("ui_result_sting"),Preferences->MusicVolume);Athletes[11]->SetAction(Winner==BattingTeam()?EC26Action::Celebrate:EC26Action::Disappointed);
        Audio->NotifyMatchResult(Winner==PlayerTeam,Rules.Winner==C26::Result::Tie);
        if(CommentaryDirector)
        {
            ECommentaryEventType ResType = (Rules.Winner==C26::Result::Tie)?ECommentaryEventType::MatchTie:(Winner==PlayerTeam?ECommentaryEventType::MatchWin:ECommentaryEventType::MatchLoss);
            CommentaryDirector->OnMatchResult(MakeCommentaryEvent(ResType));
        }
        ChangePhase(EC26Phase::Result);Venue->React(1.f);OnCricketEvent.Broadcast(TEXT("MatchComplete"),Athletes[11]->GetActorLocation());
        UE_LOG(LogC26,Display,TEXT("C26_MATCH_COMPLETE epoch=%u first=%d/%d second=%d/%d result=%d"),Rules.Epoch,Rules.Scores[0].Runs,Rules.Scores[0].Wickets,Rules.Scores[1].Runs,Rules.Scores[1].Wickets,int(Rules.Winner));
    }
    else if(Rules.Now().Closed)
    {Callout=FString::Printf(TEXT("TARGET %d"),Rules.Target());Detail=TEXT("SIX BALLS TO MAKE IT YOURS");Audio->Cue(TEXT("ui_result_sting"),.4f);Audio->NotifyInningsBreak();if(CommentaryDirector)CommentaryDirector->OnInningsBreak(MakeCommentaryEvent(ECommentaryEventType::InningsBreak));ChangePhase(EC26Phase::Interval);}
    else PrepareDelivery();
}
void AC26MatchGameMode::Skip()
{
    if(Phase==EC26Phase::Intro)PrepareDelivery();
    else if(Phase==EC26Phase::Replay){Director->Restore(Athletes);AfterPresentation();}
    else if(Phase==EC26Phase::Reaction)AfterPresentation();
    else if(Phase==EC26Phase::Interval){if(Rules.StartChase())PrepareDelivery();}
}
void AC26MatchGameMode::UpdateCapture(float Dt)
{
#if !UE_BUILD_SHIPPING
    if(!ProbeName.IsEmpty())
    {
        if(PhaseTime>4.f&&!ProbeCaptured)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Artifacts")/(ProbeName+TEXT(".png")),true,false);
            ProbeCaptured=true;
            UE_LOG(LogC26,Display,TEXT("C26_PROBE %s"),*ProbeName);
        }
        if(PhaseTime>5.f)FPlatformMisc::RequestExit(false);
        return;
    }
#endif
    // Deterministic visual-acceptance pass: drive the presentation and write one frame per beat.
    if(!Capture)return;
    if(CaptureHold>0){CaptureHold-=Dt;return;}
    // The screenshot request captures a LATER frame, so transient beat state
    // (pause overlay) must survive until that frame has rendered.
    if(ClearPauseNext){Paused=false;ClearPauseNext=false;}
    if(CaptureIndex>=UE_ARRAY_COUNT(GC26Beats))
    {
        UE_LOG(LogC26,Display,TEXT("C26_SHOTS_COMPLETE captured=%d"),CaptureIndex);
        Capture=false;SettingsOpen=ControlsOpen=false;Paused=false;FPlatformMisc::RequestExit(false);return;
    }
    const FC26Beat& Beat=GC26Beats[CaptureIndex];
    if(FCString::Strcmp(Beat.Name,TEXT("19_replay"))==0&&Phase==EC26Phase::Reaction)Important=true;
    // Fire the beat's navigation exactly once, then let the screen settle.
    if(Beat.Nav&&CaptureWait==0&&!CaptureShotsNavFired)
    {
        UIAction(Beat.Nav);CaptureShotsNavFired=true;CaptureHold=.6f;CaptureWait+=Dt;return;
    }
    const bool WantsPause=Beat.Overlay==3;
    if(WantsPause&&!Paused){Paused=true;CaptureHold=.5f;return;}
    const bool WantsOldOverlay=Beat.Overlay==1||Beat.Overlay==2;
    if(WantsOldOverlay&&!(Beat.Overlay==1?SettingsOpen:ControlsOpen))
    {
        SettingsOpen=Beat.Overlay==1;ControlsOpen=Beat.Overlay==2;CaptureHold=.45f;return;
    }
    const bool Matches=Phase==Beat.Phase&&(Beat.Batting<0||PlayerBatting()==(Beat.Batting==1))
        &&(Beat.Screen<0||MenuScreen==Beat.Screen)&&(Beat.Overlay!=3||Paused)
        &&(Beat.Toss<=0||TossStage>=Beat.Toss);
    if(!Matches)
    {
        SettingsOpen=ControlsOpen=false;if(!WantsPause)Paused=false;CaptureWait+=Dt;
        if(CaptureWait>Beat.Timeout)
        {
            UE_LOG(LogC26,Warning,TEXT("C26_SHOT_SKIPPED %s: state skipped or timed out in %.1fs (phase=%d)"),Beat.Name,CaptureWait,int(Phase));
            CaptureWait=0;CaptureShotsNavFired=false;++CaptureIndex;
        }
        return;
    }
    if(PhaseTime<Beat.After&&!(Beat.Overlay==3&&Beat.After<1.f)){CaptureWait+=Dt;return;}
    // Pause overlay needs a settled frame before the shot is taken.
    if(Beat.Overlay==3&&CaptureWait<.35f){CaptureWait+=Dt;return;}
    FString Directory=FPaths::ProjectDir()/TEXT("Artifacts/Shots");
    FParse::Value(FCommandLine::Get(),TEXT("C26ShotsDir="),Directory);
    const FString File=Directory/FString(Beat.Name)+TEXT(".png");
    FScreenshotRequest::RequestScreenshot(File,true,false);
    UE_LOG(LogC26,Display,TEXT("C26_SHOT %s -> %s"),Beat.Name,*File);
    ++CaptureIndex;CaptureWait=0;CaptureShotsNavFired=false;CaptureHold=.35f;
    if(Beat.Overlay==1||Beat.Overlay==2){SettingsOpen=ControlsOpen=false;}
    if(Beat.Overlay==3){ClearPauseNext=true;}
}
void AC26MatchGameMode::UpdateBallVisual()
{if(BallMesh){BallMesh->SetWorldLocation(Simulation.Ball.Position);BallMesh->AddLocalRotation(FRotator(11,4,0));}}
void AC26MatchGameMode::Haptic(float Strength)
{if(Preferences->Vibration)if(auto* P=GetWorld()->GetFirstPlayerController())P->PlayDynamicForceFeedback(Strength,.08f,true,true,true,true);}
void AC26MatchGameMode::Tick(float Dt)
{
    Super::Tick(Dt);if(!Director||Athletes.Num()!=14)return;
    if(HitStopUntil>0&&GetWorld()->GetRealTimeSeconds()>=HitStopUntil)ClearHitStop();
    Dt=FMath::Min(Dt,.05f);Clock+=Dt;UpdateCapture(Dt);
    if(ScreenFade<1.f)ScreenFade=FMath::Min(1.f,ScreenFade+Dt*3.2f);
    // Frontend toss coin animation runs on the menu clock, never gameplay.
    if(Phase==EC26Phase::Menu&&MenuScreen==4&&TossStage==1)
    {
        TossClock+=Dt;
        if(TossClock>=1.5f){TossStage=2;Audio->Cue(TEXT("ui_result_sting"),.35f);Haptic(.3f);}
    }
    if(Paused||SettingsOpen||ControlsOpen)return;
    const EC26Phase PhaseBeforeUpdate=Phase;
    PhaseTime+=Dt;
    // GesturePro presentation-state upkeep (never touches simulation truth).
    if(BattingCommitAge>=0.f)BattingCommitAge+=Dt;
    if(bBattingGestureActive&&(Phase==EC26Phase::Delivery||Phase==EC26Phase::RunUp))BattingGestureHoldTime+=Dt;
    if(!PlayerBatting()&&!AutoPlay)
    {
        if(Paused||SettingsOpen||ControlsOpen)CancelBowlingDrags();
        if(Phase==EC26Phase::Ready)BuildTrajectoryPreview();
        // The release window opens as the arm comes over, which is where the bar
        // becomes live; before that a tap is simply too early to be a release.
        if(Phase==EC26Phase::RunUp&&!ReleaseLocked&&BowlingMeter()>=ActiveBar.EarlyStart)
            BowlingState=EC26BowlingState::ReleaseWindow;
    }
    // A live gesture must never survive a state that can no longer play a shot.
    if(bBattingGestureActive)
    {
        if(Paused||SettingsOpen||ControlsOpen)CancelBattingGesture(TEXT("paused"));
        else if(Phase!=EC26Phase::RunUp&&Phase!=EC26Phase::Delivery)CancelBattingGesture(TEXT("delivery ended"));
        else if(ShotQueued)CancelBattingGesture(TEXT("shot already committed"));
        else if(!PlayerBatting()||AutoPlay)CancelBattingGesture(TEXT("no longer batting"));
        else if(GestureDeliveryId!=DeliveryId)CancelBattingGesture(TEXT("stale delivery"));
    }
    // Marker lives across the whole run-up AND the whole flight.
    if(Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)UpdateBouncePrediction(Dt);
    else if(BouncePrediction.bVisible){BouncePrediction.bVisible=false;BouncePrediction.Alpha=0.f;}
    if(Phase==EC26Phase::Delivery&&ShotQueued&&BattingState==EC26BattingState::ShotCommitted)
        BattingState=EC26BattingState::Contact;
    if(ShotQueued&&Phase==EC26Phase::InPlay&&BattingState!=EC26BattingState::FollowThrough&&BattingState!=EC26BattingState::Recovery)
        BattingState=EC26BattingState::FollowThrough;
    if((Phase==EC26Phase::Reaction||Phase==EC26Phase::Replay)&&BattingState!=EC26BattingState::Idle&&BattingState!=EC26BattingState::Recovery)
        BattingState=EC26BattingState::Recovery;
    if(Phase==EC26Phase::Reaction||Phase==EC26Phase::Replay||Phase==EC26Phase::Ready)CancelBattingGesture(TEXT("phase reset"));
    if(BattingState==EC26BattingState::Recovery&&Phase==EC26Phase::Ready)BattingState=EC26BattingState::Idle;
    if(Phase==EC26Phase::Intro&&PhaseTime>6.5f)PrepareDelivery();
    else if(Phase==EC26Phase::Ready&&AutoPlay&&PhaseTime>(Capture?1.1f:.45f))StartDelivery();
    else if(Phase==EC26Phase::RunUp)
    {
        const float T=FMath::Clamp(PhaseTime/C26Field::RunUpDuration,0.f,1.f);
        const float Travel=T*T*(2.f-T);
        Athletes[0]->SetActorLocation(FVector(-20,FMath::Lerp(-2700.f,-995.f,Travel),5));
        Athletes[0]->MoveSpeed=1705.f/C26Field::RunUpDuration*(4*T-3*T*T);
        // Distance-based footfalls: a step roughly every 95 cm of ground covered, so the sound
        // matches the stride at any point of the acceleration curve. Deliberately quiet.
        const float BowlerY=Athletes[0]->GetActorLocation().Y;
        if(BowlerY-LastStepY>95.f){LastStepY=BowlerY;Audio->CueAt(TEXT("runup_step"),Athletes[0]->GetActorLocation(),.20f);}
        if(PhaseTime>C26Field::RunUpDuration-C26Field::ReleasePoseTime)
        {Athletes[0]->SetAction(EC26Action::Bowling,false);Athletes[0]->ActionTime=PhaseTime-(C26Field::RunUpDuration-C26Field::ReleasePoseTime);}
        if(!FootPlanted&&Effects&&PhaseTime>C26Field::RunUpDuration-.14f)
        {
            FootPlanted=true;
            Effects->FootPlant(Athletes[0]->GetActorLocation()+FVector(18,86,0),.85f);
            Audio->CueAt(TEXT("foot_plant"),Athletes[0]->GetActorLocation(),.5f);
        }
        if(PhaseTime>=C26Field::RunUpDuration)ReleaseBall();
    }
    else if(Phase==EC26Phase::Delivery)UpdateDelivery(Dt);
    else if(Phase==EC26Phase::InPlay){UpdateRunning(Dt);if(!Resolved)UpdateFielding(Dt);}
    else if(Phase==EC26Phase::Reaction&&PhaseTime>(Important?1.5f:1.1f))
    {if(Important&&Director->BeginReplay(Athletes,Simulation.Ball.Position)){++SmokeReplays;ChangePhase(EC26Phase::Replay);Audio->Cue(TEXT("ui_button_click"),.25f);}else AfterPresentation();}
    else if(Phase==EC26Phase::Replay){if(!Director->PlayReplay(Dt,Simulation.Ball.Position,Athletes))AfterPresentation();}
    else if(Phase==EC26Phase::Interval&&AutoPlay&&PhaseTime>1.2f)Skip();
    else if(Phase==EC26Phase::Result&&Smoke&&PhaseTime>.5f)
    {
        ++SmokeMatches;
        if(SmokeMatches>=10)
        {
            UE_LOG(LogC26,Display,TEXT("C26_SMOKE_PASS matches=%d boundaries=%d wickets=%d replays=%d extras=%d actors=%d"),SmokeMatches,SmokeBoundaries,SmokeWickets,SmokeReplays,SmokeExtras,Athletes.Num());
            FPlatformMisc::RequestExit(false);Smoke=false;
        }
        else{PlayerBatsFirst=!PlayerBatsFirst;StartMatch();}
    }
    if(Phase!=EC26Phase::Replay)
    {
        // Backlift load. The run-up ramp and the player's pull both raise the bat;
        // neither one swings it. Only ReleaseBattingGesture -> Shot() does that.
        const float RunUpLoad=Phase==EC26Phase::RunUp?FMath::Clamp((PhaseTime-(C26Field::RunUpDuration-.65f))/.65f,0.f,1.f):0.f;
        const float PullLoad=bBattingGestureActive?FMath::Clamp(0.25f+BattingPullFrac*0.70f,0.f,1.f):0.f;
        if(!ShotQueued)Athletes[11]->Trigger=FMath::Max(RunUpLoad,PullLoad);
        Athletes[11]->FootworkIntent=Intent.Footwork;Athletes[11]->StrideIntent=Intent.Stride;Athletes[11]->Defending=Intent.Defend;
        if(Phase==EC26Phase::Delivery&&ShotQueued)
            Athletes[11]->ActionTime=C26Field::BatContactPoseTime-FMath::Max(0.f,TimingCountdown())-Dt;
        const bool ReleasedThisFrame=PhaseBeforeUpdate==EC26Phase::RunUp&&Phase==EC26Phase::Delivery;
        const bool ContactThisFrame=PhaseBeforeUpdate==EC26Phase::Delivery&&Phase==EC26Phase::InPlay;
        if((Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay)&&Athletes[0]->Action==EC26Action::Bowling&&Athletes[0]->ActionTime<1.25f&&!ReleasedThisFrame)
            Athletes[0]->AddActorWorldOffset(FVector(0,330.f*FMath::Clamp((1.25f-Athletes[0]->ActionTime)/.63f,0.f,1.f)*Dt,0));
        if(Athletes.IsValidIndex(11)&&Athletes.IsValidIndex(0))
        {
            const FVector Focus=Simulation.Ball.Active?Simulation.Ball.Position:Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery?Athletes[0]->HandPosition():Athletes[0]->GetActorLocation();
            Athletes[11]->LookAt=Focus;
            Athletes[0]->LookAt=Athletes[11]->GetActorLocation();
            for(int I=1;I<11;++I)Athletes[I]->LookAt=Simulation.Ball.Active?Simulation.Ball.Position:Athletes[11]->GetActorLocation();
        }
        // Presentation budget. The director's own position is the view the frame is composed
        // from, so athletes are graded against it rather than against the pitch: a fielder who
        // runs into a replay close-up is promoted for those frames instead of staying cheap.
        UpdateFieldPresence(Dt);
        const FVector ViewPoint=Director->GetActorLocation();
        for(AC26Athlete* Athlete:Athletes)Athlete->UpdateDetail(ViewPoint);
        // Release/contact already evaluated their exact event poses. Advancing them again here
        // detaches the visible hand from the ball and skips the actual bat-impact frame.
        for(int I=0;I<Athletes.Num();++I)
        {
            const bool EventPose=(I==0&&ReleasedThisFrame)||(I==11&&ContactThisFrame)||(I==ActiveFielder&&(ThrowClock>=0||CatchClock>=0))||(I==1&&KeeperTakeClock>=0);
            Athletes[I]->Animate(EventPose?0.f:Dt);
        }
        if(ThrowClock>=.53f&&!ThrowReleased)Simulation.Ball.Position=Athletes[ActiveFielder]->HandPosition();
        else if(ThrowClock>=.20f&&!ThrowReleased)Simulation.Ball.Position=Athletes[ActiveFielder]->ReceivingPosition();
        if(CatchClock>=0)Simulation.Ball.Position=Athletes[ActiveFielder]->ReceivingPosition();
        if(KeeperTakeClock>=0)Simulation.Ball.Position=Athletes[1]->ReceivingPosition();
        if(Phase==EC26Phase::RunUp||Phase==EC26Phase::Ready)Simulation.Ball.Position=Athletes[0]->HandPosition();
        if(Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay||Phase==EC26Phase::RunUp)Director->Record(Dt,Simulation.Ball.Position,Athletes);
    }
    if(StumpClock>=0&&Phase!=EC26Phase::Replay)
    {
        StumpClock=FMath::Min(1.f,StumpClock+Dt);const int E=BrokenWicketY>0?5:0;
        for(int B=0;B<2;++B)
        {
            const float T=StumpClock,Side=B==0?-1.f:1.f;
            const float Z=FMath::Max(2.f,72.f+165.f*T-.5f*Tuning.Gravity*T*T);
            Stumps[E+3+B]->SetWorldLocation(FVector(Side*(5.f+60.f*T),BrokenWicketY+145.f*T,Z));
            Stumps[E+3+B]->SetWorldRotation(FRotator(T*410.f,Side*T*320.f,90.f+T*560.f));
        }
    }
    if(Phase==EC26Phase::Reaction&&PhaseTime<.8f)Director->Record(Dt,Simulation.Ball.Position,Athletes);
    UpdateBallVisual();
    if(bDebugTrace&&(Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay))
    {
        if(!DebugPrevBall.IsZero())DrawDebugLine(GetWorld(),DebugPrevBall,Simulation.Ball.Position,FColor::White,false,8.f,0,.6f);
        DebugPrevBall=Simulation.Ball.Position;
    }
    else DebugPrevBall=FVector::ZeroVector;
    Director->Direct(Phase,PhaseTime,PlayerBatting(),Simulation.Ball.Position,Simulation.Ball.Velocity,Intent.Loft,Dt);Venue->UpdateAtmosphere(Clock);
    if(Effects)
    {
        // Billboards face whatever the director just cut to, including during a replay.
        const FRotationMatrix Lens(Director->GetActorRotation());
        Effects->Advance(Dt,Lens.GetScaledAxis(EAxis::Y),Lens.GetScaledAxis(EAxis::Z),Lens.GetScaledAxis(EAxis::X));
    }
    if(Smoke){SmokeWatchdog+=Dt;if(SmokeWatchdog>240){UE_LOG(LogC26,Error,TEXT("C26_SMOKE_TIMEOUT phase=%d delivery=%u"),int(Phase),DeliveryId);FPlatformMisc::RequestExit(false);Smoke=false;}}
#if !UE_BUILD_SHIPPING
    if(GoldenGate)UpdateGoldenGate(Dt);
    if(BatLab)UpdateBatLab(Dt);
    if(BowlLab)UpdateBowlLab(Dt);
#endif
}
void AC26MatchGameMode::UIAction(FName Action)
{
    if((Action==TEXT("loft")||Action==TEXT("defend"))&&(ShotQueued||Paused||SettingsOpen||ControlsOpen||!PlayerBatting()||(Phase!=EC26Phase::Ready&&Phase!=EC26Phase::RunUp&&Phase!=EC26Phase::Delivery)))return;
    LastAction=Action;LastActionAt=Clock;
    Audio->Cue(TEXT("ui_button_click"),.25f);
    if(Action==TEXT("play")||Action==TEXT("again"))StartMatch();
    else if(Action==TEXT("menu"))Menu();
    else if(Action==TEXT("team"))PlayerTeam=1-PlayerTeam;
    else if(Action==TEXT("pick0"))PlayerTeam=0;
    else if(Action==TEXT("pick1"))PlayerTeam=1;
    else if(Action==TEXT("batfirst")){PlayerBatsFirst=true;UseToss=false;if(Phase==EC26Phase::Menu&&MenuScreen==4&&TossStage==2&&TossPlayerWon)TossPlayerChoseBat=true;}
    else if(Action==TEXT("bowlfirst")){PlayerBatsFirst=false;UseToss=false;if(Phase==EC26Phase::Menu&&MenuScreen==4&&TossStage==2&&TossPlayerWon)TossPlayerChoseBat=false;}
    else if(Action==TEXT("toss"))UseToss=!UseToss;
    // ---- front-end navigation ----
    else if(Action==TEXT("nav_home"))SetScreen(0);
    else if(Action==TEXT("nav_play"))SetScreen(1);
    else if(Action==TEXT("nav_teams"))SetScreen(2);
    else if(Action==TEXT("nav_matchup"))SetScreen(3);
    else if(Action==TEXT("nav_toss")){TossStage=0;SetScreen(4);}
    else if(Action==TEXT("nav_myteam"))SetScreen(5);
    else if(Action==TEXT("nav_career"))SetScreen(6);
    else if(Action==TEXT("nav_tour"))SetScreen(7);
    else if(Action==TEXT("nav_online"))SetScreen(8);
    else if(Action==TEXT("nav_train"))SetScreen(9);
    else if(Action==TEXT("nav_world"))SetScreen(10);
    else if(Action==TEXT("nav_settings"))SetScreen(11);
    else if(Action==TEXT("nav_help"))SetScreen(12);
    else if(Action==TEXT("nav_store"))SetScreen(13);
    else if(Action==TEXT("back"))GoBack();
    else if(Action==TEXT("quickplay")){SetScreen(3);}
    else if(Action==TEXT("mode_super"))SetScreen(2);
    else if(Action==TEXT("mode_soon")){Toast(TEXT("COMING SOON  /  THIS MODE IS IN DEVELOPMENT"));}
    // ---- hub surfaces that present real content but have no systems behind them
    // yet. They answer instead of swallowing the press, so nothing in the front
    // end reads as a dead button.
    else if(Action==TEXT("tossheads")||Action==TEXT("tosstails"))
    {
        if(TossStage==0){TossStage=1;TossClock=0;TossPlayerWon=AI.Random.FRand()>.5f;TossAIChoiceBat=AI.Random.FRand()>.5f;
            Toast(Action==TEXT("tossheads")?TEXT("HEADS CALLED"):TEXT("TAILS CALLED"));}
    }
    else if(Action.ToString().StartsWith(TEXT("squad")))Toast(TEXT("PLAYER UPGRADES ARRIVE WITH CAREER MODE"));
    else if(Action==TEXT("career_club"))Toast(TEXT("CLUB CAREER  /  SEASON 1 OPENS IN THE NEXT UPDATE"));
    else if(Action==TEXT("career_intl"))Toast(TEXT("LOCKED  /  REACH CLUB TIER 3 TO UNLOCK"));
    else if(Action==TEXT("career_achv"))Toast(TEXT("ACHIEVEMENTS  /  14 OF 60 COMPLETE"));
    else if(Action.ToString().StartsWith(TEXT("lb_")))Toast(TEXT("RANKING FILTERS GO LIVE WITH ONLINE PLAY"));
    else if(Action==TEXT("mp_ranked"))Toast(TEXT("RANKED SEASONS  /  MATCHMAKING NOT YET LIVE"));
    else if(Action==TEXT("mp_friends"))Toast(TEXT("PRIVATE LOBBIES  /  MATCHMAKING NOT YET LIVE"));
    else if(Action.ToString().StartsWith(TEXT("store_")))Toast(TEXT("STORE PURCHASES ARE DISABLED IN THIS BUILD"));
    else if(Action==TEXT("matchup_go")){TossStage=0;SetScreen(4);}
    else if(Action==TEXT("tossflip")){if(TossStage==0){TossStage=1;TossClock=0;TossPlayerWon=AI.Random.FRand()>.5f;TossAIChoiceBat=AI.Random.FRand()>.5f;}}
    else if(Action==TEXT("tossquick")){PlayerBatsFirst=true;UseToss=false;TossResolved=false;StartMatch();}
    else if(Action==TEXT("tosscontinue"))
    {
        if(TossStage==2)
        {
            PlayerBatsFirst=TossPlayerWon?TossPlayerChoseBat:!TossAIChoiceBat;
            UseToss=false;TossResolved=true;
            const int BatFirst=TossPlayerWon?(TossPlayerChoseBat?PlayerTeam:1-PlayerTeam):(TossAIChoiceBat?1-PlayerTeam:PlayerTeam);
            ResolvedTossText=TeamShort(TossPlayerWon?PlayerTeam:1-PlayerTeam)+TEXT(" WIN THE TOSS  /  ")+TeamShort(BatFirst)+TEXT(" BAT FIRST");
            StartMatch();
        }
    }
    else if(Action==TEXT("confirm_restart"))PendingConfirm=TEXT("restart");
    else if(Action==TEXT("confirm_exit"))PendingConfirm=TEXT("exit");
    else if(Action==TEXT("yes"))
    {
        if(PendingConfirm==TEXT("restart")){PendingConfirm=NAME_None;StartMatch();}
        else if(PendingConfirm==TEXT("exit")){PendingConfirm=NAME_None;Menu();}
    }
    else if(Action==TEXT("no"))PendingConfirm=NAME_None;
    else if(Action==TEXT("stab0"))SettingsTab=0;
    else if(Action==TEXT("stab1"))SettingsTab=1;
    else if(Action==TEXT("stab2"))SettingsTab=2;
    else if(Action==TEXT("stab3"))SettingsTab=3;
    else if(Action==TEXT("stab4"))SettingsTab=4;
    else if(Action==TEXT("stab5"))SettingsTab=5;
    else if(Action==TEXT("settings"))SettingsOpen=!SettingsOpen;
    else if(Action==TEXT("help"))ControlsOpen=!ControlsOpen;
    else if(Action==TEXT("close")){SettingsOpen=false;ControlsOpen=false;Paused=false;Preferences->Save();}
    else if(Action==TEXT("difficulty")){Preferences->Difficulty=(Preferences->Difficulty+1)%3;Preferences->Save();}
    else if(Action==TEXT("quality")){Preferences->Quality=(Preferences->Quality+1)%4;Preferences->Apply();Venue->SetQuality(Preferences->Quality);Preferences->Save();}
    else if(Action==TEXT("sound")){Preferences->SoundVolume=Preferences->SoundVolume>.1f?0:.75f;Audio->SyncVolumesFromSettings();Audio->SetTension(.1f,Audio->Master);Preferences->Save();}
    else if(Action==TEXT("master")){Preferences->SoundVolume=Preferences->SoundVolume>.1f?0:.75f;Audio->SyncVolumesFromSettings();Preferences->Save();}
    else if(Action==TEXT("commentary")){Preferences->CommentaryVolume=Preferences->CommentaryVolume>.1f?0:.95f;Audio->SyncVolumesFromSettings();Preferences->Save();}
    else if(Action==TEXT("crowd")){Preferences->CrowdVolume=Preferences->CrowdVolume>.1f?0:.85f;Audio->SyncVolumesFromSettings();Preferences->Save();}
    else if(Action==TEXT("sfx")){const bool bOff=Preferences->SFXVolume>.1f;Preferences->SFXVolume=bOff?0:.9f;Preferences->UIVol=bOff?0:.8f;Audio->SyncVolumesFromSettings();Preferences->Save();}
    else if(Action==TEXT("vibration")){Preferences->Vibration=!Preferences->Vibration;Preferences->Save();}
    else if(Action==TEXT("subtitles")){Preferences->Subtitles=!Preferences->Subtitles;Preferences->Save();}
    else if(Action==TEXT("reducedmotion")){Preferences->ReducedMotion=!Preferences->ReducedMotion;Preferences->Save();}
    else if(Action==TEXT("hints")){Preferences->Hints=!Preferences->Hints;Preferences->Save();}
    else if(Action==TEXT("sensitivity")){Preferences->Sensitivity=Preferences->Sensitivity>=1.49f?.7f:Preferences->Sensitivity+.4f;Preferences->Save();}
    else if(Action==TEXT("controls_scheme")){Preferences->ControlScheme=(Preferences->ControlScheme+1)%2;Preferences->Save();Toast(Preferences->ControlScheme==0?TEXT("GESTURE PRO CONTROLS"):TEXT("CLASSIC CONTROLS"));}
    else if(Action==TEXT("lefthand")){Preferences->LeftHandedUI=!Preferences->LeftHandedUI;Preferences->Save();}
    else if(Action==TEXT("pause"))Paused=!Paused;
    else if(Action==TEXT("skip"))Skip();
    else if(Action==TEXT("ready"))StartDelivery();
    else if(Action==TEXT("release"))BowlRelease();
    else if(Action==TEXT("loft")){Intent.Loft=!Intent.Loft;Intent.Defend=false;}
    else if(Action==TEXT("defend")){Intent.Defend=!Intent.Defend;Intent.Loft=false;}
    else if(Action==TEXT("run"))Run();
    else if(Action==TEXT("cancel"))CancelRun();
    else if(Action==TEXT("delivery_next"))CycleDelivery(1);
    else if(Action==TEXT("delivery_prev"))CycleDelivery(-1);
    else if(Action==TEXT("around")){if(Phase==EC26Phase::Ready&&!PlayerBatting()){BowlingPlan.bAroundWicket=!BowlingPlan.bAroundWicket;TrajectoryPreviewHash=0;Toast(BowlingPlan.bAroundWicket?TEXT("AROUND THE WICKET"):TEXT("OVER THE WICKET"));}}
    else if(Action.ToString().StartsWith(TEXT("p_")))ApplyBowlingPreset(Action);
}
void AC26MatchGameMode::DebugOutcome(FString Type)
{
#if !UE_BUILD_SHIPPING
    // Commentary test harness: "c six", "c wicket", "c four", "c dot",
    // "c finalball", "c pressure", "c win", "c dump". Audible without
    // playing a full over; exercises the real queue/priority path.
    if(Type.StartsWith(TEXT("c ")))
    {
        const FString Cat=Type.RightChop(2);
        if(Cat==TEXT("dump")){Audio->DumpState();return;}
        if(Cat==TEXT("routine_dot")||Cat==TEXT("dot")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::RoutineDot);return;}
        if(Cat==TEXT("four")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::NormalFour);return;}
        if(Cat==TEXT("six")||Cat==TEXT("normal_six")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::NormalSix);return;}
        if(Cat==TEXT("pressure_six")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::HighPressureSix);return;}
        if(Cat==TEXT("win_six")||Cat==TEXT("winning_six")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::MatchWinningSix);return;}
        if(Cat==TEXT("wicket")||Cat==TEXT("bowled")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::WicketBowled);return;}
        if(Cat==TEXT("caught")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::WicketCaught);return;}
        if(Cat==TEXT("final_ball")||Cat==TEXT("finalball")){if(CommentaryDirector)CommentaryDirector->TestScenario(ECommentaryScenario::FinalBallTense);return;}
        Audio->TestCommentary(FName(*Cat));
        return;
    }
    if(Phase==EC26Phase::Menu)StartMatch();
    if(Phase==EC26Phase::Intro)PrepareDelivery();
    if(Type==TEXT("controls")){bDebugControls=!bDebugControls;UE_LOG(LogC26,Display,TEXT("C26_CONTROLS_DEBUG %d"),bDebugControls?1:0);return;}
    if(Phase!=EC26Phase::Ready)return;
    if(Type==TEXT("six")){Pending.Rope=C26::Boundary::Six;Pending.BatRuns=6;}
    else if(Type==TEXT("four")){Pending.Rope=C26::Boundary::Four;Pending.BatRuns=4;}
    else if(Type==TEXT("wicket"))Pending.Wicket=C26::Dismissal::Bowled;
    else if(Type==TEXT("wide"))Pending.WideRuns=1;
    else if(Type==TEXT("noball"))Pending.NoBall=true;
    ResettingMatch=true;Resolve();ResettingMatch=false;
#endif
}

// ============================================================================
// GesturePro controls — WCC3-style pull-and-release batting & bowling.
// The gesture drives INTENT (aim, power, timing, footwork hint, pace plan).
// FC26Simulation + C26::Match remain the sole authority on outcomes.
// ============================================================================
// ---------------------------------------------------------------------------
// BATTING GESTURE STATE MACHINE
//
// One pointer-down/up cycle produces exactly one batting attempt:
//
//   Idle -> ReadyForShot (run-up starts)
//        -> Pulling      (pointer down in the zone; inside the dead zone)
//        -> Armed        (finger outside the dead zone; direction + power live)
//        -> Released     (pointer up: THIS is when the shot is committed)
//        -> ShotCommitted -> Contact -> FollowThrough -> Recovery -> Idle
//
// Nothing is committed on press or on crossing a threshold. The pull only
// carries INTENT (direction, aggression, footwork hint); FC26Simulation stays
// the sole authority on what the bat actually does to the ball.
// ---------------------------------------------------------------------------
bool AC26MatchGameMode::IsBattingInputLive() const
{
    if (Paused || SettingsOpen || ControlsOpen || AutoPlay) return false;
    if (!PlayerBatting() || ShotQueued) return false;
    if (Rules.Now().Closed) return false;
    // Live for the WHOLE run-up as well as the flight: the player is meant to be
    // able to start shaping the shot while the bowler is still running in.
    return Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery;
}

bool AC26MatchGameMode::IsInBattingGestureZone(FVector2D DesignPos) const
{
    return DesignPos.X >= GestureTuning.GestureZoneMin.X && DesignPos.X <= GestureTuning.GestureZoneMax.X
        && DesignPos.Y >= GestureTuning.GestureZoneMin.Y && DesignPos.Y <= GestureTuning.GestureZoneMax.Y;
}

bool AC26MatchGameMode::BeginBattingGesture(int32 PointerId, FVector2D DesignPos)
{
    if (!IsBattingInputLive()) return false;
    // Multi-touch protection: the first finger down owns the gesture until it lifts.
    if (bBattingGestureActive) return false;
    if (!IsInBattingGestureZone(DesignPos)) return false;

    GesturePointerId = PointerId;
    GestureDeliveryId = DeliveryId;
    bBattingGestureActive = true;
    bGestureArmed = false;
    GestureCancelReason.Reset();
    // Floating origin: the control appears exactly where the thumb landed.
    BattingGestureStart = BattingGestureCurrent = DesignPos;
    BattingPullRaw = FVector2D::ZeroVector;
    BattingPullRawMagnitude = 0.f;
    BattingPullFrac = 0.f;
    BattingAggression = 0.f;
    BattingGestureAngle = 0.f;
    BattingPullScreenAngle = 0.f;
    BattingGesturePower = GestureTuning.MinPower;
    BattingSuitability = 1.f;
    BattingGestureHoldTime = 0.f;
    GestureStartRealTime = FPlatformTime::Seconds();
    BattingState = EC26BattingState::Pulling;
    EvaluateBattingGesture();
    return true;
}

void AC26MatchGameMode::UpdateBattingGesture(int32 PointerId, FVector2D DesignPos)
{
    // Only the finger that started the gesture may move it.
    if (!bBattingGestureActive || PointerId != GesturePointerId) return;
    if (GestureDeliveryId != DeliveryId) { CancelBattingGesture(TEXT("stale delivery")); return; }
    BattingGestureCurrent = DesignPos;
    EvaluateBattingGesture();
}

void AC26MatchGameMode::EvaluateBattingGesture()
{
    // Raw finger travel, in design units, so behaviour is identical at any resolution.
    BattingPullRaw = C26Controls::PullVector(BattingGestureStart, BattingGestureCurrent);
    BattingPullRawMagnitude = BattingPullRaw.Size();

    const float Sens = FMath::Max(0.05f, GestureTuning.PullSensitivity);
    BattingPullFrac = C26Controls::PullMagnitude01(BattingPullRaw,
        GestureTuning.GestureDeadZone, GestureTuning.GestureMaxRadius, Sens);

    // Aggression curve: CurveFloat when authored, otherwise the built-in bands.
    BattingAggression = GestureTuning.MagnitudeCurve
        ? FMath::Clamp(GestureTuning.MagnitudeCurve->GetFloatValue(BattingPullFrac), 0.f, 1.f)
        : C26Controls::DefaultMagnitudeCurve(BattingPullFrac);

    const bool bInsideDeadZone = BattingPullRawMagnitude * Sens <= GestureTuning.GestureDeadZone;
    bGestureArmed = !bInsideDeadZone;
    if (bBattingGestureActive)
        BattingState = bGestureArmed ? EC26BattingState::Armed : EC26BattingState::Pulling;

    if (bInsideDeadZone)
    {
        // Neutral preparation state: no direction claimed, no power promised.
        BattingPullScreenAngle = 0.f;
        BattingGestureAngle = 0.f;
        BattingGesturePower = GestureTuning.MinPower;
    }
    else
    {
        BattingPullScreenAngle = FMath::RadiansToDegrees(FMath::Atan2(BattingPullRaw.X, -BattingPullRaw.Y));
        // Batter-relative: + off side, - leg side. Mirrored for a left-hander so
        // "drag toward cover" always means the batter's own cover, never the screen's.
        BattingGestureAngle = FMath::Clamp(
            C26Controls::AimAngleFromPull(BattingPullRaw, GestureTuning.GestureDeadZone,
                GestureTuning.DirectionSensitivity, bLeftHandedBatter),
            -GestureTuning.MaxAimAngle, GestureTuning.MaxAimAngle);
        BattingGesturePower = C26Controls::PowerFromAggression(BattingAggression,
            GestureTuning.MinPower, GestureTuning.MaxPower);
    }

    // Preview the delivery the gesture will actually meet. Before release the plan
    // is the best truth available; after release the measured trajectory is.
    const FC26DeliveryPlan& Plan = (Phase == EC26Phase::Delivery) ? Bowling : LockedBowling;
    const bool bDefendGuess = Intent.Defend || BattingPullRawMagnitude <= GestureTuning.DefendPullMax;
    BattingSuitability = FMath::Clamp(
        C26Controls::ShotSuitability(BattingGestureAngle, Plan.Length, Plan.Line, Intent.Loft, bDefendGuess)
        - (bDefendGuess ? 0.f : C26Controls::ControlPenaltyFromPower(BattingGesturePower)), 0.15f, 1.f);
    BattingStrideAuto = C26Controls::IdealStrideForLength(Plan.Length);

    // Contact geometry for the shot-family preview: the real predicted contact
    // point once the ball is live, otherwise a length-derived estimate.
    float PreviewHeight = Plan.Length > 710.f ? 18.f : Plan.Length < 180.f ? 132.f : 74.f;
    float PreviewOffset = Plan.Line;
    if (Phase == EC26Phase::Delivery && Simulation.ContactTime > 0.f)
    {
        PreviewHeight = Simulation.ContactPosition.Z;
        PreviewOffset = Simulation.ContactPosition.X - FMath::Clamp(Footwork, -1.f, 1.f) * 42.f;
    }
    const float PreviewStride = bFootworkManual
        ? FMath::Clamp((Intent.Stride + BattingStrideAuto) * 0.5f, -1.f, 1.f) : BattingStrideAuto;
    BattingShotCandidate = C26Controls::ShotFamily(BattingGestureAngle, Plan.Length, PreviewHeight,
        PreviewStride, PreviewOffset, Intent.Loft && !bDefendGuess, bDefendGuess);

    // The pull loads the backlift. It does NOT swing: only a release can do that.
    if (Athletes.IsValidIndex(11))
    {
        Athletes[11]->FootworkIntent = Footwork;
        Athletes[11]->StrideIntent = PreviewStride;
    }
}

void AC26MatchGameMode::CancelBattingGesture(const TCHAR* Why)
{
    if (!bBattingGestureActive) return;
    bBattingGestureActive = false;
    bGestureArmed = false;
    GesturePointerId = -1;
    GestureCancelReason = Why ? FString(Why) : TEXT("cancelled");
    BattingPullFrac = 0.f;
    BattingAggression = 0.f;
    BattingPullRaw = FVector2D::ZeroVector;
    BattingPullRawMagnitude = 0.f;
    BattingShotCandidate.Reset();
    // Never leave the machine parked in Pulling/Armed: a lost touch must return
    // the batter to a state that can accept the next gesture.
    if (BattingState == EC26BattingState::Pulling || BattingState == EC26BattingState::Armed)
        BattingState = IsBattingInputLive() ? EC26BattingState::ReadyForShot : EC26BattingState::Idle;
    UE_LOG(LogC26, Verbose, TEXT("C26_GESTURE_CANCEL %s delivery=%u"), Why ? Why : TEXT("?"), DeliveryId);
}

float AC26MatchGameMode::SecondsToIdealRelease() const
{
    // Ideal release is one reaction-window (0.10s) before the ball reaches the bat,
    // matching the error definition FC26Simulation::Hit is given.
    if (Phase == EC26Phase::Delivery && Simulation.ContactTime > 0.f)
        return (Simulation.ContactTime - 0.10f) - Simulation.Ball.Age;
    if (Phase == EC26Phase::RunUp)
    {
        // Ball does not exist yet: run-up remaining + the locked plan's projected flight.
        const float ToRelease = FMath::Max(0.f, C26Field::RunUpDuration - PhaseTime);
        return ToRelease + (BouncePrediction.ProjectedContactTime - 0.10f);
    }
    return 0.f;
}

void AC26MatchGameMode::ReleaseBattingGesture(int32 PointerId, FVector2D DesignPos)
{
    if (!bBattingGestureActive || PointerId != GesturePointerId) return;

    // Freeze the gesture on the LAST finger position: direction is locked here and
    // nowhere earlier, which is why the player can keep changing their mind.
    BattingGestureCurrent = DesignPos;
    EvaluateBattingGesture();
    bBattingGestureActive = false;
    GesturePointerId = -1;
    GestureReleaseRealTime = FPlatformTime::Seconds();
    BattingState = EC26BattingState::Released;

    if (GestureDeliveryId != DeliveryId) { GestureCancelReason = TEXT("stale delivery"); BattingState = EC26BattingState::Idle; return; }
    if (ShotQueued) { GestureCancelReason = TEXT("shot already queued"); BattingState = EC26BattingState::ShotCommitted; return; }
    if (!PlayerBatting() || AutoPlay) { GestureCancelReason = TEXT("not batting"); BattingState = EC26BattingState::Idle; return; }
    if (Phase != EC26Phase::Delivery && Phase != EC26Phase::RunUp)
    { GestureCancelReason = TEXT("delivery over"); BattingState = EC26BattingState::Idle; return; }

    // ---- final gesture snapshot -------------------------------------------
    const float FinalMagnitude = BattingPullFrac;
    const float FinalAggression = BattingAggression;
    const float FinalAngle = BattingGestureAngle;
    const float RawDist = BattingPullRawMagnitude;

    FC26ShotIntent ShotCmd = Intent;
    const bool bBelowCommit = FinalMagnitude < GestureTuning.MinimumCommitMagnitude
        || RawDist <= GestureTuning.DefendPullMax;
    if (bBelowCommit)
    {
        // A short, controlled pull is a defensive stroke, never a wild swing.
        ShotCmd.Defend = true;
        ShotCmd.Loft = false;
        ShotCmd.Power = GestureTuning.MinPower;
        ShotCmd.Angle = FMath::Clamp(FinalAngle, -45.f, 45.f);
    }
    else
    {
        // The pull vector means exactly two things: direction and aggression.
        // It deliberately does NOT also mean loft - dragging up the screen is how
        // the player aims STRAIGHT, so overloading it would fight the direction
        // mapping. Loft stays on its own LOFT / GROUND / DEFEND control.
        ShotCmd.Angle = FinalAngle;
        ShotCmd.Power = C26Controls::PowerFromAggression(FinalAggression, GestureTuning.MinPower, GestureTuning.MaxPower);
    }
    // Footwork is derived from length; the manual stick only nudges it.
    ShotCmd.Stride = bFootworkManual
        ? FMath::Clamp((Intent.Stride + BattingStrideAuto) * 0.5f, -1.f, 1.f) : BattingStrideAuto;

    // ---- release timing: ONE delta feeds both the meter and the simulation --
    const float DeltaSeconds = -SecondsToIdealRelease();
    BattingReleaseDeltaMs = DeltaSeconds * 1000.f;
    PendingShotError = DeltaSeconds;
    bPendingShotFromGesture = true;

    const int Diff = Preferences ? Preferences->Difficulty : 1;
    const float Scale = C26Controls::TimingWindowScale(Diff);
    BattingReleaseTiming = C26Controls::ReleaseTimingFromDelta(BattingReleaseDeltaMs,
        GestureTuning.PerfectWindowMs * Scale, GestureTuning.GoodWindowMs * Scale,
        GestureTuning.VeryEarlyLateMs * Scale, Tuning.ContactWindow * 1000.f * Scale);
    BattingTimingLabel = C26Controls::TimingLabelFromDelta(BattingReleaseDeltaMs,
        GestureTuning.PerfectWindowMs * Scale, GestureTuning.GoodWindowMs * Scale,
        Tuning.ContactWindow * 1000.f * Scale, BattingTimingQuality);

    // Aggression costs control, but it must NEVER bend the timing number: the
    // meter and the bat read the same delta. The risk is applied by the
    // simulation as reduced suitability (see FC26Simulation::Hit).

    BattingFeedbackUntil = Clock + 1.35f;
    BattingCommitAge = 0.f;
    ++GestureCommitCount;
    Haptic(0.08f + FinalAggression * 0.16f);

    UE_LOG(LogC26, Log, TEXT("C26_SHOT_COMMIT delivery=%u aim=%+.1f mag=%.2f agg=%.2f delta=%+.0fms timing=%s shot=%s"),
        DeliveryId, FinalAngle, FinalMagnitude, FinalAggression, BattingReleaseDeltaMs,
        C26Controls::ReleaseTimingName(BattingReleaseTiming), *BattingShotCandidate);

    Shot(ShotCmd); // authoritative commit
    BattingState = EC26BattingState::ShotCommitted;
    // The swing itself starts here, on release, and never before.
    if (Athletes.IsValidIndex(11)) Athletes[11]->Trigger = 1.f;
}

// ---------------------------------------------------------------------------
// BOWLING: the player authors a delivery, then executes it.
//
//   SELECT TYPE -> DRAG TARGET -> SET MOVEMENT (direction + amount)
//   -> SET PACE -> START RUN-UP -> RELEASE ON THE BAR
//
// Every one of those is a real input into C26Delivery::Compose, which is the
// single place a delivery is built. The preview calls the same function with a
// clean release, so what the player is shown before the run-up is the ball they
// get when they execute it well - and only their execution changes it.
// ---------------------------------------------------------------------------
bool AC26MatchGameMode::BatterIsLeftHanded() const
{
    // The batter's hand, not the UI preference: swing direction is defined
    // relative to whoever is on strike.
    if (PlayerBatting()) return Preferences && Preferences->LeftHandedBatter;
    return (Rules.Now().Striker % 2) == 1;   // deterministic AI striker handedness
}

void AC26MatchGameMode::RefreshBowlerProfile()
{
    // Two named bowlers, one per side, with genuinely different lockers.
    const bool Meteors = BattingTeam() != 0;
    FC26BowlerProfile P;
    if (Meteors)
    {
        // N. ARCHER - right-arm fast, swings it, good cutters.
        P.Kind = EC26BowlerKind::Fast; P.bLeftArm = false;
        P.MinSpeedKph = 128.f; P.MaxSpeedKph = 148.f;
        P.Accuracy = 0.74f; P.SwingSkill = 0.80f; P.ReverseSkill = 0.55f;
        P.CutterSkill = 0.66f; P.SlowerSkill = 0.60f; P.SpinSkill = 0.05f; P.Control = 0.75f;
    }
    else
    {
        // V. SEN - left-arm fast-medium, less raw pace, better slower ball.
        P.Kind = EC26BowlerKind::FastMedium; P.bLeftArm = true;
        P.MinSpeedKph = 122.f; P.MaxSpeedKph = 141.f;
        P.Accuracy = 0.79f; P.SwingSkill = 0.72f; P.ReverseSkill = 0.42f;
        P.CutterSkill = 0.74f; P.SlowerSkill = 0.78f; P.SpinSkill = 0.05f; P.Control = 0.80f;
    }
    // Stamina drains across the over and takes accuracy and pace with it.
    P.Stamina = FMath::Clamp(1.f - Rules.Now().LegalBalls * 0.045f, 0.62f, 1.f);
    P.Accuracy *= FMath::Lerp(0.82f, 1.f, P.Stamina);
    P.Control *= FMath::Lerp(0.85f, 1.f, P.Stamina);
    P.MaxSpeedKph -= (1.f - P.Stamina) * 9.f;
    BowlerProfile = P;

    C26Delivery::Library(BowlerProfile, DeliveryLibrary);
    // Reverse swing is conditional: strip it out until the ball is old enough.
    if (!C26Delivery::ReverseAvailable(BowlerProfile, BowlingTuning, Rules.Now().LegalBalls))
    {
        DeliveryLibrary.Remove(EC26Delivery::ReverseOut);
        DeliveryLibrary.Remove(EC26Delivery::ReverseIn);
    }
    if (DeliveryLibrary.Num() && !DeliveryLibrary.Contains(BowlingPlan.Type))
        BowlingPlan.Type = DeliveryLibrary[0];
    // A named delivery owns its direction; keep the dial honest about that.
    if (!C26Delivery::DirectionIsFree(BowlingPlan.Type))
        BowlingPlan.MovementDirection = C26Delivery::NaturalDirection(BowlingPlan.Type);
}

void AC26MatchGameMode::CycleDelivery(int Direction)
{
    if (Phase != EC26Phase::Ready || PlayerBatting() || Paused || DeliveryLibrary.Num() == 0) return;
    int32 Index = DeliveryLibrary.IndexOfByKey(BowlingPlan.Type);
    if (Index == INDEX_NONE) Index = 0;
    Index = (Index + Direction + DeliveryLibrary.Num()) % DeliveryLibrary.Num();
    SelectDelivery(DeliveryLibrary[Index]);
}

void AC26MatchGameMode::SelectDelivery(EC26Delivery Type)
{
    if (Phase != EC26Phase::Ready || PlayerBatting() || Paused) return;
    if (!DeliveryLibrary.Contains(Type)) { Toast(TEXT("NOT IN THIS BOWLER'S LOCKER")); return; }
    BowlingPlan.Type = Type;
    if (!C26Delivery::DirectionIsFree(Type))
        BowlingPlan.MovementDirection = C26Delivery::NaturalDirection(Type);
    // Keep the requested pace inside the new delivery's own window.
    float Lo, Hi; C26Delivery::PaceWindow(Type, BowlerProfile, Lo, Hi);
    BowlingPlan.PaceNormalized = FMath::Clamp(BowlingPlan.PaceNormalized, 0.f, 1.f);
    BowlingState = EC26BowlingState::Planning;
    TrajectoryPreviewHash = 0;
    Audio->Cue(TEXT("ui_button_click"), 0.20f);
}

void AC26MatchGameMode::ApplyBowlingPreset(FName Preset)
{
    if (Phase != EC26Phase::Ready || PlayerBatting() || Paused) return;
    // Presets only WRITE into the plan; every field stays editable afterwards.
    const float Off = BatterIsLeftHanded() ? -1.f : 1.f;   // +X is a right-hander's off side
    if (Preset == TEXT("p_yorker"))
    { BowlingPlan.Type = EC26Delivery::Pace; BowlingPlan.TargetLength = 795.f; BowlingPlan.TargetLine = 4.f * Off; BowlingPlan.PaceNormalized = 0.86f; BowlingPlan.MovementMagnitude = 0.30f; }
    else if (Preset == TEXT("p_fourth"))
    { BowlingPlan.Type = EC26Delivery::Outswing; BowlingPlan.TargetLength = 470.f; BowlingPlan.TargetLine = 26.f * Off; BowlingPlan.PaceNormalized = 0.66f; BowlingPlan.MovementMagnitude = 0.72f; }
    else if (Preset == TEXT("p_bouncer"))
    { BowlingPlan.Type = EC26Delivery::Pace; BowlingPlan.TargetLength = 95.f; BowlingPlan.TargetLine = -6.f * Off; BowlingPlan.PaceNormalized = 0.95f; BowlingPlan.MovementMagnitude = 0.20f; }
    else if (Preset == TEXT("p_widey"))
    { BowlingPlan.Type = EC26Delivery::Pace; BowlingPlan.TargetLength = 800.f; BowlingPlan.TargetLine = 62.f * Off; BowlingPlan.PaceNormalized = 0.80f; BowlingPlan.MovementMagnitude = 0.25f; }
    else if (Preset == TEXT("p_slowcut"))
    { if (DeliveryLibrary.Contains(EC26Delivery::SlowerCutter)) BowlingPlan.Type = EC26Delivery::SlowerCutter;
      BowlingPlan.TargetLength = 560.f; BowlingPlan.TargetLine = 20.f * Off; BowlingPlan.PaceNormalized = 0.30f; BowlingPlan.MovementMagnitude = 0.80f; }
    else if (Preset == TEXT("p_inyork"))
    { if (DeliveryLibrary.Contains(EC26Delivery::Inswing)) BowlingPlan.Type = EC26Delivery::Inswing;
      BowlingPlan.TargetLength = 780.f; BowlingPlan.TargetLine = 10.f * Off; BowlingPlan.PaceNormalized = 0.88f; BowlingPlan.MovementMagnitude = 0.68f; }
    if (!C26Delivery::DirectionIsFree(BowlingPlan.Type))
        BowlingPlan.MovementDirection = C26Delivery::NaturalDirection(BowlingPlan.Type);
    BowlingIntendedPitch = FVector(BowlingPlan.TargetLine, BowlingPlan.TargetLength, 8.f);
    TrajectoryPreviewHash = 0;
    Audio->Cue(TEXT("ui_button_click"), 0.22f);
}

// ---- movement dial: direction is the angle, amount is the radius -----------
bool AC26MatchGameMode::BeginMovementDrag(int32 PointerId, FVector2D DesignPos)
{
    if (Phase != EC26Phase::Ready || PlayerBatting() || Paused || bMovementDragging) return false;
    MovementPointerId = PointerId;
    bMovementDragging = true;
    MovementDragStart = MovementDragCurrent = DesignPos;
    BowlingState = EC26BowlingState::MovementDrag;
    UpdateMovementDrag(PointerId, DesignPos);
    return true;
}

void AC26MatchGameMode::UpdateMovementDrag(int32 PointerId, FVector2D DesignPos)
{
    if (!bMovementDragging || PointerId != MovementPointerId) return;
    MovementDragCurrent = DesignPos;
    // The dial lives at a fixed centre; the finger's offset from it is the vector.
    const FVector2D Centre(1352.f, 640.f);
    const FVector2D V = DesignPos - Centre;
    const float R = FMath::Clamp(V.Size() / 78.f, 0.f, 1.f);
    BowlingPlan.MovementMagnitude = R;
    if (C26Delivery::DirectionIsFree(BowlingPlan.Type) && V.SizeSquared() > 36.f)
    {
        // Screen-right on the dial means "away from the batter", which is what
        // the arrow draws; the world sign is resolved at composition time.
        BowlingPlan.MovementDirection = FMath::Clamp(V.X / 60.f, -1.f, 1.f);
    }
    TrajectoryPreviewHash = 0;
}

void AC26MatchGameMode::EndMovementDrag(int32 PointerId)
{
    if (PointerId != MovementPointerId) return;
    bMovementDragging = false; MovementPointerId = -1;
    if (BowlingState == EC26BowlingState::MovementDrag) BowlingState = EC26BowlingState::Planning;
}

// ---- pace slider: continuous, inside this bowler's own range ---------------
bool AC26MatchGameMode::BeginPaceDrag(int32 PointerId, FVector2D DesignPos)
{
    if (Phase != EC26Phase::Ready || PlayerBatting() || Paused || PacePointerId >= 0) return false;
    PacePointerId = PointerId;
    BowlingState = EC26BowlingState::PaceDrag;
    UpdatePaceDrag(PointerId, DesignPos);
    return true;
}

void AC26MatchGameMode::UpdatePaceDrag(int32 PointerId, FVector2D DesignPos)
{
    if (PointerId != PacePointerId) return;
    // Slider track: design X 1184..1520 at Y 742.
    BowlingPlan.PaceNormalized = FMath::Clamp((DesignPos.X - 1184.f) / 336.f, 0.f, 1.f);
    TrajectoryPreviewHash = 0;
}

void AC26MatchGameMode::EndPaceDrag(int32 PointerId)
{
    if (PointerId != PacePointerId) return;
    PacePointerId = -1;
    if (BowlingState == EC26BowlingState::PaceDrag) BowlingState = EC26BowlingState::Planning;
}

void AC26MatchGameMode::CancelBowlingDrags()
{
    bMovementDragging = false;
    MovementPointerId = PacePointerId = TargetPointerId = -1;
    if (BowlingState == EC26BowlingState::MovementDrag || BowlingState == EC26BowlingState::PaceDrag
        || BowlingState == EC26BowlingState::TargetDrag)
        BowlingState = EC26BowlingState::Planning;
}

void AC26MatchGameMode::PaceRangeKph(float& OutMin, float& OutMax) const
{
    C26Delivery::PaceWindow(BowlingPlan.Type, BowlerProfile, OutMin, OutMax);
}

float AC26MatchGameMode::PlannedKph() const
{
    return C26Delivery::PlannedKph(BowlingPlan, BowlerProfile);
}

FC26DeliveryPlan AC26MatchGameMode::PreviewDelivery() const
{
    // Composed at a clean release with no scatter: this is the delivery the
    // player is promising to bowl, not a decorative curve.
    return C26Delivery::Compose(BowlingPlan, BowlerProfile, BowlingTuning,
        1.f, 0.f, false, BatterIsLeftHanded(), Preferences ? Preferences->Difficulty : 1, 0.5f);
}

void AC26MatchGameMode::BuildTrajectoryPreview()
{
    // DELIBERATELY SUPPRESSED — clean bowling view.
    //
    // The projected ball path is no longer built. AC26HUD only renders the spline
    // when this array holds more than one point, so leaving it empty removes the
    // whole projection — including the dashed continuation that used to run past
    // the bounce all the way to the bat, which was the worst of the clutter.
    //
    // The aiming aid the bowler actually needs is unaffected: the gold pitch
    // marker at the planned bounce point is drawn by AC26HUD::DrawBowlingTarget()
    // from BowlingPlan, not from this preview.
    //
    // To bring the path back, restore the release-and-sample loop here: release
    // PreviewDelivery() from the true hand origin (LastBowlingOrigin + the crease
    // offset, falling back to the end of the run-up before the first ball), then
    // step FC26Simulation::Predict() across ContactTime in 27 samples, recording
    // the first index where S.Bounced as TrajectoryPreviewBounce.
    TrajectoryPreview.Reset();
    TrajectoryPreviewBounce = -1;
}

FString AC26MatchGameMode::GetDeliveryName() const { return FString(C26Delivery::Name(BowlingPlan.Type)); }

FString AC26MatchGameMode::GetMovementText() const
{
    const float Dir = C26Delivery::DirectionIsFree(BowlingPlan.Type)
        ? BowlingPlan.MovementDirection : C26Delivery::NaturalDirection(BowlingPlan.Type);
    return FString(C26Delivery::DirectionName(BowlingPlan.Type, Dir));
}

FString AC26MatchGameMode::GetReleaseBandName() const
{
    return FString(C26Delivery::BandName(ReleaseBand));
}

float AC26MatchGameMode::CreaseOffsetCm() const
{
    if (!BowlingPlan.bAroundWicket) return 0.f;
    // The sign follows the bowling arm, so a right-armer's around-the-wicket
    // release sits on the opposite side of the stumps from his over-the-wicket
    // one and a left-armer mirrors it. The magnitude is data-driven.
    return (BowlerProfile.bLeftArm ? -1.f : 1.f) * BowlingTuning.AroundWicketOffsetCm;
}

FVector AC26MatchGameMode::ReleaseOriginCm() const
{
    return FVector(-20.f + CreaseOffsetCm(), -995.f,
        Athletes.IsValidIndex(0) ? Athletes[0]->HandPosition().Z : 196.f);
}
// ---------------------------------------------------------------------------
// PITCH MARKER LIFECYCLE
//
//   RUN-UP STARTS  -> visible immediately, showing the bowler's INTENDED pitch
//   BALL RELEASED  -> still visible; smoothly corrected toward the measured
//                     bounce point (never teleported)
//   BALL IN FLIGHT -> still visible
//   BALL BOUNCES   -> and only then, fades out over MarkerFadeDuration
//
// There is deliberately no appear delay and no "disappear before bounce" timer:
// the real bounce event is the only thing that ends it.
// ---------------------------------------------------------------------------
void AC26MatchGameMode::BeginBouncePreview()
{
    BouncePrediction = FC26BouncePrediction();

    // The locked plan's Line/Length IS the intended pitch point in world XY:
    // FC26Simulation::Release aims the ball to touch down exactly there.
    const FVector Intended(LockedBowling.Line, LockedBowling.Length, C26Field::SurfaceZ + 2.f);
    BouncePrediction.IntendedLocation = Intended;
    BouncePrediction.DisplayLocation = Intended;
    BouncePrediction.CorrectionStart = Intended;
    BouncePrediction.TrueLocation = Intended;
    BouncePrediction.bFromIntent = true;
    BouncePrediction.bBounced = false;
    BouncePrediction.FadeClock = 0.f;
    BouncePrediction.LengthCategory = C26Controls::ClassifyLength(LockedBowling.Length);
    BouncePrediction.LineCategory = C26Controls::ClassifyLine(LockedBowling.Line);

    const int Diff = Preferences ? Preferences->Difficulty : 1;
    BouncePrediction.UncertaintyRadius = FMath::Max(GestureTuning.MarkerUncertainty, C26Controls::MarkerUncertainty(Diff));

    // Project the flight of the locked plan so release timing exists during the
    // run-up, before any ball has been bowled.
    {
        FC26Simulation Scratch;
        Scratch.Tuning = Simulation.Tuning;
        const FVector Origin = Athletes.IsValidIndex(0)
            ? FVector(-20.f, -995.f, Athletes[0]->HandPosition().Z) : FVector(-20.f, -995.f, 196.f);
        Scratch.Release(LockedBowling, Origin);
        BouncePrediction.ProjectedContactTime = FMath::Max(0.15f, Scratch.ContactTime);
        FVector ScratchPos; float ScratchTime;
        if (Scratch.GetBouncePrediction(ScratchPos, ScratchTime))
            BouncePrediction.BounceTime = ScratchTime;
        else
            BouncePrediction.BounceTime = Scratch.ContactTime;
    }
    BouncePrediction.AppearTime = 0.f;                                   // visible from the first frame
    BouncePrediction.FadeStartTime = BouncePrediction.BounceTime;        // fade begins AT the bounce
    BouncePrediction.FadeEndTime = BouncePrediction.BounceTime + GestureTuning.MarkerFadeDuration;
    BouncePrediction.bVisible = PlayerBatting() && !AutoPlay;
    // Readable on the FIRST frame of the run-up, then eased to full over ~0.1s.
    // Starting at zero would cost a frame, which is exactly the lateness this
    // rebuild exists to remove.
    BouncePrediction.Alpha = BouncePrediction.bVisible
        ? FMath::Clamp(GestureTuning.MarkerOpacity, 0.f, 1.f) * 0.45f : 0.f;
}

void AC26MatchGameMode::CalculateBouncePrediction()
{
    // Called at ball release: the measured trajectory now exists. Keep the marker
    // exactly where the player has been reading it and correct toward the truth.
    FVector TruePos; float TrueTime;
    if (Simulation.GetBouncePrediction(TruePos, TrueTime))
    {
        BouncePrediction.TrueLocation = FVector(TruePos.X, TruePos.Y, C26Field::SurfaceZ + 2.f);
        BouncePrediction.BounceTime = TrueTime;
    }
    else
    {
        // Full toss: there is no bounce, so mark the point it will reach the bat.
        BouncePrediction.TrueLocation = FVector(Simulation.ContactPosition.X, Simulation.ContactPosition.Y, C26Field::SurfaceZ + 2.f);
        BouncePrediction.BounceTime = Simulation.ContactTime;
    }
    BouncePrediction.CorrectionStart = BouncePrediction.DisplayLocation;
    BouncePrediction.AppearTime = 0.f;
    BouncePrediction.FadeStartTime = BouncePrediction.BounceTime;
    BouncePrediction.FadeEndTime = BouncePrediction.BounceTime + GestureTuning.MarkerFadeDuration;
    BouncePrediction.LengthCategory = C26Controls::ClassifyLength(LockedBowling.Length);
    BouncePrediction.LineCategory = C26Controls::ClassifyLine(LockedBowling.Line);
}

void AC26MatchGameMode::UpdateBouncePrediction(float Dt)
{
    if (!PlayerBatting() || AutoPlay || (Phase != EC26Phase::RunUp && Phase != EC26Phase::Delivery))
    { BouncePrediction.bVisible = false; BouncePrediction.Alpha = 0.f; return; }

    BouncePrediction.bVisible = true;

    if (Phase == EC26Phase::Delivery && Simulation.Ball.Active)
    {
        // Smooth execution correction. Fully converged by MarkerPredictionBlendSpeed
        // of the flight, so the marker is honest well before the ball arrives and
        // never snaps between two positions.
        const float Window = FMath::Max(0.05f, BouncePrediction.BounceTime * FMath::Clamp(GestureTuning.MarkerPredictionBlendSpeed, 0.1f, 1.f));
        const float T = FMath::Clamp(Simulation.Ball.Age / Window, 0.f, 1.f);
        const float S = T * T * (3.f - 2.f * T);
        BouncePrediction.DisplayLocation = FMath::Lerp(BouncePrediction.CorrectionStart, BouncePrediction.TrueLocation, S);
        BouncePrediction.bFromIntent = S < 0.999f;
    }

    // The ACTUAL bounce (or the bat/keeper taking a full toss) ends the marker.
    if (!BouncePrediction.bBounced && Phase == EC26Phase::Delivery
        && (Simulation.BounceEvent || Simulation.CrossedContact || !Simulation.Ball.Active))
    {
        BouncePrediction.bBounced = true;
        BouncePrediction.FadeClock = 0.f;
    }

    const float Ceiling = FMath::Clamp(GestureTuning.MarkerOpacity, 0.f, 1.f);
    if (BouncePrediction.bBounced)
    {
        BouncePrediction.FadeClock += Dt;
        const float Fade = 1.f - FMath::Clamp(BouncePrediction.FadeClock / FMath::Max(0.02f, GestureTuning.MarkerFadeDuration), 0.f, 1.f);
        BouncePrediction.Alpha = Ceiling * Fade;
        if (Fade <= 0.f) BouncePrediction.bVisible = false;
    }
    else
    {
        BouncePrediction.Alpha = FMath::FInterpConstantTo(BouncePrediction.Alpha, Ceiling, Dt, Ceiling / 0.10f);
    }
}

bool AC26MatchGameMode::IsBounceIndicatorVisible() const
{
    return BouncePrediction.bVisible&&BouncePrediction.Alpha>0.01f;
}

FVector AC26MatchGameMode::GetBounceIndicatorLocation() const
{
    return BouncePrediction.DisplayLocation;
}

float AC26MatchGameMode::GetBounceIndicatorAlpha() const
{
    return BouncePrediction.Alpha;
}

FString AC26MatchGameMode::GetBounceIndicatorText() const
{
    return GetDeliveryLengthName()+TEXT("  •  ")+GetDeliveryLineName();
}

FString AC26MatchGameMode::GetBattingStateName() const
{
    switch(BattingState)
    {
    case EC26BattingState::Idle:return TEXT("IDLE");
    case EC26BattingState::ReadyForShot:return TEXT("READY");
    case EC26BattingState::Pulling:return TEXT("PULLING");
    case EC26BattingState::Armed:return TEXT("ARMED");
    case EC26BattingState::Released:return TEXT("RELEASED");
    case EC26BattingState::ShotCommitted:return TEXT("COMMITTED");
    case EC26BattingState::Contact:return TEXT("CONTACT");
    case EC26BattingState::FollowThrough:return TEXT("FOLLOW-THROUGH");
    default:return TEXT("RECOVERY");
    }
}

FString AC26MatchGameMode::GetReleaseTimingName() const
{
    return FString(C26Controls::ReleaseTimingName(BattingReleaseTiming));
}

FString AC26MatchGameMode::GetBowlingStateName() const
{
    switch(BowlingState)
    {
    case EC26BowlingState::Idle:return TEXT("IDLE");
    case EC26BowlingState::Planning:return TEXT("PLANNING");
    case EC26BowlingState::TargetDrag:return TEXT("TARGET");
    case EC26BowlingState::MovementDrag:return TEXT("MOVEMENT");
    case EC26BowlingState::PaceDrag:return TEXT("PACE");
    case EC26BowlingState::Ready:return TEXT("READY");
    case EC26BowlingState::RunUp:return TEXT("RUN-UP");
    case EC26BowlingState::ReleaseWindow:return TEXT("RELEASE WINDOW");
    case EC26BowlingState::Released:return TEXT("RELEASED");
    case EC26BowlingState::BallInFlight:return TEXT("IN FLIGHT");
    default:return TEXT("COMPLETED");
    }
}

FString AC26MatchGameMode::GetDeliveryLengthName() const
{
    const float L=(Phase==EC26Phase::Ready)?Bowling.Length:LockedBowling.Length;
    switch(C26Controls::ClassifyLength(L))
    {
    case EC26DeliveryLength::Yorker:return TEXT("YORKER");
    case EC26DeliveryLength::Full:return TEXT("FULL");
    case EC26DeliveryLength::Short:return TEXT("SHORT");
    case EC26DeliveryLength::Bouncer:return TEXT("BOUNCER");
    default:return TEXT("GOOD LENGTH");
    }
}

FString AC26MatchGameMode::GetDeliveryLineName() const
{
    const float L=(Phase==EC26Phase::Ready)?Bowling.Line:LockedBowling.Line;
    switch(C26Controls::ClassifyLine(L))
    {
    case EC26DeliveryLine::WideOff:return TEXT("WIDE OUTSIDE OFF");
    case EC26DeliveryLine::OutsideOff:return TEXT("OUTSIDE OFF");
    case EC26DeliveryLine::OffStump:return TEXT("OFF STUMP");
    case EC26DeliveryLine::MiddleStump:return TEXT("MIDDLE");
    case EC26DeliveryLine::LegStump:return TEXT("LEG STUMP");
    default:return TEXT("DOWN LEG");
    }
}
