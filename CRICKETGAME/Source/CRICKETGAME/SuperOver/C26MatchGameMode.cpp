#include "C26MatchGameMode.h"
#include "C26Athlete.h"
#include "C26CameraDirector.h"
#include "C26Stadium.h"
#include "C26Audio.h"
#include "C26Settings.h"
#include "C26PlayerController.h"
#include "C26HUD.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
// One ordered visual-acceptance beat per presentation state. Batting: -1 any, 1 player batting,
// 0 player bowling. Overlay: 0 none, 1 settings, 2 controls.
struct FC26Beat{EC26Phase Phase;int32 Batting;int32 Overlay;float After;const TCHAR* Name;float Timeout;};
const FC26Beat GC26Beats[]={
    {EC26Phase::Menu,-1,0,6.f,TEXT("01_menu"),60.f},
    {EC26Phase::Menu,-1,1,0.f,TEXT("02_settings"),60.f},
    {EC26Phase::Menu,-1,2,0.f,TEXT("03_how_to_play"),60.f},
    {EC26Phase::Intro,-1,0,1.4f,TEXT("04_intro_flyover"),60.f},
    {EC26Phase::Intro,-1,0,4.2f,TEXT("05_intro_batter"),60.f},
    {EC26Phase::Ready,1,0,.5f,TEXT("06_ready_batting"),60.f},
    {EC26Phase::RunUp,1,0,1.9f,TEXT("07_runup_batting"),60.f},
    {EC26Phase::Delivery,1,0,.3f,TEXT("08_delivery_batting"),60.f},
    {EC26Phase::InPlay,-1,0,.45f,TEXT("09_in_play"),60.f},
    {EC26Phase::InPlay,-1,0,1.6f,TEXT("10_fielding"),60.f},
    {EC26Phase::Reaction,-1,0,.7f,TEXT("11_reaction"),60.f},
    {EC26Phase::Replay,-1,0,1.2f,TEXT("12_replay"),60.f},
    {EC26Phase::Interval,-1,0,.9f,TEXT("13_interval"),220.f},
    {EC26Phase::Ready,0,0,.5f,TEXT("14_ready_bowling"),220.f},
    {EC26Phase::RunUp,0,0,1.5f,TEXT("15_runup_bowling"),60.f},
    {EC26Phase::Delivery,0,0,.3f,TEXT("16_delivery_bowling"),60.f},
    {EC26Phase::Result,-1,0,1.2f,TEXT("17_result"),340.f},
};
}

