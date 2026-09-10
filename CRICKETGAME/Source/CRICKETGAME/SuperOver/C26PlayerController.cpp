#include "C26PlayerController.h"
#include "C26MatchGameMode.h"
#include "C26HUD.h"
#include "C26Settings.h"
#include "C26Athlete.h"
#include "Engine/World.h"
AC26PlayerController::AC26PlayerController(){bShowMouseCursor=true;bEnableTouchEvents=true;bEnableClickEvents=false;}
void AC26PlayerController::BeginPlay(){Super::BeginPlay();SetInputMode(FInputModeGameOnly());bShowMouseCursor=true;}
void AC26PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindTouch(IE_Pressed,this,&AC26PlayerController::TouchStart);
    InputComponent->BindTouch(IE_Repeat,this,&AC26PlayerController::TouchMove);
    InputComponent->BindTouch(IE_Released,this,&AC26PlayerController::TouchEnd);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AC26PlayerController::MouseStart);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AC26PlayerController::MouseEnd);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&AC26PlayerController::Action);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AC26PlayerController::Run);
    InputComponent->BindKey(EKeys::C,IE_Pressed,this,&AC26PlayerController::Cancel);
    InputComponent->BindKey(EKeys::L,IE_Pressed,this,&AC26PlayerController::Loft);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AC26PlayerController::PauseMatch);
}
void AC26PlayerController::BeginGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];G={};G.Active=true;G.Start=G.Last=P;G.Time=FPlatformTime::Seconds();
    const FName Button=H->ActionAt(P);
    if(!Button.IsNone()){G.Consumed=true;M->UIAction(Button);return;}
    if(M->Paused||M->SettingsOpen||M->ControlsOpen){G.Consumed=true;return;}
    G.Foot=H->ToDesign(P).X<420&&M->PlayerBatting();MoveGesture(Index,P);
}
void AC26PlayerController::MoveGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];if(!G.Active||G.Consumed)return;G.Last=P;
    if(M->Paused||M->SettingsOpen||M->ControlsOpen)return;
    if(G.Foot&&!M->ShotQueued)
    {
        M->Footwork=FMath::Clamp((H->ToDesign(P).X-167)/72.f,-1.f,1.f);
        M->Intent.Stride=FMath::Clamp((721.f-H->ToDesign(P).Y)/82.f,-1.f,1.f);
        if(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp)M->Athletes[11]->SetActorLocation(FVector(-38+M->Footwork*35,900,5));
    }
    else if(!M->PlayerBatting()&&M->Phase==EC26Phase::Ready)
    {
        FVector Origin,Dir;
        if(DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Dir)&&FMath::Abs(Dir.Z)>.001f)
        {const FVector Hit=Origin+Dir*((8-Origin.Z)/Dir.Z);M->AimPitch(Hit.X,Hit.Y);}
    }
}
void AC26PlayerController::EndGesture(int Index,FVector2D P)
{
    auto* H=Cast<AC26HUD>(GetHUD());auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!H||!M||Index<0||Index>=10)return;
    auto& G=Gestures[Index];if(!G.Active)return;G.Active=false;if(G.Consumed||G.Foot)return;
    const FVector2D Delta=(H->ToDesign(P)-H->ToDesign(G.Start))*M->Preferences->Sensitivity;
    if(M->PlayerBatting()&&(M->Phase==EC26Phase::Delivery||M->Phase==EC26Phase::RunUp))
    {
        FC26ShotIntent Shot=M->Intent;
        if(Delta.Size()>18){Shot.Angle=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Delta.X,-Delta.Y)),-135.f,135.f);Shot.Power=FMath::Clamp(.5f+Delta.Size()/380.f,.45f,1.f);}
        M->Shot(Shot);
    }
}
void AC26PlayerController::TouchStart(ETouchIndex::Type F,FVector P){BeginGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::TouchMove(ETouchIndex::Type F,FVector P){MoveGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::TouchEnd(ETouchIndex::Type F,FVector P){EndGesture(int(F),FVector2D(P.X,P.Y));}
void AC26PlayerController::MouseStart(){float X,Y;if(GetMousePosition(X,Y))BeginGesture(0,FVector2D(X,Y));}
void AC26PlayerController::MouseEnd(){float X,Y;if(GetMousePosition(X,Y))EndGesture(0,FVector2D(X,Y));}
void AC26PlayerController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if(Gestures[0].Active&&IsInputKeyDown(EKeys::LeftMouseButton)){float X,Y;if(GetMousePosition(X,Y))MoveGesture(0,FVector2D(X,Y));}
    auto* M=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!M)return;
    float D=(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)?1.f:0.f)-(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)?1.f:0.f);
    if(D!=0&&!M->ShotQueued&&!M->Paused&&!M->SettingsOpen&&!M->ControlsOpen){M->Footwork=FMath::Clamp(M->Footwork+D*Dt*2,-1.f,1.f);if(M->Athletes.Num()>11&&(M->Phase==EC26Phase::Ready||M->Phase==EC26Phase::RunUp))M->Athletes[11]->SetActorLocation(FVector(-38+M->Footwork*35,900,5));}
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
