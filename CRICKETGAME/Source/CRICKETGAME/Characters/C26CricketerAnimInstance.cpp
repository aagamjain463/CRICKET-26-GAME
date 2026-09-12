#include "C26CricketerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"

namespace
{
struct FC26CricketerProxy : FAnimInstanceProxy
{
    FAnimNode_SequenceEvaluator_Standalone Previous, Current;
    FAnimNode_TwoWayBlend Root;
    explicit FC26CricketerProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
    {
        Root.A.SetLinkNode(&Previous);Root.B.SetLinkNode(&Current);
        // Deterministic evaluation: action notifies are markers consumed by the match clock.
        // Extracting root motion or dispatching notify gameplay a second time is forbidden.
        Previous.SetTeleportToExplicitTime(true);Current.SetTeleportToExplicitTime(true);
    }
    virtual FAnimNode_Base* GetCustomRootNode() override {return &Root;}
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
    {Nodes.Add(&Root);Nodes.Add(&Previous);Nodes.Add(&Current);}
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        const auto* Anim=CastChecked<UC26CricketerAnimInstance>(Instance);
        Previous.SetSequence(Anim->PreviousSequence?Anim->PreviousSequence:Anim->CurrentSequence);
        Previous.SetExplicitTime(Anim->PreviousTime);
        Current.SetSequence(Anim->CurrentSequence);Current.SetExplicitTime(Anim->CurrentTime);
        Root.Alpha=Anim->BlendAlpha;
    }
};
}
FAnimInstanceProxy* UC26CricketerAnimInstance::CreateAnimInstanceProxy(){return new FC26CricketerProxy(this);}
void UC26CricketerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
