#include "C26CharacterReviewMode.h"
#include "C26CricketerAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "C26CharacterProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"

AC26CharacterReviewMode::AC26CharacterReviewMode()
{DefaultPawnClass=nullptr;PrimaryActorTick.bCanEverTick=true;}
void AC26CharacterReviewMode::Tick(float Dt)
{
    Super::Tick(Dt);
#if !UE_BUILD_SHIPPING
    Age+=Dt;
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(TActorIterator<ACameraActor> It(GetWorld());It&&PC)PC->SetViewTarget(*It);
    if(FParse::Param(FCommandLine::Get(),TEXT("C26ShotReview")))
    {if(TActorIterator<ASkeletalMeshActor> It(GetWorld());It)TickShotReview(*It);return;}
    if(!FParse::Param(FCommandLine::Get(),TEXT("C26ReviewCapture")))return;
    if(TActorIterator<ASkeletalMeshActor> It(GetWorld());It)
    {
        auto* Body=It->GetSkeletalMeshComponent();
        FString BodyPath;
        if(FParse::Value(FCommandLine::Get(),TEXT("C26ReviewBody="),BodyPath))
            if(auto* Model=LoadObject<USkeletalMesh>(nullptr,*BodyPath))
                if(Body->GetSkeletalMeshAsset()!=Model)Body->SetSkeletalMesh(Model);
        FString ClipPath=TEXT("/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run.C26_A_Run");
        FParse::Value(FCommandLine::Get(),TEXT("C26ReviewAnimation="),ClipPath);
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*ClipPath);
        int32 Lod=0;FParse::Value(FCommandLine::Get(),TEXT("C26ReviewLOD="),Lod);Body->SetForcedLOD(Lod+1);Body->UpdateLODStatus();
        if(!Clip)return;
        if(Body->GetAnimClass()!=UC26CricketerAnimInstance::StaticClass())
        {Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->SetComponentTickEnabled(false);}
        auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());if(!Anim)return;
        const int32 Next=FMath::Clamp(int32(Age)-4,0,5);
        Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;
        Anim->CurrentTime=Clip->GetPlayLength()*Next/6.f;
        Body->TickAnimation(Dt,false);Body->RefreshBoneTransforms();
        if(Age>4.5f+Next&&Frame!=Next)
        {
            Frame=Next;const FString Dir=FPaths::ProjectDir()/TEXT("Artifacts/CharacterAudit/RunReview");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_lod%d_%d.png"),*Clip->GetName(),Lod,Next),false,false);
            UE_LOG(LogTemp,Display,TEXT("C26_RUN_REVIEW time=%.3f left=%s right=%s head=%s"),Anim->CurrentTime,
                *Body->GetBoneLocation(TEXT("foot_l")).ToString(),*Body->GetBoneLocation(TEXT("foot_r")).ToString(),*Body->GetBoneLocation(TEXT("head")).ToString());
        }
        if(Age>11.f)FPlatformMisc::RequestExit(false);
    }
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
float SegmentDistance(const FVector& A0,const FVector& A1,const FVector& B0,const FVector& B1)
{
    FVector PA,PB;FMath::SegmentDistToSegmentSafe(A0,A1,B0,B1,PA,PB);return FVector::Dist(PA,PB);
}
}
void AC26CharacterReviewMode::TickShotReview(ASkeletalMeshActor* Actor)
{
    // Measures what the game renders: the batter profile's body, skeleton, sockets and equipment
    // offsets, driven through the same UC26CricketerAnimInstance the match uses.
    auto* Body=Actor->GetSkeletalMeshComponent();
    auto* Profile=LoadObject<UC26CharacterProfile>(nullptr,TEXT("/Game/Cricket26/Characters/Data/DA_C26_BatterReview.DA_C26_BatterReview"));
    if(!Profile||!Profile->Body){UE_LOG(LogTemp,Error,TEXT("C26_SHOT_REVIEW_FAIL no batter profile"));FPlatformMisc::RequestExit(false);return;}
    const FC26EquipmentDefinition* BatItem=nullptr;
    if(ShotStep<0)
    {
        if(Body->GetSkeletalMeshAsset()!=Profile->Body)Body->SetSkeletalMesh(Profile->Body);
        Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->SetComponentTickEnabled(false);
        for(const auto& Item:Profile->Equipment)
        {
            auto* Part=NewObject<UStaticMeshComponent>(Actor);Part->SetStaticMesh(Item.Mesh);
            Part->SetupAttachment(Body,Item.ResolveSocket(false));Part->SetRelativeTransform(Item.ResolveOffset(false));
            Part->RegisterComponent();ReviewGear.Add(Part);
        }
        ShotStep=0;
    }
    for(const auto& Item:Profile->Equipment)if(Item.Slot==EC26EquipmentSlot::Bat)BatItem=&Item;
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    if(!Anim||!BatItem)return;
    const TArray<FString>& Shots=C26Character::ShotClips();
    const int32 PerShot=10; // 5 poses x 2 cameras
    const int32 Shot=ShotStep/PerShot;
    if(Shot>=Shots.Num())
    {
        const FString Path=FPaths::ProjectDir()/TEXT("Artifacts/CharacterAudit/ShotReview/report.txt");
        FFileHelper::SaveStringToFile(ShotReport,*Path);
        UE_LOG(LogTemp,Display,TEXT("C26_SHOT_REVIEW_DONE shots=%d"),Shots.Num());FPlatformMisc::RequestExit(false);return;
    }
    const FC26CricketClip* Clip=Profile->FindClip(FName(*(Shots[Shot]+TEXT("_R"))));
    if(!Clip||!Clip->Sequence){UE_LOG(LogTemp,Error,TEXT("C26_SHOT_REVIEW_FAIL missing %s_R"),*Shots[Shot]);ShotStep=(Shot+1)*PerShot;return;}
    const float Length=Clip->Sequence->GetPlayLength(),Contact=Clip->EventTime();
    auto Pose=[&](float T)
    {
        Anim->PreviousSequence=Anim->CurrentSequence=Clip->Sequence;Anim->BlendAlpha=1;Anim->CurrentTime=T;
        Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
        for(UStaticMeshComponent* Part:ReviewGear)Part->UpdateComponentToWorld();
    };
    if(Shot==0&&MeasuredShot<0&&Age>=1.f)
    {
        // Stance in component space, so equipment offsets can be solved against the exact frame the
        // clips were authored in (Tools/FitBatterSockets.py).
        Pose(0.f);
        const FTransform ToComponent=Body->GetComponentTransform().Inverse();
        FString Json=TEXT("{");
        const TCHAR* Names[]={TEXT("pelvis"),TEXT("spine_05"),TEXT("neck_01"),TEXT("head"),TEXT("clavicle_l"),TEXT("clavicle_r"),
            TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r"),TEXT("thigh_l"),
            TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),TEXT("ball_l"),TEXT("ball_r")};
        for(int32 I=0;I<UE_ARRAY_COUNT(Names);++I)
        {
            const FTransform T=Body->GetSocketTransform(Names[I],RTS_Component);
            const FVector X=T.GetUnitAxis(EAxis::X),Y=T.GetUnitAxis(EAxis::Y),Z=T.GetUnitAxis(EAxis::Z),L=T.GetLocation();
            Json+=FString::Printf(TEXT("%s\"%s\":{\"loc\":[%f,%f,%f],\"x\":[%f,%f,%f],\"y\":[%f,%f,%f],\"z\":[%f,%f,%f]}"),I?TEXT(","):TEXT(""),
                Names[I],L.X,L.Y,L.Z,X.X,X.Y,X.Z,Y.X,Y.Y,Y.Z,Z.X,Z.Y,Z.Z);
        }
        FFileHelper::SaveStringToFile(Json+TEXT("}"),*(FPaths::ProjectDir()/TEXT("Artifacts/CharacterAudit/ue-stance-bones.json")));
        UE_LOG(LogTemp,Display,TEXT("C26_STANCE_BONES written"));
        if(FParse::Param(FCommandLine::Get(),TEXT("C26StanceOnly"))){FPlatformMisc::RequestExit(false);return;}
    }
    if(MeasuredShot!=Shot&&Age>=1.f)
    {
        // Every frame: right-handed grip, bat vs torso/head, hands on the handle.
        MeasuredShot=Shot;int32 TopHandFails=0,TorsoFails=0,HeadFails=0,OffHandle=0;float MinTorso=1e9,MinHead=1e9,MaxOffAxis=0;FString HeadFrames;
        for(float T=0;T<=Length+1e-3f;T+=1.f/30.f)
        {
            Pose(T);
            const FTransform Bat=BatItem->ResolveOffset(false)*Body->GetSocketTransform(BatItem->ResolveSocket(false));
            const FVector Origin=Bat.GetLocation(),Axis=Bat.GetUnitAxis(EAxis::Z);
            const FVector HL=Body->GetBoneLocation(TEXT("hand_l")),HR=Body->GetBoneLocation(TEXT("hand_r"));
            if(FVector::DotProduct(HL-Origin,Axis)-FVector::DotProduct(HR-Origin,Axis)<3.f)++TopHandFails;
            for(const FVector& H:{HL,HR})
            {
                const FVector Rel=H-Origin;const float Off=(Rel-Axis*FVector::DotProduct(Rel,Axis)).Size();
                MaxOffAxis=FMath::Max(MaxOffAxis,Off);if(Off>10.f)++OffHandle;
            }
            const float Torso=SegmentDistance(Origin-Axis*28.f,Origin-Axis*83.f,Body->GetBoneLocation(TEXT("pelvis")),Body->GetBoneLocation(TEXT("neck_01")));
            const FVector Skull=Body->GetBoneLocation(TEXT("head"))+FVector(0,0,9);
            const float Head=SegmentDistance(Origin,Origin-Axis*83.f,Skull,Skull);
            MinTorso=FMath::Min(MinTorso,Torso);MinHead=FMath::Min(MinHead,Head);
            if(Torso<12.5f)++TorsoFails;if(Head<10.5f){++HeadFails;{const FTransform C=Body->GetComponentTransform();const FVector O=C.InverseTransformPosition(Origin),A=C.InverseTransformVector(Axis),K=C.InverseTransformPosition(Skull);
HeadFrames+=FString::Printf(TEXT(" f%d:%.1f[o=%.1f,%.1f,%.1f a=%.2f,%.2f,%.2f skull=%.1f,%.1f,%.1f]"),FMath::RoundToInt(T*30.f),Head,O.X,O.Y,O.Z,A.X,A.Y,A.Z,K.X,K.Y,K.Z);}}
        }
        const bool Pass=TopHandFails==0&&TorsoFails==0&&HeadFails==0&&OffHandle==0;
        const FString Line=FString::Printf(TEXT("C26_SHOT_REVIEW %s_R %s top_hand_fail=%d torso_fail=%d head_fail=%d off_handle=%d min_torso=%.1f min_head=%.1f max_hand_off_axis=%.1f contact=%.3f length=%.3f head_frames=%s"),
            *Shots[Shot],Pass?TEXT("PASS"):TEXT("FAIL"),TopHandFails,TorsoFails,HeadFails,OffHandle,MinTorso,MinHead,MaxOffAxis,Contact,Length,*HeadFrames);
        UE_LOG(LogTemp,Display,TEXT("%s"),*Line);ShotReport+=Line+TEXT("\n");
    }
    // Captures: stance, late downswing, contact, extension, finish - from point and from the bowler's 3/4.
    const int32 Local=ShotStep%PerShot;
    const float Times[]={0.f,Contact-.2f,Contact,Contact+.2f,Contact+.45f};
    Pose(FMath::Clamp(Times[Local/2],0.f,Length));
    Pose(0.f);
    const FVector Pelvis=Body->GetBoneLocation(TEXT("pelvis"));
    FVector Pitch=Body->GetBoneLocation(TEXT("foot_l"))-Body->GetBoneLocation(TEXT("foot_r"));Pitch.Z=0;Pitch.Normalize();
    FVector Off=Body->GetBoneLocation(TEXT("hand_l"))-Pelvis;Off.Z=0;Off=(Off-Pitch*FVector::DotProduct(Off,Pitch)).GetSafeNormal();
    Pose(FMath::Clamp(Times[Local/2],0.f,Length));
    const FVector Focus=FVector(Pelvis.X,Pelvis.Y,Actor->GetActorLocation().Z+95.f);
    const FVector Eye=Focus+((Local%2==0)?Off*430.f:(Pitch*.62f+Off*.78f).GetSafeNormal()*450.f)+FVector(0,0,15);
    if(TActorIterator<ACameraActor> Cam(GetWorld());Cam){Cam->SetActorLocation(Eye);Cam->SetActorRotation((Focus-Eye).Rotation());}
    Frame=(Frame+1)%3;
    if(Age<1.f||Frame!=2)return;
    const FString Dir=FPaths::ProjectDir()/TEXT("Artifacts/CharacterAudit/ShotReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_R_%d_%s.png"),*Shots[Shot],Local/2,Local%2==0?TEXT("point"):TEXT("bowler34")),false,false);
    ++ShotStep;
}
#endif
