#include "C26MetaHumanPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/SoftObjectPath.h"

const TCHAR* AC26MetaHumanPlayer::BodyMeshPath()
{
    // Matches build_params.absolute_build_path in Tools/MetaHumanBuildPlayer001.py. The
    // MetaHuman build pipeline names the assembled body skeletal mesh after the character asset.
    return TEXT("/Game/Cricket26/Characters/MetaHumans/Players/Player_001/Body/SKM_MH_C26_Player_001.SKM_MH_C26_Player_001");
}

const TCHAR* AC26MetaHumanPlayer::FSocketBones::RightHand = TEXT("hand_r");
const TCHAR* AC26MetaHumanPlayer::FSocketBones::LeftHand  = TEXT("hand_l");
const TCHAR* AC26MetaHumanPlayer::FSocketBones::Head      = TEXT("head");
const TCHAR* AC26MetaHumanPlayer::FSocketBones::RightCalf = TEXT("calf_r");
const TCHAR* AC26MetaHumanPlayer::FSocketBones::LeftCalf  = TEXT("calf_l");

AC26MetaHumanPlayer::AC26MetaHumanPlayer()
{
    PrimaryActorTick.bCanEverTick = false;
    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AC26MetaHumanPlayer::BeginPlay()
{
    Super::BeginPlay();
    if (!Body->GetSkeletalMeshAsset())
    {
        const FSoftObjectPath Path(BodyMeshPath());
        if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Path.TryLoad()))
        {
            Body->SetSkeletalMesh(Mesh);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("C26_MH_PLAYER MH_C26_Player_001 body mesh not found at %s -- the MetaHuman "
                     "asset exists (Content/Cricket26/Characters/MetaHumans/Players/Player_001) "
                     "but has not been auto-rigged/assembled yet. Sign into an Epic account once "
                     "in the editor, then re-run Tools/MetaHumanBuildPlayer001.py."),
                BodyMeshPath());
        }
    }
}

bool AC26MetaHumanPlayer::GetEquipmentSocketTransform(FName BoneName, FTransform& OutTransform) const
{
    if (!Body || !Body->GetSkeletalMeshAsset())
    {
        OutTransform = FTransform::Identity;
        return false;
    }
    const int32 BoneIndex = Body->GetBoneIndex(BoneName);
    if (BoneIndex == INDEX_NONE)
    {
        OutTransform = FTransform::Identity;
        return false;
    }
    OutTransform = Body->GetBoneTransform(BoneIndex);
    return true;
}
