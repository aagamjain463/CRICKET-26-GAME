#include "../C26MatchGameMode.h"

#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26CameraDirector.h"
#include "../C26Stadium.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "UnrealClient.h"

void AC26MatchGameMode::UpdateGoldenGate(float Dt)
{
    auto Check=[&](bool Passed,const TCHAR* Message)
    {
        if(!Passed)++GateFailures;
        UE_LOG(LogC26,Display,TEXT("C26_GATE_CHECK stage=%d %s %s"),GateStage,Passed?TEXT("PASS"):TEXT("FAIL"),Message);
    };
    auto CaptureFrame=[&](const TCHAR* Name)
    {
        const FString Key=FString::Printf(TEXT("%d_%s"),GateStage,Name);
        if(GateShots.Contains(Key))return false;
        GateShots.Add(Key);IFileManager::Get().MakeDirectory(*GateDirectory,true);
        if(!GateNoScreens)FScreenshotRequest::RequestScreenshot(GateDirectory/(Key+TEXT(".png")),true,false);
        UE_LOG(LogC26,Display,TEXT("C26_GATE_FRAME %s"),*Key);return true;
    };
    if(FPlatformTime::Seconds()-GateStarted>120)
    {
        UE_LOG(LogC26,Error,TEXT("C26_GATE_TIMEOUT stage=%d phase=%d"),GateStage,int(Phase));
        GoldenGate=false;FPlatformMisc::RequestExitWithStatus(false,1);return;
    }
    if(Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery||Phase==EC26Phase::InPlay)
        GateFrameTimes.Add(FApp::GetDeltaTime()*1000.f);
    if(Phase==EC26Phase::Ready)
    {
        if(GateStage==0&&Rules.Now().LegalBalls==1)
        {
            Check(GateCollected&&GateThrown,TEXT("drive completed pickup and hand throw"));
            Check(!Simulation.Ball.Active&&!ShotQueued&&!Running,TEXT("next delivery cleared live state"));
            GateStage=1;
        }
        else if(GateStage==1&&Rules.Now().LegalBalls==2)
        {
            Check(LastContact.Timing==EC26Timing::Miss,TEXT("early input missed"));
            Check(Rules.Now().Ledger.size()==2,TEXT("one score commit per delivery"));
            GateStage=2;StartMatch();Skip();
            Check(Rules.Epoch!=GateEpoch&&Rules.Now().LegalBalls==0&&Rules.Now().Runs==0,
                TEXT("restart creates clean match epoch"));
            Check(!Director->IsReplaying&&FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this),1.f),
                TEXT("restart clears replay and hit stop"));
        }
        else if(GateStage==2&&Rules.Now().LegalBalls==1)
        {
            Check(Rules.Now().Ledger.size()==1,TEXT("delivery after restart commits exactly once"));
            GateFrameTimes.Sort();
            float Sum=0;for(float Ms:GateFrameTimes)Sum+=Ms;
            const int Count=GateFrameTimes.Num();
            UE_LOG(LogC26,Display,TEXT("C26_GATE_FRAME_TIME samples=%d mean_ms=%.2f p95_ms=%.2f max_ms=%.2f screenshots=%d fixed_step=%d (desktop only)"),
                Count,Count?Sum/Count:0,Count?GateFrameTimes[FMath::Min(Count-1,FMath::FloorToInt(Count*.95f))]:0,Count?GateFrameTimes.Last():0,!GateNoScreens,FApp::UseFixedTimeStep());
            UE_LOG(LogC26,Display,TEXT("C26_GATE_%s failures=%d frames=%d"),GateFailures?TEXT("FAIL"):TEXT("PASS"),GateFailures,GateShots.Num());
            GoldenGate=false;FPlatformMisc::RequestExitWithStatus(false,GateFailures?1:0);return;
        }
        if(PhaseTime>.25f&&CaptureFrame(TEXT("01_ready")))
        {
            auto* PC=GetWorld()->GetFirstPlayerController();int W=0,H=0;PC->GetViewportSize(W,H);
            FVector2D Feet,Head,Bowler;
            const FVector StrikerPos=Athletes[11]->GetActorLocation();
            PC->ProjectWorldLocationToScreen(StrikerPos,Feet);
            PC->ProjectWorldLocationToScreen(StrikerPos+FVector(0,0,180),Head);
            PC->ProjectWorldLocationToScreen(Athletes[0]->GetActorLocation()+FVector(0,0,150),Bowler);
            const float Height=(Feet.Y-Head.Y)/FMath::Max(1,H);
            UE_LOG(LogC26,Display,TEXT("C26_GATE_COMPOSITION athlete_height_fraction=%.3f feet=%s head=%s bowler=%s"),Height,*Feet.ToString(),*Head.ToString(),*Bowler.ToString());
            Check(Height>.25f&&Height<.53f,TEXT("human-scale batter occupies readable gameplay frame"));
            Check(Head.Y>H*.20f&&Feet.Y<H*.90f,TEXT("batter head and feet inside gameplay safe area"));
            Check(Bowler.X>W*.2f&&Bowler.X<W*.8f&&Bowler.Y>H*.22f&&Bowler.Y<H*.7f,TEXT("bowler clear of top HUD"));
            Check(Venue&&Venue->Bowl->GetNumSections()==2,TEXT("continuous ground plus crease paint only"));
            Check(Venue&&Venue->Bowl->GetMaterial(0)&&Venue->Bowl->GetMaterial(0)->GetName().Contains(TEXT("Eclipse")),TEXT("authored playing surface loaded"));
            Check(Stumps.Num()==10&&FMath::IsNearlyEqual(float(Stumps[0]->Bounds.Origin.Z-Stumps[0]->Bounds.BoxExtent.Z),C26Field::SurfaceZ,.15f),TEXT("wicket base grounded at physics surface"));
        }
        if(PhaseTime>.8f)
        {
            Bowling=FC26DeliveryPlan();Bowling.Line=18.f;
            Intent=FC26ShotIntent();Intent.Angle=0;Intent.Power=.55f;Intent.Stride=.75f;
            AI.Random.Initialize(260026);StartDelivery();
        }
    }
    else if(Phase==EC26Phase::RunUp)
    {
        if(PhaseTime>1.9f)CaptureFrame(TEXT("02_runup"));
        if(GateStage==1&&!ShotQueued&&PhaseTime>.4f)Shot(Intent);
    }
    else if(Phase==EC26Phase::Delivery)
    {
        if(Simulation.Ball.Age==0&&CaptureFrame(TEXT("03_release")))
        {
            const float Gap=FVector::Dist(Simulation.Ball.Position,Athletes[0]->HandPosition());
            UE_LOG(LogC26,Display,TEXT("C26_GATE_RELEASE hand_gap_cm=%.3f"),Gap);
            Check(Gap<1.f,TEXT("rendered release frame stays on hand"));
        }
        if(Simulation.Ball.Bounced)CaptureFrame(TEXT("04_bounce"));
        if(GateStage!=1&&!ShotQueued&&TimingCountdown()<=.10f+Dt*.5f)Shot(Intent);
    }
    else if(Phase==EC26Phase::InPlay)
    {
        if(PhaseTime==0&&CaptureFrame(TEXT("05_contact")))
        {
            const FVector Local=Athletes[11]->Bat->GetComponentTransform().InverseTransformPosition(Simulation.Ball.Position);
            // Measure the actual generated blade triangles, not just the simulation contact plane.
            float Gap=BIG_NUMBER;
            const FProcMeshSection* Blade=Athletes[11]->Bat->GetProcMeshSection(0);
            if(Blade)for(int Triangle=0;Triangle+2<Blade->ProcIndexBuffer.Num();Triangle+=3)
            {
                const FVector A=Blade->ProcVertexBuffer[Blade->ProcIndexBuffer[Triangle]].Position;
                const FVector B=Blade->ProcVertexBuffer[Blade->ProcIndexBuffer[Triangle+1]].Position;
                const FVector C=Blade->ProcVertexBuffer[Blade->ProcIndexBuffer[Triangle+2]].Position;
                if(FMath::Max3(A.Z,B.Z,C.Z)>-24.f)continue; // Exclude handle/shoulder.
                Gap=FMath::Min(Gap,float(FVector::Dist(Local,FMath::ClosestPointOnTriangleToPoint(Local,A,B,C))));
            }
            UE_LOG(LogC26,Display,TEXT("C26_GATE_CONTACT gap_cm=%.3f ball_local=%s pose_time=%.4f"),Gap,*Local.ToString(),Athletes[11]->ActionTime);
            Check(LastContact.Shot==TEXT("STRAIGHT DRIVE")&&Simulation.Ball.Struck&&!Intent.Loft,
                TEXT("real straight-drive contact, ground intent"));
            Check(Gap<=Tuning.BallRadius,TEXT("ball intersects rendered blade triangles"));
            Check(FMath::IsNearlyEqual(Athletes[11]->ActionTime,C26Field::BatContactPoseTime,.001f),
                TEXT("contact frame uses contact pose"));
            Important=true; // Exercise replay on this fielded drive, without changing its score.
        }
        if(PhaseTime>.5f)CaptureFrame(TEXT("06_tracking"));
        if(ThrowClock>=0)
        {
            GateCollected=true;CaptureFrame(TEXT("07_pickup"));
            if(ThrowReleased){GateThrown=true;CaptureFrame(TEXT("08_throw"));}
        }
    }
    else if(Phase==EC26Phase::Reaction)
    {
        if(PhaseTime>.25f&&CaptureFrame(TEXT("09_score")))
        {
            Check(Rules.Now().LegalBalls==(GateStage==1?2:1),TEXT("legal-ball count advances once"));
            if(GateStage==1)Check(!Simulation.Ball.Struck&&LastContact.Timing==EC26Timing::Miss,TEXT("miss never emits a hit"));
        }
    }
    else if(Phase==EC26Phase::Replay)
    {
        if(FVector::Dist(Simulation.Ball.Position,LastContact.ContactPoint)<8.f)
            CaptureFrame(TEXT("10_replay_contact"));
    }
}
#endif
