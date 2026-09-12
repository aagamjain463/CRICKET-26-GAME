#include "C26PlayerController.h"
#include "C26MatchGameMode.h"
#include "C26HUD.h"
#include "C26Settings.h"
#include "C26Athlete.h"
#include "Camera/CameraActor.h"

AC26PlayerController::AC26PlayerController()
{
    bShowMouseCursor=true;bEnableClickEvents=true;bEnableTouchEvents=true;bEnableTouchOverEvents=true;
}
void AC26PlayerController::BeginPlay()
{
    Super::BeginPlay();SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock).SetHideCursorDuringCapture(false));
}
void AC26PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("Action",IE_Pressed,this,&AC26PlayerController::Action);
    InputComponent->BindAction("Run",IE_Pressed,this,&AC26PlayerController::Run);
    InputComponent->BindAction("Cancel",IE_Pressed,this,&AC26PlayerController::Cancel);
    InputComponent->BindAction("Loft",IE_Pressed,this,&AC26PlayerController::Loft);
    InputComponent->BindAction("Pause",IE_Pressed,this,&AC26PlayerController::PauseMatch);
    // Direct key bindings for desktop testing (no action mapping required).
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&AC26PlayerController::Action);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&AC26PlayerController::Action);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AC26PlayerController::Run);
    InputComponent->BindKey(EKeys::C,IE_Pressed,this,&AC26PlayerController::Cancel);
    InputComponent->BindKey(EKeys::L,IE_Pressed,this,&AC26PlayerController::Loft);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AC26PlayerController::PauseMatch);
    InputComponent->BindKey(EKeys::P,IE_Pressed,this,&AC26PlayerController::PauseMatch);
    InputComponent->BindTouch(IE_Pressed,this,&AC26PlayerController::TouchStart);
    InputComponent->BindTouch(IE_Repeat,this,&AC26PlayerController::TouchMove);
    InputComponent->BindTouch(IE_Released,this,&AC26PlayerController::TouchEnd);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AC26PlayerController::MouseStart);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AC26PlayerController::MouseEnd);
}
// ---------------------------------------------------------------------------
// Pointer routing. Touch and mouse walk the SAME three functions, so the
// desktop test path and the shipping touch path can never diverge:
//   press  -> BeginGesture  -> AC26MatchGameMode::BeginBattingGesture
//   drag   -> MoveGesture   -> AC26MatchGameMode::UpdateBattingGesture
//   lift   -> EndGesture    -> AC26MatchGameMode::ReleaseBattingGesture (commits)
// Each pointer is tracked by index; only the pointer that opened the batting
// gesture may update or commit it.
// ---------------------------------------------------------------------------
void AC26PlayerController::BeginGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];G={};G.Active=true;G.Start=G.Last=P;G.Time=FPlatformTime::Seconds();
    // HUD buttons win: a press on a button is a button press, never a shot.
    const FName Button=H->ActionAt(P);
    if(!Button.IsNone()){G.Consumed=true;M->UIAction(Button);return;}
    if(M->Paused||M->SettingsOpen||M->ControlsOpen){G.Consumed=true;return;}

    const bool bGesturePro = (!M->Preferences || M->Preferences->ControlScheme == 0);
    const FVector2D DesignPos = H->ToDesign(P);

    if(bGesturePro)
    {
        if(M->PlayerBatting())
        {
            // Left of the batting zone is the footwork stick; inside it is the shot.
            if(M->IsInBattingGestureZone(DesignPos))
            {
                if(M->BeginBattingGesture(Index,DesignPos)){G.Batting=true;BattingPointer=Index;}
            }
            else if(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp||M->Phase==EC26Phase::Delivery)
            {
                G.Foot=true;
            }
        }
        else
        {
            if(M->Phase==EC26Phase::Ready)
            {
                // Planning controls are hit-tested in priority order, and each one
                // takes ownership of this pointer so a movement drag can never be
                // mistaken for a target drag or vice versa.
                if(M->IsOnPaceSlider(DesignPos))
                {
                    if(M->BeginPaceDrag(Index,DesignPos))G.Pace=true;
                }
                else if(M->IsOnMovementDial(DesignPos))
                {
                    if(M->BeginMovementDrag(Index,DesignPos))G.Movement=true;
                }
                else
                {
                    // Everything else on the pitch is the continuous bounce target.
                    FVector Origin,Dir;
                    if(DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Dir)&&FMath::Abs(Dir.Z)>.001f)
                    {
                        const FVector Hit=Origin+Dir*((8-Origin.Z)/Dir.Z);
                        M->AimPitch(Hit.X,Hit.Y);G.Target=true;
                    }
                }
            }
            else if(M->Phase==EC26Phase::RunUp)
            {
                // Press anywhere to release. BowlRelease() itself is the one-shot
                // guard, so a stray second finger cannot bowl a second ball.
                M->BowlRelease();
                G.Consumed = true;
            }
        }
    }
    else
    {
        G.Foot=DesignPos.X<420&&M->PlayerBatting();
    }
    MoveGesture(Index,P);
}
void AC26PlayerController::MoveGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];if(!G.Active||G.Consumed)return;G.Last=P;
    if(M->Paused||M->SettingsOpen||M->ControlsOpen)return;

    const bool bGesturePro = (!M->Preferences || M->Preferences->ControlScheme == 0);
    const FVector2D DesignPos = H->ToDesign(P);

    if(bGesturePro)
    {
        if(M->PlayerBatting())
        {
            if(G.Batting)
            {
                // Live aim: direction and magnitude follow the finger every frame and
                // stay changeable right up to the instant it lifts.
                M->UpdateBattingGesture(Index,DesignPos);
            }
            else if(G.Foot&&!M->ShotQueued)
            {
                M->Footwork=FMath::Clamp((DesignPos.X-167)/72.f,-1.f,1.f);
                M->Intent.Stride=FMath::Clamp((721.f-DesignPos.Y)/82.f,-1.f,1.f);
                M->bFootworkManual=true;
                if(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp)M->Athletes[11]->SetActorLocation(FVector(-38+M->Footwork*35,900,5));
            }
        }
        else
        {
            if(G.Pace)M->UpdatePaceDrag(Index,DesignPos);
            else if(G.Movement)M->UpdateMovementDrag(Index,DesignPos);
            else if(G.Target&&M->Phase==EC26Phase::Ready)
            {
                FVector Origin,Dir;
                if(DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Dir)&&FMath::Abs(Dir.Z)>.001f)
                {
                    const FVector Hit=Origin+Dir*((8-Origin.Z)/Dir.Z);
                    M->AimPitch(Hit.X,Hit.Y);
                }
            }
        }
    }
    else
    {
        if(G.Foot&&!M->ShotQueued)
        {
            M->Footwork=FMath::Clamp((DesignPos.X-167)/72.f,-1.f,1.f);
            M->Intent.Stride=FMath::Clamp((721.f-DesignPos.Y)/82.f,-1.f,1.f);
            if(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp)M->Athletes[11]->SetActorLocation(FVector(-38+M->Footwork*35,900,5));
        }
        else if(!M->PlayerBatting()&&M->Phase==EC26Phase::Ready)
        {
            FVector Origin,Dir;
            if(DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Dir)&&FMath::Abs(Dir.Z)>.001f)
            {
                const FVector Hit=Origin+Dir*((8-Origin.Z)/Dir.Z);
                M->AimPitch(Hit.X,Hit.Y);
            }
        }
    }
}
void AC26PlayerController::EndGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];if(!G.Active)return;
    G.Active=false;G.Mouse=false;
    const bool bWasBatting=G.Batting;G.Batting=false;
    if(BattingPointer==Index)BattingPointer=-1;
    if(G.Consumed){G.Pace=G.Movement=G.Target=false;return;}

    const bool bGesturePro = (!M->Preferences || M->Preferences->ControlScheme == 0);
    const FVector2D DesignPos = H->ToDesign(P);

    if(bGesturePro)
    {
        if(bWasBatting)
        {
            // RELEASE COMMITS THE SHOT. Nothing before this point does.
            M->ReleaseBattingGesture(Index,DesignPos);
        }
        else if(!M->PlayerBatting())
        {
            if(G.Pace)M->EndPaceDrag(Index);
            if(G.Movement)M->EndMovementDrag(Index);
        }
    }
    else
    {
        if(G.Foot)return;
        const FVector2D Delta=(DesignPos-H->ToDesign(G.Start))*M->Preferences->Sensitivity;
        if(M->PlayerBatting()&&(M->Phase==EC26Phase::Delivery||M->Phase==EC26Phase::RunUp))
        {
            FC26ShotIntent Shot=M->Intent;
            if(Delta.Size()>18){Shot.Angle=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Delta.X,-Delta.Y)),-135.f,135.f);Shot.Power=FMath::Clamp(.5f+Delta.Size()/380.f,.45f,1.f);}
            M->Shot(Shot);
        }
    }
}
void AC26PlayerController::TouchStart(ETouchIndex::Type F,FVector P){BeginGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::TouchMove(ETouchIndex::Type F,FVector P){MoveGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::TouchEnd(ETouchIndex::Type F,FVector P){EndGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::MouseStart(){float X,Y;if(GetMousePosition(X,Y)){BeginGesture(0,FVector2D(X,Y));Gestures[0].Mouse=true;}}
void AC26PlayerController::MouseEnd(){float X,Y;if(GetMousePosition(X,Y))EndGesture(0,FVector2D(X,Y));}
void AC26PlayerController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    // Mouse drag pump: UE only reports mouse *clicks*, so the held position is
    // sampled every frame. This is what makes mouse behave exactly like touch.
    if(Gestures[0].Active&&Gestures[0].Mouse&&IsInputKeyDown(EKeys::LeftMouseButton)){float X,Y;if(GetMousePosition(X,Y))MoveGesture(0,FVector2D(X,Y));}
    // Watchdog: if the button came up without an Up event (focus loss, cursor
    // leaving the window, a swallowed release) the gesture must still resolve
    // rather than leaving the batter stuck in Pulling forever.
    if(Gestures[0].Active&&Gestures[0].Mouse&&!IsInputKeyDown(EKeys::LeftMouseButton))
    {float X,Y;if(GetMousePosition(X,Y))EndGesture(0,FVector2D(X,Y));else EndGesture(0,Gestures[0].Last);}
    auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!M)return;
    // The game mode can drop a gesture on its own (pause, wicket, innings end).
    // Keep the controller's pointer bookkeeping in step so the next press works.
    if(BattingPointer>=0&&!M->bBattingGestureActive)
    {Gestures[FMath::Clamp(BattingPointer,0,9)].Batting=false;BattingPointer=-1;}
    if(M->Phase!=EC26Phase::Ready)
    {for(auto& Gs:Gestures){Gs.Pace=false;Gs.Movement=false;Gs.Target=false;}}
    float D=(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)?1.f:0.f)-(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)?1.f:0.f);
    if(D!=0&&!M->ShotQueued&&!M->Paused&&!M->SettingsOpen&&!M->ControlsOpen){M->Footwork=FMath::Clamp(M->Footwork+D*Dt*2,-1.f,1.f);M->bFootworkManual=true;if(M->Athletes.Num()>11&&(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp))M->Athletes[11]->SetActorLocation(FVector(-38+M->Footwork*35,900,5));}
}
void AC26PlayerController::Action()
{
    auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!M||M->Paused||M->SettingsOpen||M->ControlsOpen)return;
    if(M->Phase==EC26Phase::Menu||M->Phase==EC26Phase::Result)M->StartMatch();
    else if(M->Phase==EC26Phase::Ready)M->StartDelivery();
    else if(M->Phase==EC26Phase::RunUp&&!M->PlayerBatting())M->BowlRelease();
    else if(M->Phase==EC26Phase::Delivery&&M->PlayerBatting())M->Shot(M->Intent);
    else M->Skip();
}
void AC26PlayerController::Run(){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->Run();}
void AC26PlayerController::Cancel(){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->CancelRun();}
void AC26PlayerController::Loft(){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->UIAction(TEXT("loft"));}
void AC26PlayerController::PauseMatch(){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode())){if(M->SettingsOpen||M->ControlsOpen)M->UIAction(TEXT("close"));else M->Paused=!M->Paused;}}
void AC26PlayerController::C26Force(const FString& Outcome){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->DebugOutcome(Outcome);}
void AC26PlayerController::C26Restart(){if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->StartMatch();}
void AC26PlayerController::C26Auto()
{
#if !UE_BUILD_SHIPPING
    if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode()))M->AutoPlay=!M->AutoPlay;
#endif
}
#if !UE_BUILD_SHIPPING
void AC26PlayerController::DebugPointer(int32 Index,FVector2D ScreenPos,int32 Event)
{
    if(Event==0)BeginGesture(Index,ScreenPos);
    else if(Event==1)MoveGesture(Index,ScreenPos);
    else EndGesture(Index,ScreenPos);
}
#endif
void AC26PlayerController::C26LeftHand()
{
    auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!M||!M->Preferences)return;
    M->Preferences->LeftHandedBatter=!M->Preferences->LeftHandedBatter;
    M->Preferences->Save();
    M->bLeftHandedBatter=M->Preferences->LeftHandedBatter;
    M->Toast(M->bLeftHandedBatter?TEXT("LEFT-HANDED BATTER"):TEXT("RIGHT-HANDED BATTER"));
}
void AC26PlayerController::C26Controls()
{
    if(auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode())){M->bDebugControls=!M->bDebugControls;}
}