AC26MatchGameMode::AC26MatchGameMode()
{
    PrimaryActorTick.bCanEverTick=true;DefaultPawnClass=nullptr;
    PlayerControllerClass=AC26PlayerController::StaticClass();HUDClass=AC26HUD::StaticClass();
    Audio=CreateDefaultSubobject<UC26Audio>(TEXT("MatchAudio"));
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
    if(Smoke){AutoPlay=true;Preferences->Difficulty=1;StartMatch();UE_LOG(LogC26,Display,TEXT("C26_SMOKE_BEGIN: ten complete autonomous matches"));}
    Capture=FParse::Param(FCommandLine::Get(),TEXT("C26Shots"));
    if(Capture)
    {
        AutoPlay=true;Preferences->Difficulty=1;PlayerBatsFirst=true;UseToss=false;
        Preferences->Quality=3;Preferences->Apply();Venue->SetQuality(3);
        UE_LOG(LogC26,Display,TEXT("C26_SHOTS_BEGIN: %d visual acceptance beats"),int(UE_ARRAY_COUNT(GC26Beats)));
    }
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
FString AC26MatchGameMode::BowlerName()const{return BattingTeam()==0?TEXT("N. ARCHER"):TEXT("V. SEN");}
void AC26MatchGameMode::BuildMatchActors()
{
    FieldPositions=FC26AI::Field();
    for(int I=0;I<14;++I)Athletes.Add(GetWorld()->SpawnActor<AC26Athlete>());
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* White=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_White.M_White"));
    AActor* Props=GetWorld()->SpawnActor<AActor>();Props->SetRootComponent(NewObject<USceneComponent>(Props));Props->GetRootComponent()->RegisterComponent();
    BallMesh=NewObject<UStaticMeshComponent>(Props,TEXT("WhiteCricketBall"));BallMesh->SetupAttachment(Props->GetRootComponent());
    BallMesh->SetStaticMesh(Sphere);BallMesh->SetWorldScale3D(FVector(Tuning.BallRadius*2/100.f*3.5f));BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);BallMesh->RegisterComponent();
    auto* BallMaterial=UMaterialInstanceDynamic::Create(White,this);BallMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.98,.97,.88));BallMaterial->SetScalarParameterValue(TEXT("Glow"),.8f);BallMesh->SetMaterial(0,BallMaterial);
    for(int End=0;End<2;++End)for(int I=0;I<5;++I)
    {
        auto* S=NewObject<UStaticMeshComponent>(Props);S->SetupAttachment(Props->GetRootComponent());S->SetStaticMesh(Cylinder);S->SetMaterial(0,White);S->SetCollisionEnabled(ECollisionEnabled::NoCollision);S->RegisterComponent();Stumps.Add(S);
    }
    ResetStumps();
    FirstBattingTeam=0;
    for(int I=0;I<11;++I){Athletes[I]->Configure(I==0?EC26Role::Bowler:I==1?EC26Role::Keeper:EC26Role::Fielder,1,I+1);Athletes[I]->ResetAt(FieldPositions[I],(FVector(0,850,0)-FieldPositions[I]).Rotation().Yaw);}
    Athletes[11]->Configure(EC26Role::Batter,0,7);Athletes[11]->ResetAt(FVector(-38,900,5),-90);
    Athletes[12]->Configure(EC26Role::Batter,0,18);Athletes[12]->ResetAt(FVector(-80,-865,5),90);
    Athletes[13]->Configure(EC26Role::Umpire,0,0);Athletes[13]->ResetAt(FVector(40,-1280,5),90);
    Simulation.Ball.Position=FVector(0,0,-100);UpdateBallVisual();
}
void AC26MatchGameMode::ResetStumps()
{
    for(int End=0;End<2;++End)for(int I=0;I<5;++I)
    {
        auto S=Stumps[End*5+I];const float Y=End?C26Field::WicketY:-C26Field::WicketY;
        if(I<3){S->SetWorldLocation(FVector((I-1)*10.2f,Y,40.55f));S->SetWorldRotation(FRotator::ZeroRotator);S->SetWorldScale3D(FVector(.038,.038,.711));}
        else{S->SetWorldLocation(FVector((I==3?-1:1)*5.1f,Y,77.f));S->SetWorldRotation(FRotator(0,0,90));S->SetWorldScale3D(FVector(.026,.026,.1095));}
    }
}
void AC26MatchGameMode::BreakWicket(float Y)
{
    const int E=Y>0?5:0;
    Stumps[E+3]->SetWorldLocation(FVector(-14,Y+21,67));Stumps[E+3]->SetWorldRotation(FRotator(24,38,40));
    Stumps[E+4]->SetWorldLocation(FVector(18,Y+35,48));Stumps[E+4]->SetWorldRotation(FRotator(55,-28,20));
    Stumps[E+1]->SetWorldRotation(FRotator(-8,0,0));Audio->Cue(TEXT("stump_hit"),.9f);Haptic(.6f);
}
void AC26MatchGameMode::ChangePhase(EC26Phase NewPhase)
{
    UE_LOG(LogC26,Log,TEXT("Match %u phase %d -> %d | delivery %u | innings %d | ball %d | score %d/%d"),Rules.Epoch,int(Phase),int(NewPhase),DeliveryId,Rules.Current+1,Rules.Now().LegalBalls,Rules.Now().Runs,Rules.Now().Wickets);
    Phase=NewPhase;PhaseTime=0;OnMatchChanged.Broadcast();
}
void AC26MatchGameMode::StartMatch()
{
    Rules.Reset();Simulation.Reset();Simulation.Tuning=Tuning;AI.Reset(260026+Rules.Epoch*917+FMath::RandRange(0,999));
    Director->Reset();Audio->Reset();ResetStumps();Paused=SettingsOpen=ControlsOpen=false;
    Running=Returning=ReleaseLocked=ShotQueued=Resolved=false;ThrowClock=-1;RequestedRuns=CompletedRuns=0;RunProgress=0;Intent={};Footwork=0;
    FirstBattingTeam=PlayerBatsFirst?PlayerTeam:1-PlayerTeam;
    if(UseToss){const bool Won=AI.Random.FRand()>.5f;FirstBattingTeam=Won?PlayerTeam:1-PlayerTeam;TossText=TeamShort(FirstBattingTeam)+TEXT(" WIN THE TOSS  /  BAT FIRST");}
    else TossText=TeamShort(FirstBattingTeam)+TEXT(" BAT FIRST  /  SIX BALLS. TWO WICKETS.");
    Callout=TEXT("ONE OVER. ALL TO PLAY FOR.");Detail=TEXT("ECLIPSE OVAL  /  NIGHT SESSION");
    PrepareDelivery();ChangePhase(EC26Phase::Intro);SmokeWatchdog=0;
}
void AC26MatchGameMode::Menu()
{
    Director->Restore(Athletes);Director->Reset();Simulation.Reset();Audio->Reset();Paused=false;SettingsOpen=false;ChangePhase(EC26Phase::Menu);
}
void AC26MatchGameMode::PrepareDelivery()
{
    Simulation.Reset();Simulation.Tuning=Tuning;Director->Reset();ResetStumps();
    Pending={};Pending.Epoch=Rules.Epoch;Pending.Id=++DeliveryId;
    Resolved=Important=ShotQueued=ReleaseLocked=Running=Returning=false;RunProgress=0;RequestedRuns=CompletedRuns=0;
    ActiveFielder=-1;ThrowClock=-1;FieldDecisionClock=0;LastContact={};Callout.Empty();Detail.Empty();
    Intent.Footwork=Footwork;RunnerAId=Rules.Now().Striker;RunnerBId=Rules.Now().NonStriker;
    FieldPositions=FC26AI::Field(AI.History.OffsideBias>.3f);
    for(int I=0;I<11;++I)
    {
        Athletes[I]->Configure(I==0?EC26Role::Bowler:I==1?EC26Role::Keeper:EC26Role::Fielder,1-BattingTeam(),I+1);
        Athletes[I]->ResetAt(FieldPositions[I],(FVector(0,850,0)-FieldPositions[I]).Rotation().Yaw);
    }
    Athletes[11]->Configure(EC26Role::Batter,BattingTeam(),7);Athletes[11]->ResetAt(FVector(-38+Footwork*35,900,5),-90);
    Athletes[12]->Configure(EC26Role::Batter,BattingTeam(),18);Athletes[12]->ResetAt(FVector(-80,-865,5),90);
    Athletes[13]->SetAction(EC26Action::Ready);
    Bowling=PlayerBatting()||AutoPlay?AI.Bowl(Rules,Preferences->Difficulty):FC26DeliveryPlan();
    Simulation.Ball.Position=Athletes[0]->HandPosition();UpdateBallVisual();
    Audio->SetTension(Rules.Current==1?1.f-float(Rules.BallsRemaining())/7.f:.15f,Preferences->SoundVolume);
    ChangePhase(EC26Phase::Ready);
}
void AC26MatchGameMode::StartDelivery()
{
    if(Phase!=EC26Phase::Ready||Paused)return;
    Athletes[0]->ResetAt(FVector(-20,-2250,5),90);Athletes[0]->SetAction(EC26Action::Running);
    ChangePhase(EC26Phase::RunUp);Audio->Cue(TEXT("ui_button_click"),.2f);
}
float AC26MatchGameMode::BowlingMeter()const{return FMath::Clamp(PhaseTime/2.5f,0.f,1.f);}
void AC26MatchGameMode::BowlRelease()
{
    if(Phase!=EC26Phase::RunUp||ReleaseLocked||PlayerBatting())return;
    const float Forgiveness=Preferences->Difficulty==0?.29f:Preferences->Difficulty==2?.16f:.22f;
    ReleaseQuality=(PhaseTime-2.12f)/Forgiveness;ReleaseLocked=true;Haptic(.16f);
}
void AC26MatchGameMode::ReleaseBall()
{
    if(!PlayerBatting()&&!AutoPlay)
    {
        if(!ReleaseLocked)ReleaseQuality=1.25f;
        Bowling.Line+=FMath::Clamp(ReleaseQuality,-2.f,2.f)*43.f;
        Bowling.Length+=FMath::Clamp(ReleaseQuality,-2.f,2.f)*38.f;
        Bowling.NoBall=ReleaseQuality>1.15f;
        Detail=FMath::Abs(ReleaseQuality)<.4f?TEXT("EXCELLENT RELEASE"):FMath::Abs(ReleaseQuality)<1?TEXT("GOOD RELEASE"):TEXT("RELEASE OFF TARGET");
    }
    FVector Origin=Athletes[0]->HandPosition();
    Simulation.Release(Bowling,Origin);Pending.NoBall=Bowling.NoBall;
    if(!PlayerBatting()||AutoPlay){Intent=AI.Bat(Bowling,Rules,Preferences->Difficulty);AITiming=AI.TimingError(Preferences->Difficulty);}
    ChangePhase(EC26Phase::Delivery);
}
float AC26MatchGameMode::TimingCountdown()const{return Simulation.ContactTime-Simulation.Ball.Age;}
void AC26MatchGameMode::Shot(const FC26ShotIntent& NewIntent)
{
    if((Phase!=EC26Phase::Delivery&&Phase!=EC26Phase::RunUp)||ShotQueued||(!PlayerBatting()&&!AutoPlay))return;
    ShotQueued=true;Intent=NewIntent;Intent.Footwork=Footwork;
    ShotInputTime=Phase==EC26Phase::RunUp?-1.f:Simulation.Ball.Age;
    Athletes[11]->SetShotContact(Simulation.ContactPosition,Intent.Angle,Intent.Loft);
}
void AC26MatchGameMode::AimPitch(float Line,float Length)
{if(Phase==EC26Phase::Ready&&!PlayerBatting()){Bowling.Line=FMath::Clamp(Line,-135.f,135.f);Bowling.Length=FMath::Clamp(Length,30.f,840.f);}}
void AC26MatchGameMode::UpdateDelivery(float Dt)
{
    const bool AIAtBat=!PlayerBatting()||AutoPlay;
    if(AIAtBat&&!ShotQueued&&Simulation.Ball.Age>=Simulation.ContactTime-.10f+AITiming)
    {
        ShotQueued=true;ShotInputTime=Simulation.Ball.Age;Athletes[11]->SetShotContact(Simulation.ContactPosition,Intent.Angle,Intent.Loft);
    }
    // Stop precisely at the contact plane before applying a rebound; no jump to a distant bat.
    const float Remaining=Simulation.ContactTime-Simulation.Ball.Age;
    if(!Simulation.CrossedContact&&Remaining>0&&Remaining<Dt)
    {
        Simulation.Step(Remaining);
        if(Simulation.BounceEvent)Audio->Cue(TEXT("ball_bounce"),.45f);
        Simulation.CrossedContact=true;
        if(ShotQueued)
        {
            float Error=ShotInputTime-(Simulation.ContactTime-.10f);
            LastContact=Simulation.Hit(Intent,Error,Preferences->Difficulty,AI.Random);
            if(LastContact.Timing!=EC26Timing::Miss)
            {
                Athletes[11]->ContactTarget=Simulation.Ball.Position;Athletes[11]->ActionTime=.247f;Athletes[11]->Animate(0);
                Audio->Cue(LastContact.Timing==EC26Timing::Edge?TEXT("bat_edge"):Intent.Defend?TEXT("bat_defensive"):TEXT("bat_sweet_spot"),.8f);
                Haptic(LastContact.Timing==EC26Timing::Perfect?.45f:.2f);AI.History.OffsideBias=FMath::Lerp(AI.History.OffsideBias,Intent.Angle>0?1.f:-1.f,.3f);
                Detail=LastContact.Shot;ChangePhase(EC26Phase::InPlay);return;
            }
        }
    }
    Simulation.Step(Dt);
    if(Simulation.BounceEvent)Audio->Cue(TEXT("ball_bounce"),.4f);
    if(Simulation.StumpEvent)
    {
        if(Pending.NoBall||Rules.Now().FreeHit){Simulation.Ball.Active=false;BreakWicket(C26Field::WicketY);Resolve();}
        else{Pending.Wicket=C26::Dismissal::Bowled;BreakWicket(C26Field::WicketY);Resolve();}return;
    }
    if(Simulation.Ball.Position.Y>1240||Simulation.Ball.Age>2.f)
    {
        if(FMath::Abs(Simulation.ContactPosition.X)>90&&!Pending.NoBall)Pending.WideRuns=1;
        Athletes[1]->SetAction(EC26Action::Catch);Audio->Cue(TEXT("keeper_catch"),.6f);Simulation.Ball.Active=false;Resolve();
    }
}
void AC26MatchGameMode::Run()
{
    if(Phase!=EC26Phase::InPlay||Resolved)return;
    RequestedRuns=FMath::Min(3,RequestedRuns+1);
    if(!Running)
    {
        RunFromA=Athletes[11]->GetActorLocation();RunFromB=Athletes[12]->GetActorLocation();
        RunToA=FVector(-45,RunFromA.Y>0?-C26Field::CreaseY:C26Field::CreaseY,5);
        RunToB=FVector(35,RunFromB.Y>0?-C26Field::CreaseY:C26Field::CreaseY,5);
        Running=true;Returning=false;RunProgress=0;
        Athletes[11]->SetActorRotation((RunToA-RunFromA).Rotation());Athletes[12]->SetActorRotation((RunToB-RunFromB).Rotation());
        Athletes[11]->SetAction(EC26Action::Running);Athletes[12]->SetAction(EC26Action::Running);
    }
}
void AC26MatchGameMode::CancelRun(){RequestedRuns=0;if(Running&&RunProgress<.45f)Returning=true;}
void AC26MatchGameMode::UpdateRunning(float Dt)
{
    if(!Running)return;
    RunProgress+=Dt*Tuning.RunnerSpeed/1768.f*(Returning?-1.f:1.f);
    Athletes[11]->SetActorLocation(FMath::Lerp(RunFromA,RunToA,FMath::Clamp(RunProgress,0.f,1.f)));
    Athletes[12]->SetActorLocation(FMath::Lerp(RunFromB,RunToB,FMath::Clamp(RunProgress,0.f,1.f)));
    if(RunProgress<=0&&Returning){Running=false;Returning=false;Athletes[11]->SetAction(EC26Action::Ready);Athletes[12]->SetAction(EC26Action::Ready);}
    if(RunProgress>=1)
    {
        ++CompletedRuns;Pending.CompletedRuns=CompletedRuns;
        if(Simulation.Ball.Struck)Pending.BatRuns=CompletedRuns;else Pending.Byes=CompletedRuns;
        Running=false;RequestedRuns=FMath::Max(0,RequestedRuns-1);
        Athletes[11]->SetAction(EC26Action::Ready);Athletes[12]->SetAction(EC26Action::Ready);
        if(Rules.Current==1&&Rules.Now().Runs+CompletedRuns+(Pending.NoBall?1:0)>=Rules.Target()){Resolve();return;}
        if(RequestedRuns>0){--RequestedRuns;Run();}
    }
}
void AC26MatchGameMode::Collect(int Fielder,bool Catch)
{
    ActiveFielder=Fielder;Simulation.Ball.Active=false;
    Athletes[Fielder]->SetAction(Catch?EC26Action::Catch:EC26Action::Pickup);
    Audio->Cue(Catch?TEXT("keeper_catch"):TEXT("fielder_gather"),.6f);
    if(Catch&&!Pending.NoBall&&!Rules.Now().FreeHit){Pending.Wicket=C26::Dismissal::Caught;Resolve();return;}
    ThrowClock=0;ThrowRunner=Running?(FVector::Dist2D(Athletes[Fielder]->GetActorLocation(),RunToA)<FVector::Dist2D(Athletes[Fielder]->GetActorLocation(),RunToB)?0:1):0;
    ThrowTo=Running?(ThrowRunner==0?RunToA:RunToB):FVector(0,C26Field::WicketY,0);ThrowTo.X=0;ThrowTo.Y=ThrowTo.Y>0?C26Field::WicketY:-C26Field::WicketY;ThrowTo.Z=42;
    ThrowFrom=Athletes[Fielder]->GetActorLocation()+FVector(0,0,135);ThrowDuration=FVector::Dist(ThrowFrom,ThrowTo)/2400.f;
}
void AC26MatchGameMode::UpdateFielding(float Dt)
{
    if(ThrowClock>=0)
    {
        ThrowClock+=Dt;
        if(ThrowClock<.5f){Simulation.Ball.Position=Athletes[ActiveFielder]->HandPosition();return;}
        if(Athletes[ActiveFielder]->Action!=EC26Action::Throw)Athletes[ActiveFielder]->SetAction(EC26Action::Throw);
        const float T=FMath::Clamp((ThrowClock-.5f)/FMath::Max(.18f,ThrowDuration),0.f,1.f);
        Simulation.Ball.Position=FMath::Lerp(ThrowFrom,ThrowTo,T)+FVector(0,0,180*FMath::Sin(T*PI));
        if(T>=1)
        {
            if(Running&&!Returning&&RunProgress<.98f)
            {
                Pending.Wicket=C26::Dismissal::RunOut;Pending.DismissedBatter=ThrowRunner==0?RunnerAId:RunnerBId;Pending.CrossedOnRunOut=RunProgress>.5f;
                BreakWicket(ThrowTo.Y);
            }
            Audio->Cue(TEXT("keeper_catch"),.45f);Resolve();
        }return;
    }
    Simulation.Step(Dt);
    if(Simulation.BounceEvent)Audio->Cue(TEXT("ball_bounce"),.28f);
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
        for(int I=0;I<11;++I)
        {
            for(float T=.15f;T<5.1f;T+=.25f)
            {
                const auto P=Simulation.Predict(T);
                if(P.Position.Z>190||!C26Field::Inside(P.Position))continue;
                const float Needed=FVector::Dist2D(Athletes[I]->GetActorLocation(),P.Position)/Tuning.FielderSpeed+.15f;
                if(Needed<T&&T<Best){Best=T;Chosen=I;Target=P.Position;break;}
            }
        }
        if(Chosen<0)
        {
            for(int I=0;I<11;++I){float D=FVector::DistSquared2D(Athletes[I]->GetActorLocation(),Target);if(D<Best){Best=D;Chosen=I;}}
        }
        ActiveFielder=Chosen;Intercept=Target;Intercept.Z=5;
    }
    for(int I=0;I<11;++I)
    {
        auto F=Athletes[I];
        if(I==ActiveFielder)
        {
            FVector Delta=Intercept-F->GetActorLocation();Delta.Z=0;
            if(Delta.Size()>35){F->SetActorRotation(Delta.Rotation());F->AddActorWorldOffset(Delta.GetClampedToMaxSize(Tuning.FielderSpeed*Dt));F->SetAction(EC26Action::Running,false);}
            else F->SetAction(Simulation.Ball.Position.Z>55?EC26Action::Catch:EC26Action::Ready,false);
            const float D=FVector::Dist2D(F->GetActorLocation(),Simulation.Ball.Position);
            if(D<65&&Simulation.Ball.Position.Z<190&&Simulation.Ball.Velocity.Z<40)
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
        Important=true;++SmokeWickets;Audio->Cue(TEXT("wicket_roar"),.85f);Athletes[13]->SetAction(EC26Action::SignalOut);
        Athletes[11]->SetAction(EC26Action::Disappointed);for(int I=0;I<11;++I)Athletes[I]->SetAction(EC26Action::Celebrate);
    }
    else if(Official.Rope!=C26::Boundary::None)
    {
        bool Six=Official.Rope==C26::Boundary::Six;Callout=Six?TEXT("SIX"):TEXT("FOUR");Detail=LastContact.Shot;
        Important=true;++SmokeBoundaries;Audio->Cue(Six?TEXT("crowd_six"):TEXT("crowd_four"),Six?.85f:.7f);
        Athletes[13]->SetAction(Six?EC26Action::SignalSix:EC26Action::SignalFour);Athletes[11]->SetAction(EC26Action::Celebrate);
    }
    else if(Official.NoBall){Callout=TEXT("NO BALL");Detail=TEXT("FREE HIT NEXT DELIVERY");++SmokeExtras;Athletes[13]->SetAction(EC26Action::SignalWide);}
    else if(Official.WideRuns){Callout=TEXT("WIDE");Detail=TEXT("EXTRA RUN  /  BALL DOES NOT COUNT");++SmokeExtras;Athletes[13]->SetAction(EC26Action::SignalWide);}
    else{int Runs=Official.BatRuns+Official.Byes+Official.LegByes;Callout=Runs?FString::Printf(TEXT("%d %s"),Runs,Runs==1?TEXT("RUN"):TEXT("RUNS")):TEXT("DOT BALL");Detail=Runs?TEXT("GOOD RUNNING BETWEEN THE WICKETS"):TEXT("PRESSURE BUILDS");}
    Venue->React(Important?1:.2f);OnMatchChanged.Broadcast();ChangePhase(EC26Phase::Reaction);
}
void AC26MatchGameMode::AfterPresentation()
{
    if(Rules.Winner!=C26::Result::Playing)
    {
        const int Winner=Rules.Winner==C26::Result::FirstTeam?FirstBattingTeam:1-FirstBattingTeam;
        Callout=Rules.Winner==C26::Result::Tie?TEXT("MATCH TIED"):Winner==PlayerTeam?TEXT("VICTORY"):TEXT("DEFEAT");
        Detail=Rules.Winner==C26::Result::Tie?TEXT("LEVEL AFTER TWO SUPER OVERS"):TeamName(Winner)+TEXT(" WIN THE SUPER OVER");
        Audio->Cue(TEXT("ui_result_sting"),Preferences->MusicVolume);Athletes[11]->SetAction(Winner==BattingTeam()?EC26Action::Celebrate:EC26Action::Disappointed);
        ChangePhase(EC26Phase::Result);
        UE_LOG(LogC26,Display,TEXT("C26_MATCH_COMPLETE epoch=%u first=%d/%d second=%d/%d result=%d"),Rules.Epoch,Rules.Scores[0].Runs,Rules.Scores[0].Wickets,Rules.Scores[1].Runs,Rules.Scores[1].Wickets,int(Rules.Winner));
    }
    else if(Rules.Now().Closed)
    {Callout=FString::Printf(TEXT("TARGET %d"),Rules.Target());Detail=TEXT("SIX BALLS TO MAKE IT YOURS");Audio->Cue(TEXT("ui_result_sting"),.4f);ChangePhase(EC26Phase::Interval);}
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
    // Deterministic visual-acceptance pass: drive the presentation and write one frame per beat.
    if(!Capture)return;
    if(CaptureHold>0){CaptureHold-=Dt;return;}
    if(Phase==EC26Phase::Menu&&CaptureIndex>=3){SettingsOpen=ControlsOpen=false;StartMatch();return;}
    if(CaptureIndex>=UE_ARRAY_COUNT(GC26Beats))
    {
        UE_LOG(LogC26,Display,TEXT("C26_SHOTS_COMPLETE captured=%d"),CaptureIndex);
        Capture=false;SettingsOpen=ControlsOpen=false;FPlatformMisc::RequestExit(false);return;
    }
    const FC26Beat& Beat=GC26Beats[CaptureIndex];
    const bool Overlay=Beat.Overlay!=0;
    const bool Matches=Phase==Beat.Phase&&(Beat.Batting<0||PlayerBatting()==(Beat.Batting==1));
    if(!Matches)
    {
        SettingsOpen=ControlsOpen=false;CaptureWait+=Dt;
        if(CaptureWait>Beat.Timeout)
        {
            UE_LOG(LogC26,Warning,TEXT("C26_SHOT_SKIPPED %s: state never reached in 60s (phase=%d)"),Beat.Name,int(Phase));
            CaptureWait=0;++CaptureIndex;
        }
        return;
    }
    if(Overlay&&!(Beat.Overlay==1?SettingsOpen:ControlsOpen))
    {
        SettingsOpen=Beat.Overlay==1;ControlsOpen=Beat.Overlay==2;CaptureHold=.45f;return;
    }
    if(!Overlay&&PhaseTime<Beat.After){CaptureWait+=Dt;return;}
    const FString File=FPaths::ProjectDir()/TEXT("Artifacts/Shots")/FString(Beat.Name)+TEXT(".png");
    FScreenshotRequest::RequestScreenshot(File,true,false);
    UE_LOG(LogC26,Display,TEXT("C26_SHOT %s -> %s"),Beat.Name,*File);
    ++CaptureIndex;CaptureWait=0;CaptureHold=.35f;
    if(Overlay){SettingsOpen=ControlsOpen=false;}
}
void AC26MatchGameMode::UpdateBallVisual()
{if(BallMesh){BallMesh->SetWorldLocation(Simulation.Ball.Position);BallMesh->AddLocalRotation(FRotator(11,4,0));}}
void AC26MatchGameMode::Haptic(float Strength)
{if(Preferences->Vibration)if(auto* P=GetWorld()->GetFirstPlayerController())P->PlayDynamicForceFeedback(Strength,.08f,true,true,true,true);}
void AC26MatchGameMode::Tick(float Dt)
{
    Super::Tick(Dt);if(!Director||Athletes.Num()!=14)return;
    Dt=FMath::Min(Dt,.05f);Clock+=Dt;UpdateCapture(Dt);if(Paused||SettingsOpen||ControlsOpen)return;
    PhaseTime+=Dt;
    if(Phase==EC26Phase::Intro&&PhaseTime>6.5f)PrepareDelivery();
    else if(Phase==EC26Phase::Ready&&AutoPlay&&PhaseTime>(Capture?1.1f:.45f))StartDelivery();
    else if(Phase==EC26Phase::RunUp)
    {
        const float T=FMath::Clamp(PhaseTime/2.5f,0.f,1.f);
        Athletes[0]->SetActorLocation(FVector(-20,FMath::Lerp(-2250.f,-940.f,T),5));
        if(PhaseTime>2.22f&&Athletes[0]->Action!=EC26Action::Bowling)Athletes[0]->SetAction(EC26Action::Bowling);
        Simulation.Ball.Position=Athletes[0]->HandPosition();if(PhaseTime>=2.5f)ReleaseBall();
    }
    else if(Phase==EC26Phase::Delivery)UpdateDelivery(Dt);
    else if(Phase==EC26Phase::InPlay){UpdateRunning(Dt);if(!Resolved)UpdateFielding(Dt);}
    else if(Phase==EC26Phase::Reaction&&PhaseTime>(Important?1.5f:1.1f))
    {if(Important&&Director->BeginReplay()){++SmokeReplays;ChangePhase(EC26Phase::Replay);Audio->Cue(TEXT("ui_button_click"),.25f);}else AfterPresentation();}
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
        for(auto A:Athletes)A->Animate(Dt);
        if(Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay||Phase==EC26Phase::RunUp)Director->Record(Dt,Simulation.Ball.Position,Athletes);
    }
    UpdateBallVisual();Director->Direct(Phase,PhaseTime,PlayerBatting(),Simulation.Ball.Position,Simulation.Ball.Velocity,Intent.Loft);Venue->UpdateAtmosphere(Clock);
    if(Smoke){SmokeWatchdog+=Dt;if(SmokeWatchdog>240){UE_LOG(LogC26,Error,TEXT("C26_SMOKE_TIMEOUT phase=%d delivery=%u"),int(Phase),DeliveryId);FPlatformMisc::RequestExit(false);Smoke=false;}}
}
void AC26MatchGameMode::UIAction(FName Action)
{
    Audio->Cue(TEXT("ui_button_click"),.25f);
    if(Action==TEXT("play")||Action==TEXT("again"))StartMatch();
    else if(Action==TEXT("menu"))Menu();
    else if(Action==TEXT("team"))PlayerTeam=1-PlayerTeam;
    else if(Action==TEXT("batfirst")){PlayerBatsFirst=true;UseToss=false;}
    else if(Action==TEXT("bowlfirst")){PlayerBatsFirst=false;UseToss=false;}
    else if(Action==TEXT("toss"))UseToss=!UseToss;
    else if(Action==TEXT("settings"))SettingsOpen=!SettingsOpen;
    else if(Action==TEXT("help"))ControlsOpen=!ControlsOpen;
    else if(Action==TEXT("close")){SettingsOpen=false;ControlsOpen=false;Paused=false;Preferences->Save();}
    else if(Action==TEXT("difficulty")){Preferences->Difficulty=(Preferences->Difficulty+1)%3;Preferences->Save();}
    else if(Action==TEXT("quality")){Preferences->Quality=(Preferences->Quality+1)%4;Preferences->Apply();Venue->SetQuality(Preferences->Quality);Preferences->Save();}
    else if(Action==TEXT("sound")){Preferences->SoundVolume=Preferences->SoundVolume>.1f?0:.75f;Audio->SetTension(.1f,Preferences->SoundVolume);Preferences->Save();}
    else if(Action==TEXT("vibration")){Preferences->Vibration=!Preferences->Vibration;Preferences->Save();}
    else if(Action==TEXT("sensitivity")){Preferences->Sensitivity=Preferences->Sensitivity>=1.49f?.7f:Preferences->Sensitivity+.4f;Preferences->Save();}
    else if(Action==TEXT("pause"))Paused=!Paused;
    else if(Action==TEXT("skip"))Skip();
    else if(Action==TEXT("ready"))StartDelivery();
    else if(Action==TEXT("release"))BowlRelease();
    else if(Action==TEXT("loft")){Intent.Loft=!Intent.Loft;Intent.Defend=false;}
    else if(Action==TEXT("defend")){Intent.Defend=!Intent.Defend;Intent.Loft=false;}
    else if(Action==TEXT("run"))Run();
    else if(Action==TEXT("cancel"))CancelRun();
    else if(Action==TEXT("delivery"))
    {
        Bowling.Type=EC26Delivery((int(Bowling.Type)+1)%6);Bowling.Speed=Bowling.Type==EC26Delivery::Slower?2450:3300;
        Bowling.Length=Bowling.Type==EC26Delivery::Yorker?810:Bowling.Type==EC26Delivery::Bouncer?40:460;
        Bowling.Bounce=Bowling.Type==EC26Delivery::Bouncer?.72f:.55f;
        Bowling.Swing=Bowling.Type==EC26Delivery::Outswing?180:Bowling.Type==EC26Delivery::Inswing?-180:0;
    }
}
void AC26MatchGameMode::DebugOutcome(FString Type)
{
#if !UE_BUILD_SHIPPING
    if(Phase==EC26Phase::Menu)StartMatch();
    if(Phase==EC26Phase::Intro)PrepareDelivery();
    if(Phase!=EC26Phase::Ready)return;
    if(Type==TEXT("six")){Pending.Rope=C26::Boundary::Six;Pending.BatRuns=6;}
    else if(Type==TEXT("four")){Pending.Rope=C26::Boundary::Four;Pending.BatRuns=4;}
    else if(Type==TEXT("wicket"))Pending.Wicket=C26::Dismissal::Bowled;
    else if(Type==TEXT("wide"))Pending.WideRuns=1;
    else if(Type==TEXT("noball"))Pending.NoBall=true;
    Resolve();
#endif
}
