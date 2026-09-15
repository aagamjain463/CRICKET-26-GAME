#include "C26CricketerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "TwoBoneIK.h"

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

/** Bat control for whoever holds a bat. Authored strokes are exact on their keys only: between two 30 fps
    keys the arm chains interpolate in local space, and in a fast swing that moved the hands up to 40 cm off the
    handle and brought the blade inside the torso, although every key is clean. So the hands are driven from
    the keys: the top hand (which carries the bat) follows the component-space interpolation of the two keyed
    top hands, and the bottom hand keeps the keyed grip relative to it. The same grip holds through a blend
    from the previous clip. Reach then moves both hands by a component-space offset around one clip's contact
    frame. Only upperarm/lowerarm/hand of each side are written, so feet, pelvis, spine and head stay authored. */
struct FC26BatNode : FAnimNode_Base
{
    enum EBone { UpperL, LowerL, HandL, UpperR, LowerR, HandR, Count };
    FPoseLink Source;
    FC26BatControl Control;
    const UAnimSequence* Sequence=nullptr;
    float Time=0,BlendAlpha=1,ReachWeight=0;
    FBoneReference Bones[Count];
    FC26BatNode()
    {
        static const TCHAR* Names[Count]={TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")};
        for(int32 I=0;I<Count;++I)Bones[I].BoneName=Names[I];
    }
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override{Source.Initialize(Context);}
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
    {
        Source.CacheBones(Context);
        for(auto& Bone:Bones)Bone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
    }
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override{Source.Update(Context);}
    static void SolveArm(FCSPose<FCompactPose>& Pose,FCompactPoseBoneIndex U,FCompactPoseBoneIndex L,FCompactPoseBoneIndex H,const FTransform& Target)
    {
        FTransform UT=Pose.GetComponentSpaceTransform(U),LT=Pose.GetComponentSpaceTransform(L),HT=Pose.GetComponentSpaceTransform(H);
        // Keep the posed elbow plane; push the pole out from the shoulder-hand line so a nearly straight arm
        // still has a well-defined bend direction.
        const FVector Mid=(UT.GetLocation()+HT.GetLocation())*.5f;
        const FVector Pole=LT.GetLocation()+(LT.GetLocation()-Mid).GetSafeNormal()*20.f;
        AnimationCore::SolveTwoBoneIK(UT,LT,HT,Pole,Target.GetLocation(),false,1.,1.);
        HT.SetRotation(Target.GetRotation());
        TArray<FBoneTransform,TInlineAllocator<3>> Out={FBoneTransform(U,UT),FBoneTransform(L,LT),FBoneTransform(H,HT)};
        Pose.SafeSetCSBoneTransforms(Out);
    }
    virtual void Evaluate_AnyThread(FPoseContext& Output) override
    {
        Source.Evaluate(Output);
        if(!Control.bGripLock||!Sequence)return;
        const FBoneContainer& Required=Output.Pose.GetBoneContainer();
        for(auto& Bone:Bones)if(!Bone.IsValidToEvaluate(Required))return;
        const auto Index=[&](EBone B){return Bones[B].GetCompactPoseIndex(Required);};
        const bool LeftTop=Control.bLeftHandTop;
        const FCompactPoseBoneIndex TU=Index(LeftTop?UpperL:UpperR),TL=Index(LeftTop?LowerL:LowerR),TH=Index(LeftTop?HandL:HandR);
        const FCompactPoseBoneIndex BU=Index(LeftTop?UpperR:UpperL),BL=Index(LeftTop?LowerR:LowerL),BH=Index(LeftTop?HandR:HandL);
        // Keyed hands at the two keys around the current time.
        const auto KeyHands=[&](double At,FTransform& OutTop,FTransform& OutGrasp)
        {
            FPoseContext Key(Output);FAnimationPoseData Data(Key);
            Sequence->GetAnimationPose(Data,FAnimExtractContext(At,false));
            FCSPose<FCompactPose> KeyPose;KeyPose.InitPose(Key.Pose);
            OutTop=KeyPose.GetComponentSpaceTransform(TH);
            OutGrasp=KeyPose.GetComponentSpaceTransform(BH).GetRelativeTransform(OutTop);
        };
        const double Step=Sequence->GetSamplingFrameRate().AsInterval(),Length=Sequence->GetPlayLength();
        const double K0=FMath::Clamp(FMath::FloorToDouble(Time/Step+1e-4)*Step,0.,Length),K1=FMath::Min(K0+Step,Length);
        const float Frac=K1>K0?FMath::Clamp(float((Time-K0)/(K1-K0)),0.f,1.f):0.f;
        FTransform KeyTop,Grasp;KeyHands(K0,KeyTop,Grasp);
        if(Frac>1e-3f)
        {
            FTransform Top1,Grasp1;KeyHands(K1,Top1,Grasp1);
            FTransform Top0=KeyTop,Grasp0=Grasp;
            KeyTop.Blend(Top0,Top1,Frac);Grasp.Blend(Grasp0,Grasp1,Frac);
        }
        FCSPose<FCompactPose> Pose;Pose.InitPose(Output.Pose);
        // While the previous clip still contributes, the top hand eases from the blended pose onto the keys.
        FTransform Target;Target.Blend(Pose.GetComponentSpaceTransform(TH),KeyTop,BlendAlpha);
        if(ReachWeight>0)
        {
            // Reach only as far as BOTH hands can follow: the bat hangs off the top hand, so a reach the bottom
            // arm cannot make would tear the grip apart. Bisect the offset down until the bottom hand fits.
            const auto ArmLength=[&](FCompactPoseBoneIndex U,FCompactPoseBoneIndex L,FCompactPoseBoneIndex H)
            {return FVector::Dist(Pose.GetComponentSpaceTransform(U).GetLocation(),Pose.GetComponentSpaceTransform(L).GetLocation())
                   +FVector::Dist(Pose.GetComponentSpaceTransform(L).GetLocation(),Pose.GetComponentSpaceTransform(H).GetLocation());};
            const float TopLength=ArmLength(TU,TL,TH),BottomLength=ArmLength(BU,BL,BH)*.995f;
            const FVector TopShoulder=Pose.GetComponentSpaceTransform(TU).GetLocation(),BottomShoulder=Pose.GetComponentSpaceTransform(BU).GetLocation();
            const auto Fits=[&](float Scale)
            {
                FTransform Top=Target;Top.AddToTranslation(Control.Reach*ReachWeight*Scale);
                Top.SetLocation(TopShoulder+(Top.GetLocation()-TopShoulder).GetClampedToMaxSize(TopLength));
                return FVector::Dist((Grasp*Top).GetLocation(),BottomShoulder)<=BottomLength;
            };
            float Scale=1;
            if(!Fits(1))
            {
                float Low=0,High=1;
                for(int32 I=0;I<6;++I){const float Mid=(Low+High)*.5f;(Fits(Mid)?Low:High)=Mid;}
                Scale=Low;
            }
            Target.AddToTranslation(Control.Reach*ReachWeight*Scale);
        }
        SolveArm(Pose,TU,TL,TH,Target);
        SolveArm(Pose,BU,BL,BH,Grasp*Pose.GetComponentSpaceTransform(TH));
        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose),Output.Pose);
    }
    virtual void GatherDebugData(FNodeDebugData& DebugData) override{Source.GatherDebugData(DebugData);}
};

