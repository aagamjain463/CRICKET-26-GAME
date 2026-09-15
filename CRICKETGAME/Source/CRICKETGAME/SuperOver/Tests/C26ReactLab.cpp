#include "../C26MatchGameMode.h"

#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26CameraDirector.h"
#include "Characters/C26CharacterPresentationComponent.h"
#include "Characters/C26CricketerAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

// Round 7 reaction lab. Stage 0 plays real AI deliveries (release/contact must carry no procedural layer, and
// the bowler must settle out of the delivery clip). Stages 1-8 commit outcomes through the real Resolve() -
// the same path a live ball takes - and measure who reacts, with which clip, how smoothly, and that each
// reaction hands back to a ready state. Run: Tools/ReactLab.sh
namespace
{
struct FReactCase { const TCHAR* Name; int32 Subject; TArray<TPair<int32,const TCHAR*>> Cues; };
const TArray<FReactCase>& Cases()
{
    static const TArray<FReactCase> List={
        {TEXT("delivery"),0,{}},
        {TEXT("four"),11,{{11,TEXT("Boundary")},{0,TEXT("BoundaryConceded")},{12,TEXT("Support")}}},
        {TEXT("six"),11,{{11,TEXT("Six")},{0,TEXT("BoundaryConceded")}}},
        {TEXT("bowled"),0,{{11,TEXT("Dismissed")},{0,TEXT("Wicket")},{1,TEXT("Wicket")},{5,TEXT("Wicket")}}},
        {TEXT("caught"),6,{{6,TEXT("Catch")},{0,TEXT("Wicket")},{11,TEXT("Dismissed")},{3,TEXT("Wicket")}}},
        {TEXT("playandmiss"),11,{{11,TEXT("PlayAndMiss")},{0,TEXT("NearMiss")},{1,TEXT("Appeal")}}},
        {TEXT("edge"),11,{{11,TEXT("Edge")},{0,TEXT("NearMiss")},{1,TEXT("Support")}}},
        {TEXT("dropped"),4,{{4,TEXT("Dropped")},{0,TEXT("Dropped")}}},
        {TEXT("dot"),11,{{11,TEXT("Dot")},{0,TEXT("DotConfidence")}}},
    };
    return List;
}
UC26CricketerAnimInstance* AnimOf(const AC26Athlete* A)
{
    return A&&A->Presentation&&A->Presentation->IsActive()&&A->Presentation->Body?Cast<UC26CricketerAnimInstance>(A->Presentation->Body->GetAnimInstance()):nullptr;
}
}

