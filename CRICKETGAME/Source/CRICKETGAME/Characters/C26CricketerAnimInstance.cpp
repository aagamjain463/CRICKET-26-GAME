#include "C26CricketerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BoneContainer.h"

namespace
{
/** Rotates a handful of trunk and head bones about their own pivots in mesh space. Legs, pelvis and hands'
    parents below the chest are never touched, so planted feet stay planted. */
struct FC26LifeNode : FAnimNode_Base
{
    enum EBone { Spine01, Spine02, Spine03, Spine04, Spine05, Neck, Head, Count };
    FPoseLink Source;
    FC26SecondaryMotion Life;
    FBoneReference Bones[Count];
    FC26LifeNode()
    {
        static const TCHAR* Names[Count]={TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05"),TEXT("neck_01"),TEXT("head")};
        for(int32 I=0;I<Count;++I)Bones[I].BoneName=Names[I];
    }
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override{Source.Initialize(Context);}
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
    {
        Source.CacheBones(Context);
        for(auto& Bone:Bones)Bone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
    }
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override{Source.Update(Context);}
    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        Source.Evaluate(Output);
        if(Life.IsZero())return;
        // The source mesh faces +Y (the profile's MeshToGameplayRotation turns it to gameplay +X), so in mesh
        // space forward is +Y, the athlete's right is -X and up is +Z. Rotations are built as "tilt this
        // axis towards that one", which leaves no handedness convention to get wrong.
        const FVector Up(0,0,1),Fwd(0,1,0),Right(-1,0,0);
        const auto Tilt=[](const FVector& From,const FVector& To,float Degrees)
        {
            const float R=FMath::DegreesToRadians(Degrees);
            return FQuat::FindBetweenNormals(From,(From*FMath::Cos(R)+To*FMath::Sin(R)).GetSafeNormal());
        };
        const float Breath=FMath::Sin(Life.BreathPhase)*Life.BreathDegrees*Life.Breath;
        const float Sway=FMath::Sin(Life.SwayPhase)*Life.SwayDegrees*Life.Sway;
        // Chest opens back on the inhale; the neck takes most of it back out so the helmet and eyes stay level.
        const float Pitch[Count]={0,Life.LeanPitch*.5f,-Breath*.6f,Life.LeanPitch*.5f,-Breath*.4f,Breath*.7f-Life.LeanPitch*.35f+Life.LookPitch*-.3f,-Life.LeanPitch*.25f-Life.LookPitch*.7f};
        const float Roll[Count]={Sway,Life.LeanRoll*.5f,-Sway*.5f,Life.LeanRoll*.5f,0,-Sway*.3f-Life.LeanRoll*.4f,-Sway*.2f-Life.LeanRoll*.3f};
        const float Yaw[Count]={0,0,0,0,0,Life.LookYaw*.4f,Life.LookYaw*.6f};
        TMap<int32,FQuat> Delta;int32 Last=-1;
        const FBoneContainer& Required=Output.Pose.GetBoneContainer();
        for(int32 I=0;I<Count;++I)
        {
            if(!Bones[I].IsValidToEvaluate(Required)||(Pitch[I]==0&&Roll[I]==0&&Yaw[I]==0))continue;
            const FCompactPoseBoneIndex Index=Bones[I].GetCompactPoseIndex(Required);
            Delta.Add(Index.GetInt(),Tilt(Up,Fwd,Pitch[I])*Tilt(Up,Right,Roll[I])*Tilt(Fwd,Right,Yaw[I]));
            Last=FMath::Max(Last,Index.GetInt());
        }
        if(Last<0)return;
        // Parents precede children in a compact pose, so one pass accumulates mesh-space rotations and a
        // mesh-space delta D on bone i becomes local' = inv(parent) * D * parent * local.
        TArray<FQuat,TInlineAllocator<128>> Mesh;Mesh.SetNum(Last+1);
        for(int32 I=0;I<=Last;++I)
        {
            const FCompactPoseBoneIndex Index(I);
            const FCompactPoseBoneIndex Parent=Output.Pose.GetParentBoneIndex(Index);
            const FQuat ParentRot=Parent.GetInt()>=0?Mesh[Parent.GetInt()]:FQuat::Identity;
            FQuat Local=Output.Pose[Index].GetRotation();
            if(const FQuat* D=Delta.Find(I))
            {
                Local=(ParentRot.Inverse()*(*D)*ParentRot*Local).GetNormalized();
                Output.Pose[Index].SetRotation(Local);
            }
            Mesh[I]=ParentRot*Local;
        }
    }
    virtual void GatherDebugData(FNodeDebugData& DebugData) override{Source.GatherDebugData(DebugData);}
};

struct FC26CricketerProxy : FAnimInstanceProxy
{
    FAnimNode_SequenceEvaluator_Standalone Previous, Current;
    FAnimNode_TwoWayBlend Blend;
    FC26LifeNode Root;
    explicit FC26CricketerProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
    {
        Blend.A.SetLinkNode(&Previous);Blend.B.SetLinkNode(&Current);Root.Source.SetLinkNode(&Blend);
        // Deterministic evaluation: action notifies are markers consumed by the match clock.
        // Extracting root motion or dispatching notify gameplay a second time is forbidden.
        Previous.SetTeleportToExplicitTime(true);Current.SetTeleportToExplicitTime(true);
    }
    virtual FAnimNode_Base* GetCustomRootNode() override {return &Root;}
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
    {Nodes.Add(&Root);Nodes.Add(&Blend);Nodes.Add(&Previous);Nodes.Add(&Current);}
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        const auto* Anim=CastChecked<UC26CricketerAnimInstance>(Instance);
        Previous.SetSequence(Anim->PreviousSequence?Anim->PreviousSequence:Anim->CurrentSequence);
        Previous.SetExplicitTime(Anim->PreviousTime);
        Current.SetSequence(Anim->CurrentSequence);Current.SetExplicitTime(Anim->CurrentTime);
        Blend.Alpha=Anim->BlendAlpha;Root.Life=Anim->Life;
    }
};
}
FAnimInstanceProxy* UC26CricketerAnimInstance::CreateAnimInstanceProxy(){return new FC26CricketerProxy(this);}
void UC26CricketerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