struct FC26CricketerProxy : FAnimInstanceProxy
{
    FAnimNode_SequenceEvaluator_Standalone Previous, Current;
    FAnimNode_TwoWayBlend Blend;
    FC26BatNode Bat;
    FC26LifeNode Root;
    explicit FC26CricketerProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
    {
        Blend.A.SetLinkNode(&Previous);Blend.B.SetLinkNode(&Current);
        Bat.Source.SetLinkNode(&Blend);Root.Source.SetLinkNode(&Bat);
        // Deterministic evaluation: action notifies are markers consumed by the match clock.
        // Extracting root motion or dispatching notify gameplay a second time is forbidden.
        Previous.SetTeleportToExplicitTime(true);Current.SetTeleportToExplicitTime(true);
    }
    virtual FAnimNode_Base* GetCustomRootNode() override {return &Root;}
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
    {Nodes.Add(&Root);Nodes.Add(&Bat);Nodes.Add(&Blend);Nodes.Add(&Previous);Nodes.Add(&Current);}
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        const auto* Anim=CastChecked<UC26CricketerAnimInstance>(Instance);
        Previous.SetSequence(Anim->PreviousSequence?Anim->PreviousSequence:Anim->CurrentSequence);
        Previous.SetExplicitTime(Anim->PreviousTime);
        Current.SetSequence(Anim->CurrentSequence);Current.SetExplicitTime(Anim->CurrentTime);
        Blend.Alpha=Anim->BlendAlpha;Root.Life=Anim->Life;
        const FC26BatControl& Control=Anim->BatControl;
        Bat.Control=Control;Bat.Sequence=Anim->CurrentSequence;Bat.Time=Anim->CurrentTime;Bat.BlendAlpha=Anim->BlendAlpha;Bat.ReachWeight=0;
        // Reach belongs to one clip's contact frame, so a replay of that clip re-applies it and nothing else can.
        if(Control.ReachSequence&&Control.ReachSequence==Anim->CurrentSequence)
            Bat.ReachWeight=(1.f-FMath::SmoothStep(0.f,.2f,FMath::Abs(Anim->CurrentTime-Control.ReachTime)))*Anim->BlendAlpha;
    }
};
}
FAnimInstanceProxy* UC26CricketerAnimInstance::CreateAnimInstanceProxy(){return new FC26CricketerProxy(this);}
void UC26CricketerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
