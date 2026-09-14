#include "C26CricketerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Animation/BoneReference.h"

struct FC26CricketerProxy : FAnimInstanceProxy
{
    FAnimNode_SequenceEvaluator_Standalone Previous, Current;
    FAnimNode_TwoWayBlend Root;

    FBoneReference PelvisBone;
    FBoneReference LeftThighBone;
    FBoneReference LeftCalfBone;
    FBoneReference LeftFootBone;
    FBoneReference RightThighBone;
    FBoneReference RightCalfBone;
    FBoneReference RightFootBone;
    /** Bone indices are per-LOD. SetQualityForView re-forces the LOD as athletes move, so the
        references are re-resolved whenever the required-bone set changes rather than once. */
    uint16 BoneContainerSerial = 0;
    bool bBonesInitialized = false;

    float FootIKWeight = 0.f;
    float PelvisOffsetZ = 0.f;
    float LeftFootLockAlpha = 0.f;
    FVector LeftFootTargetCS = FVector::ZeroVector;
    float RightFootLockAlpha = 0.f;
    FVector RightFootTargetCS = FVector::ZeroVector;
    FVector AnimatedLeftFootCS = FVector::ZeroVector;
    FVector AnimatedRightFootCS = FVector::ZeroVector;
    bool bAnimatedFeetValid = false;

    explicit FC26CricketerProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Root.A.SetLinkNode(&Previous);
        Root.B.SetLinkNode(&Current);
        Previous.SetTeleportToExplicitTime(true);
        Current.SetTeleportToExplicitTime(true);