void AC26MatchGameMode::UpdateReactLab(float Dt)
{
    auto Check=[&](bool Passed,const FString& Message)
    {
        if(!Passed)++ReactFailures;
        UE_LOG(LogC26,Display,TEXT("C26_REACT_CHECK stage=%d %s %s"),ReactStage,Passed?TEXT("PASS"):TEXT("FAIL"),*Message);
    };
    const auto Finish=[&]()
    {
        UE_LOG(LogC26,Display,TEXT("C26_REACTLAB_%s failures=%d"),ReactFailures?TEXT("FAIL"):TEXT("PASS"),ReactFailures);
        ReactLab=false;FPlatformMisc::RequestExitWithStatus(false,ReactFailures?1:0);
    };
    if(FPlatformTime::Seconds()-ReactStarted>420){UE_LOG(LogC26,Error,TEXT("C26_REACTLAB_TIMEOUT stage=%d phase=%d"),ReactStage,int(Phase));++ReactFailures;Finish();return;}
    const FReactCase& Case=Cases()[FMath::Min(ReactStage,Cases().Num()-1)];
    const EC26Phase Prev=ReactPrevPhase;ReactPrevPhase=Phase;

    // Frame the stage's subject from a three-quarter front view.
    const auto Shoot=[&](int32 Index,const TCHAR* Beat)
    {
        const FString Key=FString::Printf(TEXT("%d_%s_%s"),ReactStage,Case.Name,Beat);
        if(ReactShots.Contains(Key)||!Athletes.IsValidIndex(Index))return;
        ReactShots.Add(Key);
        if(!ReactCamera.IsValid())ReactCamera=GetWorld()->SpawnActor<ACameraActor>();
        const AC26Athlete* A=Athletes[Index];
        const FVector At=A->GetActorLocation();
        const FVector Eye=At+A->GetActorForwardVector()*520+A->GetActorRightVector()*260+FVector(0,0,140);
        ReactCamera->SetActorLocation(Eye);ReactCamera->SetActorRotation((At+FVector(0,0,100)-Eye).Rotation());
        ReactCamera->GetCameraComponent()->SetFieldOfView(34);
        IFileManager::Get().MakeDirectory(*GateDirectory,true);
        FScreenshotRequest::RequestScreenshot(GateDirectory/(Key+TEXT(".png")),false,false);
        UE_LOG(LogC26,Display,TEXT("C26_REACT_FRAME %s state=%s"),*Key,*A->Presentation->CurrentState.ToString());
    };
    if(ReactCamera.IsValid())if(auto* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(ReactCamera.Get());

    if(Phase==EC26Phase::Replay){Skip();return;}
    if(Phase==EC26Phase::Interval||Phase==EC26Phase::Intro){Skip();return;}
    if(Phase==EC26Phase::Result||Phase==EC26Phase::Menu){Finish();return;}

    if(ReactStage==0)
    {
        // Gameplay event frames must be exactly the authored pose.
        if(Prev==EC26Phase::RunUp&&Phase==EC26Phase::Delivery)
            if(auto* Anim=AnimOf(Athletes[0]))Check(Anim->Life.IsZero(),TEXT("release frame carries no procedural layer"));
        if(Prev==EC26Phase::Delivery&&Phase==EC26Phase::InPlay)
            if(auto* Anim=AnimOf(Athletes[11]))Check(Anim->Life.IsZero(),TEXT("bat contact frame carries no procedural layer"));
        const AC26Athlete* Bowler=Athletes[0];
        if(Bowler->Action==EC26Action::Bowling&&Bowler->Presentation->IsRecovered()&&!ReactShots.Contains(TEXT("bowler_recovered")))
        {
            ReactShots.Add(TEXT("bowler_recovered"));
            const FName S=Bowler->Presentation->CurrentState;
            Check(S==TEXT("BowlerReady")||S==TEXT("Walk"),FString::Printf(TEXT("delivery completion settles to %s (action time %.2f)"),*S.ToString(),Bowler->ActionTime));
            Shoot(0,TEXT("settled"));
        }
        if((Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay)&&Bowler->Action==EC26Action::Bowling&&Bowler->ActionTime>.5f&&Bowler->ActionTime<.6f)Shoot(0,TEXT("follow"));
    }

    if(Phase==EC26Phase::Ready)
    {
        if(Prev==EC26Phase::Reaction||Prev==EC26Phase::Replay||Prev==EC26Phase::Presentation||Prev==EC26Phase::Interval)
        {
            if(ReactStage==0&&!ReactShots.Contains(TEXT("bowler_recovered"))&&DeliveryId<4){UE_LOG(LogC26,Display,TEXT("C26_REACT_NOTE bowler settle not observed; another delivery"));return;}
            if(ReactStage==0&&!ReactShots.Contains(TEXT("bowler_recovered")))Check(false,TEXT("delivery completion settle observed"));
            if(++ReactStage>=Cases().Num()){Finish();return;}
            AutoPlay=false;ReactPrevBones.Reset();ReactPrevState.Reset();
            UE_LOG(LogC26,Display,TEXT("C26_REACT_STAGE_BEGIN %d %s"),ReactStage,Cases()[ReactStage].Name);
            return;
        }
        if(ReactStage==0)return;
        // Standing between balls: a fielder is alive (breathing weight up) without any clip change.
        if(PhaseTime>1.4f&&!ReactShots.Contains(FString::Printf(TEXT("%d_idle"),ReactStage)))
        {
            ReactShots.Add(FString::Printf(TEXT("%d_idle"),ReactStage));
            if(auto* Anim=AnimOf(Athletes[5]))
                Check(Anim->Life.Breath>.3f&&Athletes[5]->Presentation->CurrentState==TEXT("FielderReady"),
                    FString::Printf(TEXT("idle fielder breathing weight %.2f look %.1f state %s"),Anim->Life.Breath,Anim->Life.LookYaw,*Athletes[5]->Presentation->CurrentState.ToString()));
        }
        if(PhaseTime<1.6f)return;
        // Commit the outcome through the real Resolve, exactly like DebugOutcome.
        const FString Name=Case.Name;
        if(Name==TEXT("four")){Pending.Rope=C26::Boundary::Four;Pending.BatRuns=4;}
        else if(Name==TEXT("six")){Pending.Rope=C26::Boundary::Six;Pending.BatRuns=6;}
        else if(Name==TEXT("bowled"))Pending.Wicket=C26::Dismissal::Bowled;
        else if(Name==TEXT("caught")){Pending.Wicket=C26::Dismissal::Caught;ActiveFielder=6;}
        else if(Name==TEXT("playandmiss")){ShotQueued=true;LastContact.Timing=EC26Timing::Miss;KeeperTake=true;Simulation.ContactPosition.X=10;}
        else if(Name==TEXT("edge")){ShotQueued=true;LastContact.Timing=EC26Timing::Edge;Simulation.ContactPosition.X=40;}
        else if(Name==TEXT("dropped")){ShotQueued=true;LastContact.Timing=EC26Timing::Good;DroppedBy=4;Pending.BatRuns=1;Pending.CompletedRuns=1;}
        else if(Name==TEXT("dot")){ShotQueued=true;LastContact.Timing=EC26Timing::Good;Simulation.ContactPosition.X=60;}
        UE_LOG(LogC26,Display,TEXT("C26_REACT_COMMIT %s"),*Name);
        ResettingMatch=true;Resolve();ResettingMatch=false;
        return;
    }
    if(Phase!=EC26Phase::Reaction)return;

    // Continuity: no bone jumps in the actor frame, at clip switches included.
    TArray<int32> Tracked={11,0,1,12,Case.Subject};
    for(const auto& Cue:Case.Cues)Tracked.AddUnique(Cue.Key);
    for(int32 I:Tracked)
    {
        const AC26Athlete* A=Athletes.IsValidIndex(I)?Athletes[I].Get():nullptr;
        if(!A||!AnimOf(A))continue;
        const USkeletalMeshComponent* Body=A->Presentation->Body;
        const FTransform Root=A->GetActorTransform();
        TArray<FVector> Bones;
        for(const TCHAR* Bone:{TEXT("head"),TEXT("hand_r"),TEXT("hand_l"),TEXT("foot_l")})Bones.Add(Root.InverseTransformPosition(Body->GetBoneLocation(Bone)));
        const FName State=A->Presentation->CurrentState;
        if(const TArray<FVector>* Last=ReactPrevBones.Find(I);Last&&PhaseTime>.05f)
        {
            float Jump=0;for(int32 B=0;B<Bones.Num();++B)Jump=FMath::Max(Jump,FVector::Dist(Bones[B],(*Last)[B]));
            const bool Switched=ReactPrevState.FindRef(I)!=State;
            if(Jump>30.f)Check(false,FString::Printf(TEXT("athlete %d jumps %.1fcm in one frame (%s -> %s)"),I,Jump,*ReactPrevState.FindRef(I).ToString(),*State.ToString()));
            if(Switched)UE_LOG(LogC26,Display,TEXT("C26_REACT_SWITCH stage=%d athlete=%d t=%.2f %s -> %s jump=%.2fcm clock=%.2f recovered=%d"),
                ReactStage,I,PhaseTime,*ReactPrevState.FindRef(I).ToString(),*State.ToString(),Jump,A->Presentation->GetReactionClock(),A->Presentation->IsRecovered()?1:0);
        }
        ReactPrevBones.Add(I,Bones);ReactPrevState.Add(I,State);
    }
    for(const TCHAR* Beat:{TEXT("a"),TEXT("b"),TEXT("c"),TEXT("d")})
    {
        const float At=Beat[0]==TEXT('a')?.35f:Beat[0]==TEXT('b')?.8f:Beat[0]==TEXT('c')?1.25f:2.0f;
        if(PhaseTime>=At)Shoot(Case.Subject,Beat);
    }
    // Right reaction on the right athlete, once every beat delay has elapsed.
    if(PhaseTime>=1.0f&&!ReactShots.Contains(FString::Printf(TEXT("%d_cues"),ReactStage)))
    {
        ReactShots.Add(FString::Printf(TEXT("%d_cues"),ReactStage));
        for(const auto& Cue:Case.Cues)
        {
            const AC26Athlete* A=Athletes[Cue.Key];
            const auto* P=A->Presentation.Get();
            const FName Want=C26Character::ReactionKey(Cue.Value,P->VisualRole,P->Appearance.AnimationStyle,P->Appearance.LeftHandedBat);
            const bool CueOk=A->Reaction==FName(Cue.Value);
            const bool ClipOk=Want.IsNone()||P->CurrentState==Want;
            Check(CueOk&&ClipOk,FString::Printf(TEXT("%s: athlete %d (%s) cue %s -> %s playing %s at clip %.2fs"),Case.Name,Cue.Key,
                *P->Appearance.AnimationStyle.ToString(),*A->Reaction.ToString(),*Want.ToString(),*P->CurrentState.ToString(),P->GetReactionClock()));
        }
    }
    // Last reaction frame: every played reaction is back to ready or in its authored recovery.
    const float Window=Important?2.4f:1.6f;
    if(PhaseTime+Dt>Window&&!ReactShots.Contains(FString::Printf(TEXT("%d_end"),ReactStage)))
    {
        ReactShots.Add(FString::Printf(TEXT("%d_end"),ReactStage));
        for(const auto& Cue:Case.Cues)
        {
            const auto* P=Athletes[Cue.Key]->Presentation.Get();
            const FName Want=C26Character::ReactionKey(Cue.Value,P->VisualRole,P->Appearance.AnimationStyle,P->Appearance.LeftHandedBat);
            const auto* Clip=P->Profile?P->Profile->FindClip(Want):nullptr;
            if(!Clip||!Clip->Sequence)continue;
            const float Progress=P->GetReactionClock()/Clip->Sequence->GetPlayLength();
            const bool Holds=C26Character::HoldsFinalPose(Want);
            Check(Holds?P->CurrentState==Want:(P->IsRecovered()||Progress>=.75f),FString::Printf(TEXT("%s: athlete %d %s at window end state=%s recovered=%d progress=%.2f"),
                Case.Name,Cue.Key,*Want.ToString(),*P->CurrentState.ToString(),P->IsRecovered()?1:0,Progress));
        }
    }
}
#endif
