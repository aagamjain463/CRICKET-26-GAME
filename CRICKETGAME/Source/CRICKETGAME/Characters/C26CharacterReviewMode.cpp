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

AC26CharacterReviewMode::AC26CharacterReviewMode()
{DefaultPawnClass=nullptr;PrimaryActorTick.bCanEverTick=true;}
void AC26CharacterReviewMode::Tick(float Dt)
{
    Super::Tick(Dt);
#if !UE_BUILD_SHIPPING
    Age+=Dt;
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(TActorIterator<ACameraActor> It(GetWorld());It&&PC)PC->SetViewTarget(*It);
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
