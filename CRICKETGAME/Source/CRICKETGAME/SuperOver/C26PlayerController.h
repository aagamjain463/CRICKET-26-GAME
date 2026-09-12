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
    UFUNCTION(Exec) void C26Controls();
    /** Dev: flip the striker's handedness live so the mirrored mapping can be tested. */
    UFUNCTION(Exec) void C26LeftHand();
    UFUNCTION(Exec) void C26PresentationDebug();
    UFUNCTION(Exec) void C26TriggerScene(const FString& SceneName);
    UFUNCTION(Exec) void C26Pacing(const FString& Mode);
#if !UE_BUILD_SHIPPING
    /** Scripted-playtest entry point: injects a pointer event into the REAL routing.
        Event: 0 = down, 1 = move, 2 = up. Position is in viewport pixels. */
    void DebugPointer(int32 Index,FVector2D ScreenPos,int32 Event);
#endif
private:
    /** One slot per pointer (finger 0-9, slot 0 doubles as the mouse). */
    struct FGesture
    {
        FVector2D Start = FVector2D::ZeroVector, Last = FVector2D::ZeroVector;
        double Time = 0;
        bool Active = false;     // pointer is currently down
        bool Consumed = false;   // press landed on a HUD button; ignore for gameplay
        bool Foot = false;       // pointer is driving the footwork stick
        bool Batting = false;    // pointer owns the batting gesture
        bool Mouse = false;      // slot was opened by the mouse, not a finger
        bool Pace = false;       // pointer owns the bowling pace slider
        bool Movement = false;   // pointer owns the bowling movement dial
        bool Target = false;     // pointer owns the bowling pitch target
        bool FieldDrag = false;  // pointer is dragging a fielder in field planning
    };
    FGesture Gestures[10];
    /** Which pointer currently owns the batting gesture, or -1. Mirrors the game mode. */
    int32 BattingPointer = -1;
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
    void DiveKey();
    void FieldKey();
    void ThrowBowlerKey();
    void ThrowKeeperKey();
    void TogglePresentationDebug();
    void DebugTriggerToss();
    void DebugTriggerWicket();
    void DebugTriggerCaught();
    void DebugTriggerFifty();
    void DebugTriggerCentury();
    void DebugTriggerBowlerCaptain();
    void DebugTriggerNewBatter();
    void DebugTriggerEndOfOver();
    void DebugTriggerMatchWin();
    void DebugTriggerHandshakes();
    void BeginGesture(int Index,FVector2D P);
    void MoveGesture(int Index,FVector2D P);
    void EndGesture(int Index,FVector2D P);
};
