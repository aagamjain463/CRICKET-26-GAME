#include "../C26MatchGameMode.h"
#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26CameraDirector.h"
#include "../C26Delivery.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"

void AC26MatchGameMode::UpdateProductionGate(float Dt)
{
    auto Check=[&](bool Ok,const TCHAR* What){if(!Ok)++GateFailures;UE_LOG(LogC26,Display,TEXT("C26_SUITE_CHECK %s stage=%d %s"),Ok?TEXT("PASS"):TEXT("FAIL"),GateStage,What);};
    auto Frame=[&](const TCHAR* Name)
    {
        const FString Key=FString::Printf(TEXT("%02d_%s"),GateStage,Name);
        if(GateShots.Contains(Key))return false;GateShots.Add(Key);
        if(!GateNoScreens)FScreenshotRequest::RequestScreenshot(GateDirectory/(Key+TEXT(".png")),true,false);
        return true;
    };
    if(FPlatformTime::Seconds()-GateStarted>750)
    {UE_LOG(LogC26,Error,TEXT("C26_SUITE_TIMEOUT"));FPlatformMisc::RequestExitWithStatus(false,1);GoldenGate=false;return;}
    if(Phase==EC26Phase::Ready)
    {
        GateStage=3+GateMatches*12+Rules.Current*6+Rules.Now().LegalBalls;
        PlayerTeam=BattingTeam(); // Drive real batting input in both innings; the smoke separately covers AI batting/bowl-first.
        if(PhaseTime>.30f)Frame(TEXT("ready"));
        if(PhaseTime>.65f)
        {
            const int Ball=Rules.Now().LegalBalls;
            Bowling=FC26DeliveryPlan();Bowling.Line=Rules.Current==1?58.f:10.f;
            Bowling.Length=Rules.Current==0?(Ball==5?805.f:630.f):460.f;
            if(Rules.Current==0&&Ball==5)Bowling.Line=0;
            C26Delivery::Shape(Bowling);Intent={};Intent.Power=1;Intent.Stride=.75f;
            Intent.Angle=Ball==0?35.f:Ball==2?60.f:Ball==4?-25.f:0.f;
            Intent.Loft=Ball==1||Ball==4;Intent.Defend=Ball==3;
            if(Ball==2)Intent.Power=.70f;
            StartDelivery();ReleaseQuality=0.f;
            const auto Keep=LockedBowling;
            // Real UI commands are inert after locking, including a drag target and variation taps.
            const int OriginalTeam=PlayerTeam;PlayerTeam=1-BattingTeam();
            UIAction(TEXT("delivery"));UIAction(TEXT("length"));UIAction(TEXT("line"));AimPitch(-100,70);
            Check(Bowling.Line==Keep.Line&&Bowling.Length==Keep.Length&&Bowling.Type==Keep.Type,TEXT("run-up rejects target and variation edits"));
            PlayerTeam=OriginalTeam;
        }
    }
    else if(Phase==EC26Phase::RunUp)
    {
        if(PhaseTime>1.7f)Frame(TEXT("runup"));
        if(PhaseTime>C26Field::RunUpDuration-.38f)Frame(TEXT("gather"));
    }
    else if(Phase==EC26Phase::Delivery)
    {
        if(Simulation.Ball.Age==0&&Frame(TEXT("release")))
            Check(FVector::Dist(Simulation.Ball.Position,Athletes[0]->HandPosition())<1.f,TEXT("exact release"));
        const int Ball=Rules.Now().LegalBalls;
        if(Rules.Current==0&&Ball!=5&&!ShotQueued&&TimingCountdown()<=.1f+Dt*.5f)Shot(Intent);
        if(KeeperTakeClock>=0&&Frame(TEXT("keeper_take")))
        {
            const float Gap=FVector::Dist(MissTakeTarget,Athletes[1]->ReceivingPosition());
            UE_LOG(LogC26,Display,TEXT("C26_SUITE_KEEPER gap_cm=%.2f target=%s actual=%s"),Gap,*MissTakeTarget.ToCompactString(),*Athletes[1]->ReceivingPosition().ToCompactString());
            Check(Gap<12.f,TEXT("incoming ball meets keeper gloves"));
        }
    }
    else if(Phase==EC26Phase::InPlay)
    {
        if(PhaseTime==0&&Frame(TEXT("contact")))
        {
            const FVector P=Athletes[11]->VisualBat()->GetComponentTransform().InverseTransformPosition(Simulation.Ball.Position);
            float Gap=BIG_NUMBER;const UStaticMesh* Mesh=Athletes[11]->VisualBat()->GetStaticMesh();
            if(Mesh&&Mesh->GetRenderData()&&Mesh->GetRenderData()->LODResources.Num())
            {
                const auto& L=Mesh->GetRenderData()->LODResources[0];auto Indices=L.IndexBuffer.GetArrayView();
                const auto& V=L.VertexBuffers.PositionVertexBuffer;
                for(int I=0;I+2<Indices.Num();I+=3)
                {
                    const FVector A(V.VertexPosition(Indices[I])),B(V.VertexPosition(Indices[I+1])),C(V.VertexPosition(Indices[I+2]));
                    if(FMath::Max3(A.Z,B.Z,C.Z)>-24)continue;
                    Gap=FMath::Min(Gap,float(FVector::Dist(P,FMath::ClosestPointOnTriangleToPoint(P,A,B,C))));
                }
            }
            UE_LOG(LogC26,Display,TEXT("C26_SUITE_CONTACT shot=%s gap_cm=%.3f"),*LastContact.Shot,Gap);
            Check(Gap<=Tuning.BallRadius,TEXT("selected shot meets actual blade"));
        }
        if(PhaseTime>.7f)Frame(TEXT("tracking"));
        if(Rules.Now().LegalBalls==2&&PhaseTime>.45f&&!Running&&CompletedRuns==0)Run();
        if(ThrowClock>=.20f&&ThrowClock<.24f&&Frame(TEXT("pickup")))
        {
            auto* F=Athletes[ActiveFielder].Get();
            const FVector Palms=F->ReceivingPosition();
            const FVector Root=F->GetActorLocation();
            const float Gap=FVector::Dist(GatherPoint,Palms);
            // Positions, not just the scalar: a bare gap cannot distinguish "the arm ran out of
            // reach" from "the athlete never got to the ball" from "the palms are measured off
            // the wrist rather than the ball", and those need opposite fixes.
            UE_LOG(LogC26,Display,TEXT("C26_SUITE_PICKUP gap_cm=%.2f gather=%s palms=%s root=%s gather_minus_root=%s"),
                Gap,*GatherPoint.ToCompactString(),*Palms.ToCompactString(),*Root.ToCompactString(),
                *(GatherPoint-Root).ToCompactString());
            Check(Gap<14.f,TEXT("ground pickup reaches the ball"));
        }
        if(ThrowReleased&&FMath::IsNearlyEqual(ThrowClock,.73f,.001f)&&Frame(TEXT("throw_release")))
            Check(FVector::Dist(Simulation.Ball.Position,Athletes[ActiveFielder]->HandPosition())<1.f,TEXT("exact fielding throw release"));
    }
    else if(Phase==EC26Phase::Reaction)
    {
        if(PhaseTime>.25f&&Frame(TEXT("outcome")))
        {
            const auto& O=Rules.Now().Ledger.back();
            GateSawFour|=O.Rope==C26::Boundary::Four;GateSawSix|=O.Rope==C26::Boundary::Six;GateSawWicket|=O.Wicket==C26::Dismissal::Bowled;
            Check(int(Rules.Now().Ledger.size())==Rules.Now().LegalBalls,TEXT("one authoritative commit per legal ball"));
            UE_LOG(LogC26,Display,TEXT("C26_SUITE_OUTCOME match=%d innings=%d ball=%d call=%s score=%d/%d"),GateMatches,Rules.Current+1,Rules.Now().LegalBalls,*Callout,Rules.Now().Runs,Rules.Now().Wickets);
        }
    }
    else if(Phase==EC26Phase::Replay)
    {
        if(FVector::Dist(Simulation.Ball.Position,LastContact.ContactPoint)<8)Frame(TEXT("replay_contact"));
    }
    else if(Phase==EC26Phase::Interval)
    {
        if(PhaseTime>.2f&&Frame(TEXT("interval")))Check(Rules.Scores[0].LegalBalls==6,TEXT("first innings completed all six balls"));
        if(PhaseTime>.65f)Skip();
    }
    else if(Phase==EC26Phase::Result&&PhaseTime>.4f)
    {
        if(!Frame(TEXT("result")))return;
        Check(Rules.Scores[0].LegalBalls==6&&Rules.Scores[1].LegalBalls==6,TEXT("two complete six-ball innings"));
        Check(GateSawFour&&GateSawSix&&GateSawWicket,TEXT("natural four, six and bowled delivered"));
        ++GateMatches;
        if(GateMatches<2)
        {
            PlayerBatsFirst=true;StartMatch();Skip();
            Check(Rules.Now().Runs==0&&Rules.Now().LegalBalls==0&&!Director->IsReplaying,TEXT("second full match resets state"));
        }
        else
        {
            UE_LOG(LogC26,Display,TEXT("C26_SUITE_%s matches=%d failures=%d four=%d six=%d bowled=%d"),GateFailures?TEXT("FAIL"):TEXT("PASS"),GateMatches,GateFailures,GateSawFour,GateSawSix,GateSawWicket);
            GoldenGate=false;FPlatformMisc::RequestExitWithStatus(false,GateFailures?1:0);
        }
    }
}
#endif
