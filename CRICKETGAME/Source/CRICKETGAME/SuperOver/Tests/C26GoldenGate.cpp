#include "../C26MatchGameMode.h"

#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26CameraDirector.h"
#include "../C26Stadium.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "UnrealClient.h"

void AC26MatchGameMode::UpdateGoldenGate(float Dt)
{
    if(GateSuite&&GateStage>=3){UpdateProductionGate(Dt);return;}
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
    if(FPlatformTime::Seconds()-GateStarted>180)
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
            if(GateSuite){GateStage=3;PlayerBatsFirst=true;StartMatch();Skip();return;}
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
            Check(Height>.05f&&Height<.53f,TEXT("human-scale batter occupies readable gameplay frame"));
            Check(Head.Y>0.f&&Feet.Y<H,TEXT("batter head and feet inside gameplay safe area"));
            Check(Bowler.X>W*.2f&&Bowler.X<W*.8f,TEXT("bowler clear of top HUD"));
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
        if(PhaseTime>C26Field::RunUpDuration-.48f)CaptureFrame(TEXT("02b_gather"));
        if(PhaseTime>C26Field::RunUpDuration-.15f)CaptureFrame(TEXT("02c_plant"));
        if(GateStage==1&&!ShotQueued&&PhaseTime>.4f)Shot(Intent);
        // The pitch marker must be readable from the FIRST frame of the run-up,
        // long before the ball exists, and it must still be there at the gather.
        if(GateStage==0&&!GateMarkerRunUpChecked&&PhaseTime>.25f)
        {
            GateMarkerRunUpChecked=true;
            Check(IsBounceIndicatorVisible(),TEXT("pitch marker visible at run-up start"));
            Check(BouncePrediction.bFromIntent,TEXT("run-up marker shows bowler intent"));
            UE_LOG(LogC26,Display,TEXT("C26_GATE_MARKER_RUNUP shown=(%.1f,%.1f) intent=(%.1f,%.1f) alpha=%.2f t=%.2f"),
                BouncePrediction.DisplayLocation.X,BouncePrediction.DisplayLocation.Y,
                BouncePrediction.IntendedLocation.X,BouncePrediction.IntendedLocation.Y,BouncePrediction.Alpha,PhaseTime);
        }
        if(GateStage==0&&PhaseTime>C26Field::RunUpDuration-.15f)
            Check(IsBounceIndicatorVisible(),TEXT("pitch marker still visible at delivery stride"));
        // Press and begin pulling DURING the run-up, exactly as a player would.
        if(GateStage==0&&GateGestureArmed&&!GateGesturePressed&&!ShotQueued&&!bBattingGestureActive&&PhaseTime>.5f)
        {
            GateGesturePressed=BeginBattingGesture(0,FVector2D(1200.f,500.f));
            Check(GateGesturePressed,TEXT("batting gesture accepted during run-up"));
            Check(!ShotQueued,TEXT("press alone never commits a shot"));
        }
        if(GateStage==0&&bBattingGestureActive)
        {
            // Change of mind mid-hold: aim off side (screen-LEFT, because the
            // camera is behind the bowler), then settle back to straight.
            UpdateBattingGesture(0,FVector2D(PhaseTime<2.f?1110.f:1200.f,440.f));
            Check(!ShotQueued,TEXT("dragging never commits a shot"));
        }
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
        // Stage 0 drives the shot through the real GesturePro pull-and-release
        // path (synthetic straight pull: no loft flick, no defend) instead of
        // the legacy direct Shot() call, proving the gesture commits intent.
        // Keep holding through the flight, still adjusting: no shot may fire yet.
        if(GateStage==0&&bBattingGestureActive&&!ShotQueued)
        {
            UpdateBattingGesture(0,FVector2D(1200.f,420.f));
            Check(!ShotQueued,TEXT("holding through the flight never commits a shot"));
            Check(BattingState==EC26BattingState::Armed,TEXT("armed while pulling outside the dead zone"));
        }
        if(GateStage==0&&!ShotQueued&&bBattingGestureActive&&TimingCountdown()<=.10f+Dt*.5f)
        {
            const int32 CommitsBefore=GestureCommitCount;
            ReleaseBattingGesture(0,FVector2D(1200.f,420.f));
            GateGestureArmed=false;
            Check(ShotQueued,TEXT("gesture release commits the shot"));
            Check(GestureCommitCount==CommitsBefore+1,TEXT("release commits exactly one shot"));
            Check(FMath::Abs(BattingReleaseDeltaMs)<120.f,TEXT("gesture timing near ideal"));
            Check(BattingReleaseTiming==EC26ReleaseTiming::Perfect||BattingReleaseTiming==EC26ReleaseTiming::Good,
                TEXT("release timing band matches the measured delta"));
            Check(BouncePrediction.BounceTime>0.f,TEXT("bounce predicted from real trajectory"));
            // A second release from the same pointer must be a no-op.
            ReleaseBattingGesture(0,FVector2D(1200.f,420.f));
            Check(GestureCommitCount==CommitsBefore+1,TEXT("duplicate release is ignored"));
            UE_LOG(LogC26,Display,TEXT("C26_GATE_GESTURE aim=%+.1f power=%.2f mag=%.2f delta_ms=%+.1f timing=%s shot=%s state=%d"),
                BattingGestureAngle,BattingGesturePower,BattingPullFrac,BattingReleaseDeltaMs,
                *GetReleaseTimingName(),*BattingShotCandidate,int(BattingState));
        }
        if(GateStage==0&&!GateGestureLogged&&Simulation.Ball.Age>BouncePrediction.AppearTime+0.05f
            &&Simulation.Ball.Age<BouncePrediction.FadeStartTime&&!Simulation.BounceEvent)
        {
            Check(IsBounceIndicatorVisible(),TEXT("bounce marker visible mid-flight"));
            Check(!BouncePrediction.bBounced,TEXT("marker has not started fading before the bounce"));
            GateGestureLogged=true;
        }
        if(GateStage!=1&&GateStage!=0&&!ShotQueued&&TimingCountdown()<=.10f+Dt*.5f)Shot(Intent);
    }
    else if(Phase==EC26Phase::InPlay)
    {
        if(PhaseTime==0&&CaptureFrame(TEXT("05_contact")))
        {
            // Measure against the simulation's recorded contact point, NOT the live
            // ball: one frame after contact the ball has already travelled ~1m down
            // the pitch, so ball-vs-blade could never pass for any system, old or
            // new (it never did). ContactPoint is exact and integration-free.
            const FVector Local=Athletes[11]->VisualBat()->GetComponentTransform().InverseTransformPosition(LastContact.ContactPoint);
            // Measure the actual rendered blade triangles, not just the simulation contact plane.
            // The bat is now an authored static mesh rather than a generated procedural section,
            // so the same assertion reads LOD0 of the imported willow. Keeping the measurement on
            // real geometry is the whole point of this check: it is what caught contact landing
            // on the toe of the old procedural blade rather than the middle.
            float Gap=BIG_NUMBER;
            int BladeTris=0;
            if(const UStaticMesh* Willow=Athletes[11]->VisualBat()->GetStaticMesh())
                if(Willow->GetRenderData()&&Willow->GetRenderData()->LODResources.Num())
                {
                    const FStaticMeshLODResources& LOD=Willow->GetRenderData()->LODResources[0];
                    const FPositionVertexBuffer& Positions=LOD.VertexBuffers.PositionVertexBuffer;
                    FIndexArrayView Indices=LOD.IndexBuffer.GetArrayView();
                    for(int32 Triangle=0;Triangle+2<Indices.Num();Triangle+=3)
                    {
                        const FVector A(Positions.VertexPosition(Indices[Triangle]));
                        const FVector B(Positions.VertexPosition(Indices[Triangle+1]));
                        const FVector C(Positions.VertexPosition(Indices[Triangle+2]));
                        if(FMath::Max3(A.Z,B.Z,C.Z)>-24.f)continue; // Exclude handle/splice.
                        ++BladeTris;
                        Gap=FMath::Min(Gap,float(FVector::Dist(Local,FMath::ClosestPointOnTriangleToPoint(Local,A,B,C))));
                    }
                }
            Check(BladeTris>0,TEXT("bat mesh exposes blade triangles to measure"));
            UE_LOG(LogC26,Display,TEXT("C26_GATE_CONTACT gap_cm=%.3f ball_local=%s pose_time=%.4f blade_tris=%d"),Gap,*Local.ToString(),Athletes[11]->ActionTime,BladeTris);
            UE_LOG(LogC26,Display,TEXT("C26_GATE_TRUECONTACT sim=%s"),*LastContact.ContactPoint.ToString());
            Check(LastContact.Shot==TEXT("STRAIGHT DRIVE")&&Simulation.Ball.Struck&&!Intent.Loft,
                TEXT("real straight-drive contact, ground intent"));
            Check(Gap<=Tuning.BallRadius,TEXT("ball intersects rendered blade triangles"));
            Check(FMath::IsNearlyEqual(Athletes[11]->ActionTime,C26Field::BatContactPoseTime,.001f),
                TEXT("contact frame uses contact pose"));
            const float PredErr=FVector::Dist2D(BouncePrediction.DisplayLocation,Simulation.BouncePosition);
            UE_LOG(LogC26,Display,TEXT("C26_GATE_BOUNCE pred=(%.1f,%.1f) true=(%.1f,%.1f) err=%.2fcm unc=%.1f %s/%s"),
                BouncePrediction.DisplayLocation.X,BouncePrediction.DisplayLocation.Y,
                Simulation.BouncePosition.X,Simulation.BouncePosition.Y,PredErr,BouncePrediction.UncertaintyRadius,
                *GetDeliveryLengthName(),*GetDeliveryLineName());
            Check(PredErr<=BouncePrediction.UncertaintyRadius+1.f,TEXT("marker inside stated uncertainty"));
            if(GateStage==0)
            {
                // The meter and the bat must be reading one number, not two.
                Check(FMath::IsNearlyEqual(LastContact.TimingDeltaMs,BattingReleaseDeltaMs,1.f),
                    TEXT("simulation timing error equals the gesture release delta"));
                Check(LastContact.Shot==BattingShotCandidate,
                    TEXT("shot played matches the shot the gesture previewed"));
            }
            Important=true; // Exercise replay on this fielded drive, without changing its score.
        }
        if(PhaseTime>.5f)CaptureFrame(TEXT("06_tracking"));
        if(ThrowClock>=0)
        {
            GateCollected=true;CaptureFrame(TEXT("07_pickup"));
            if(ThrowReleased){GateThrown=true;CaptureFrame(TEXT("08_throw"));}
            if(ThrowReleased&&FMath::IsNearlyEqual(ThrowClock,.73f,.001f)&&CaptureFrame(TEXT("08a_throw_release")))
                Check(FVector::Dist(Simulation.Ball.Position,Athletes[ActiveFielder]->HandPosition())<1.f,TEXT("return leaves rendered throwing hand"));
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
