#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "C26PlayerController.generated.h"
UCLASS()
class CRICKETGAME_API AC26PlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AC26PlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float Dt) override;
    UFUNCTION(Exec) void C26Force(const FString& Outcome);
    UFUNCTION(Exec) void C26Restart();
    UFUNCTION(Exec) void C26Auto();
private:
    struct FGesture{FVector2D Start,Last;double Time=0;bool Active=false,Consumed=false,Foot=false;};
    FGesture Gestures[10];
    void TouchStart(ETouchIndex::Type Finger,FVector P);
    void TouchMove(ETouchIndex::Type Finger,FVector P);
    void TouchEnd(ETouchIndex::Type Finger,FVector P);
    void MouseStart();
    void MouseEnd();
    void Action();
    void Run();
    void Cancel();
    void Loft();
    void PauseMatch();
    void BeginGesture(int Index,FVector2D P);
    void MoveGesture(int Index,FVector2D P);
    void EndGesture(int Index,FVector2D P);
};
