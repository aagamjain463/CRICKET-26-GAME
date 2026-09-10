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
    // ---- C26 design system ----
    float FA=1.f;
    FLinearColor WithA(FLinearColor C,float M=1.f) const;
    float Enter(float Delay=0.f) const;
    bool Pressed(FName Action) const;
    void ScrimLeft(float Strength=1.f);
    void ScrimBottom(float Strength=1.f);
    void Btn(FName Action,const FString& Label,float X,float Y,float W,float H,int Style=0,bool Selected=false);
    void NavBtn(FName Action,const FString& Label,float X,float Y,float W,bool Selected);
    void TopUtility();
    void NavRail(int Selected);
    void HeroCrest(float X,float Y,float Size,int Team);
    void Tag(const FString& S,float X,float Y,bool Accent=false);
    void Toast();
    void Confirm();
    void BackBtn(FName Action=FName(TEXT("back")));
    void PageHead(const FString& Kick,const FString& Title,const FString& Sub);
    FLinearColor TeamColor(int Team) const;
    FString TeamTagline(int Team) const;
    FString Track(const FString& S) const;
    // ---- premium presentational helpers (measure-aware, no overlaps) ----
    void Vignette();
    void Panel(float X,float Y,float W,float H,FLinearColor Edge);
    void Rule(float X,float Y,float W);
    void Ghost(const FString& S,float X,float Y,float Size);
    void Crest(float X,float Y,float R,int Team);
    void PlayGlyph(float X,float Y,float S,FLinearColor C);
    float TH(float Size) const;
    void Menu();
    void Home(); void Play(); void Teams(); void Matchup(); void Toss();
    void Future(int Kind);
    void SettingsHub(); void Help();
    void Score();
    void Controls();
    void Result();
    void Preferences();
    void Subtitle();
};