        PelvisBone.BoneName = TEXT("pelvis");
        LeftThighBone.BoneName = TEXT("thigh_l");
        LeftCalfBone.BoneName = TEXT("calf_l");
        LeftFootBone.BoneName = TEXT("foot_l");
        RightThighBone.BoneName = TEXT("thigh_r");
        RightCalfBone.BoneName = TEXT("calf_r");
        RightFootBone.BoneName = TEXT("foot_r");
    }

    virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
    {
        Nodes.Add(&Root);
        Nodes.Add(&Previous);
        Nodes.Add(&Current);
    }

    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
        auto* Anim = Cast<UC26CricketerAnimInstance>(InAnimInstance);
        if (!Anim || !Anim->CurrentSequence) return;

        Previous.SetSequence(Anim->PreviousSequence ? Anim->PreviousSequence : Anim->CurrentSequence);
        Previous.SetExplicitTime(Anim->PreviousTime);
        Current.SetSequence(Anim->CurrentSequence);
        Current.SetExplicitTime(Anim->CurrentTime);
        Root.Alpha = Anim->BlendAlpha;

        FootIKWeight = Anim->FootIKWeight;
        PelvisOffsetZ = Anim->PelvisOffsetZ;
        LeftFootLockAlpha = Anim->LeftFootLockAlpha;
        LeftFootTargetCS = Anim->LeftFootTargetCS;
        RightFootLockAlpha = Anim->RightFootLockAlpha;
        RightFootTargetCS = Anim->RightFootTargetCS;
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        Root.Evaluate_AnyThread(Output);

        // Nothing to stabilize: the rendered skeleton already is the animated pose, so the
        // component can read the ankles off the mesh and no readback is needed.
        bAnimatedFeetValid = false;
        if (FootIKWeight <= 0.001f || (LeftFootLockAlpha <= 0.001f && RightFootLockAlpha <= 0.001f && FMath::Abs(PelvisOffsetZ) <= 0.01f))
        {
            return true;
        }

        const FBoneContainer& RequiredBones = Output.Pose.GetBoneContainer();
        if (!bBonesInitialized || BoneContainerSerial != RequiredBones.GetSerialNumber())
        {
            BoneContainerSerial = RequiredBones.GetSerialNumber();
            PelvisBone.Initialize(RequiredBones);
            LeftThighBone.Initialize(RequiredBones);
            LeftCalfBone.Initialize(RequiredBones);
            LeftFootBone.Initialize(RequiredBones);
            RightThighBone.Initialize(RequiredBones);
            RightCalfBone.Initialize(RequiredBones);
            RightFootBone.Initialize(RequiredBones);
            bBonesInitialized = true;
        }

        if (!PelvisBone.IsValidToEvaluate() || !LeftFootBone.IsValidToEvaluate() || !RightFootBone.IsValidToEvaluate())
        {
            return true;
        }

        FCSPose<FCompactPose> CSPose;
        CSPose.InitPose(Output.Pose);

        // Capture the untouched authored ankles before anything is applied to them.
        AnimatedLeftFootCS = CSPose.GetComponentSpaceTransform(LeftFootBone.GetCompactPoseIndex(RequiredBones)).GetLocation();
        AnimatedRightFootCS = CSPose.GetComponentSpaceTransform(RightFootBone.GetCompactPoseIndex(RequiredBones)).GetLocation();
        bAnimatedFeetValid = true;

        // Pelvis vertical compensation
        if (FMath::Abs(PelvisOffsetZ) > 0.01f)
        {
            const FCompactPoseBoneIndex PelvisIndex = PelvisBone.GetCompactPoseIndex(RequiredBones);
            FTransform PelvisCS = CSPose.GetComponentSpaceTransform(PelvisIndex);
            PelvisCS.AddToTranslation(FVector(0.f, 0.f, PelvisOffsetZ * FootIKWeight));
            TArray<FBoneTransform, TInlineAllocator<4>> PelvisBoneTransforms;
            PelvisBoneTransforms.Add(FBoneTransform(PelvisIndex, PelvisCS));
            CSPose.SafeSetCSBoneTransforms(PelvisBoneTransforms);
        }

        // Left leg TwoBoneIK
        if (LeftFootLockAlpha > 0.001f && LeftThighBone.IsValidToEvaluate() && LeftCalfBone.IsValidToEvaluate())
        {
            const FCompactPoseBoneIndex ThighIndex = LeftThighBone.GetCompactPoseIndex(RequiredBones);
            const FCompactPoseBoneIndex CalfIndex = LeftCalfBone.GetCompactPoseIndex(RequiredBones);
            const FCompactPoseBoneIndex FootIndex = LeftFootBone.GetCompactPoseIndex(RequiredBones);

            FTransform ThighCS = CSPose.GetComponentSpaceTransform(ThighIndex);
            FTransform CalfCS = CSPose.GetComponentSpaceTransform(CalfIndex);
            FTransform FootCS = CSPose.GetComponentSpaceTransform(FootIndex);

            FVector KneeDir = CalfCS.GetLocation() - (ThighCS.GetLocation() + (LeftFootTargetCS - ThighCS.GetLocation()) * 0.5f);
            if (KneeDir.IsNearlyZero())
            {
                KneeDir = ThighCS.GetRotation().GetForwardVector();
            }
            FVector JointTarget = CalfCS.GetLocation() + KneeDir.GetSafeNormal() * 50.f;

            AnimationCore::SolveTwoBoneIK(ThighCS, CalfCS, FootCS, JointTarget, LeftFootTargetCS, false, 1.0, 1.0);

            TArray<FBoneTransform, TInlineAllocator<4>> LeftLegTransforms;
            LeftLegTransforms.Add(FBoneTransform(ThighIndex, ThighCS));
            LeftLegTransforms.Add(FBoneTransform(CalfIndex, CalfCS));
            LeftLegTransforms.Add(FBoneTransform(FootIndex, FootCS));
            CSPose.LocalBlendCSBoneTransforms(LeftLegTransforms, LeftFootLockAlpha * FootIKWeight);
        }

        // Right leg TwoBoneIK
        if (RightFootLockAlpha > 0.001f && RightThighBone.IsValidToEvaluate() && RightCalfBone.IsValidToEvaluate())
        {
            const FCompactPoseBoneIndex ThighIndex = RightThighBone.GetCompactPoseIndex(RequiredBones);
            const FCompactPoseBoneIndex CalfIndex = RightCalfBone.GetCompactPoseIndex(RequiredBones);
            const FCompactPoseBoneIndex FootIndex = RightFootBone.GetCompactPoseIndex(RequiredBones);

            FTransform ThighCS = CSPose.GetComponentSpaceTransform(ThighIndex);
            FTransform CalfCS = CSPose.GetComponentSpaceTransform(CalfIndex);
            FTransform FootCS = CSPose.GetComponentSpaceTransform(FootIndex);

            FVector KneeDir = CalfCS.GetLocation() - (ThighCS.GetLocation() + (RightFootTargetCS - ThighCS.GetLocation()) * 0.5f);
            if (KneeDir.IsNearlyZero())
            {
                KneeDir = ThighCS.GetRotation().GetForwardVector();
            }
            FVector JointTarget = CalfCS.GetLocation() + KneeDir.GetSafeNormal() * 50.f;

            AnimationCore::SolveTwoBoneIK(ThighCS, CalfCS, FootCS, JointTarget, RightFootTargetCS, false, 1.0, 1.0);

            TArray<FBoneTransform, TInlineAllocator<4>> RightLegTransforms;
            RightLegTransforms.Add(FBoneTransform(ThighIndex, ThighCS));
            RightLegTransforms.Add(FBoneTransform(CalfIndex, CalfCS));
            RightLegTransforms.Add(FBoneTransform(FootIndex, FootCS));
            CSPose.LocalBlendCSBoneTransforms(RightLegTransforms, RightFootLockAlpha * FootIKWeight);
        }

        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSPose), Output.Pose);
        return true;
    }

    virtual void PostEvaluate(UAnimInstance* InAnimInstance) override
    {
        FAnimInstanceProxy::PostEvaluate(InAnimInstance);
        if (auto* Anim = Cast<UC26CricketerAnimInstance>(InAnimInstance))
        {
            Anim->AnimatedLeftFootCS = AnimatedLeftFootCS;
            Anim->AnimatedRightFootCS = AnimatedRightFootCS;
            Anim->bAnimatedFeetValid = bAnimatedFeetValid;
        }
    }
};

FAnimInstanceProxy* UC26CricketerAnimInstance::CreateAnimInstanceProxy()
{
    return new FC26CricketerProxy(this);
}

void UC26CricketerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete InProxy;
}
