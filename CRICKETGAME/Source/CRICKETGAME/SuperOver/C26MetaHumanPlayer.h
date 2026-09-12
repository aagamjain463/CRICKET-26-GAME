#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26MetaHumanPlayer.generated.h"

class USkeletalMeshComponent;

/** MH_C26_Player_001 in the level: a real MetaHuman-founded cricket athlete, entirely separate
    from AC26Athlete (the working static-mesh/procedural-mesh gameplay actor used by the live
    match simulation). This class exists so the MetaHuman pipeline can be validated -- scale,
    stance, camera framing, stadium lighting -- without touching anything BuildMatchActors()
    spawns or the rules/scoring/replay systems that depend on it. It is not wired into
    AC26MatchGameMode and does not affect a running match.

    The body mesh is a soft reference resolved at BeginPlay so this class compiles and the actor
    can be placed in the level before the MetaHuman asset has been built (the plugin's
    auto-rigging step needs a one-time interactive Epic account sign-in in the editor; see
    Docs/CURRENT_TASK.md). Once Tools/MetaHumanBuildPlayer001.py has produced a real skeletal
    mesh at the path below, this actor picks it up automatically -- no code change required. */
UCLASS()
class CRICKETGAME_API AC26MetaHumanPlayer : public AActor
{
    GENERATED_BODY()
public:
    AC26MetaHumanPlayer();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="C26|MetaHuman")
    TObjectPtr<USkeletalMeshComponent> Body;

    /** Package path build_meta_human writes to (Tools/MetaHumanBuildPlayer001.py absolute_build_path).
        Kept as a soft path, not a hard reference, so the project still opens/compiles cleanly
        before the asset exists. */
    static const TCHAR* BodyMeshPath();

    /** Standard bone names on the MetaHuman default skeleton (root_ctrl_bone hierarchy under
        pelvis -> spine_0.. -> ... -> hand_r/hand_l, head, calf_r/calf_l). These are fixed by the
        MetaHuman skeleton the plugin ships, independent of any one character's face/body sculpt,
        so equipment attachment can be wired now even though this character's own skeleton has
        not been built yet. */
    struct FSocketBones
    {
        static const TCHAR* RightHand;
        static const TCHAR* LeftHand;
        static const TCHAR* Head;
        static const TCHAR* RightCalf;
        static const TCHAR* LeftCalf;
    };

    /** World transform of a named equipment attachment bone, or identity + false if the body
        mesh isn't loaded/skinned yet. Equipment actors (C26_Bat_01, C26_Helmet_01,
        C26_BattingGlove_L/R, C26_BattingPad_L/R) attach here rather than being welded into the
        body mesh, so a future two-handed bat can be re-parented between hands without a new
        skinned asset. */
    UFUNCTION(BlueprintCallable, Category="C26|MetaHuman|Equipment")
    bool GetEquipmentSocketTransform(FName BoneName, FTransform& OutTransform) const;
};
