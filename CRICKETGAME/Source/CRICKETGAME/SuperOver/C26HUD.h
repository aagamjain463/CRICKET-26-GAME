#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "C26HUD.generated.h"
class AC26MatchGameMode;
struct FC26HitZone{FName Action;FBox2D Rect;};
UCLASS()
class CRICKETGAME_API AC26HUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    FName ActionAt(FVector2D Point) const;
    FVector2D ToDesign(FVector2D Point) const;
private:
    UPROPERTY() TObjectPtr<UFont> DisplayFont;
    TArray<FC26HitZone> Zones;
    float Scale=1,OffsetX=0,OffsetY=0;
    AC26MatchGameMode* Match=nullptr;
    void Rect(float X,float Y,float W,float H,FLinearColor Color);
    void Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Thickness=1);
    void Text(const FString& S,float X,float Y,float Size,FLinearColor Color,bool Center=false);
    float Width(const FString& S,float Size) const;
    void Circle(float X,float Y,float Radius,FLinearColor Color,float Thickness=1);
    void Button(FName Action,const FString& Label,float X,float Y,float W,float H,bool Accent=false,bool Selected=false);
    void Logo(float X,float Y,float Size,int Team);
    void Menu();
    void Score();
    void Controls();
    void Result();
    void Preferences();
};
