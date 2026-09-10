#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "C26UILayout.h"
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
    bool IsFootworkPoint(FVector2D Point) const;
    FVector2D FootworkInput(FVector2D Point) const;
    bool BlocksGameplayInput() const;
    bool HandleLocalAction(FName Action);
private:
    UPROPERTY() TObjectPtr<UFont> DisplayFont;
    UPROPERTY() TObjectPtr<UFont> BodyFont;
    C26UI::Layout Layout;
    FVector2D SafePaddingRatio = FVector2D::ZeroVector;
    FVector2D Pointer = FVector2D(-1.f, -1.f);
    bool MenuExpanded = false;
    int CompactPage = 0;
    int PreviousMenuScreen = -1;
    UFont* FontFor(float Size) const;
    float FontWidth(const FString& S, float Size, UFont* Font) const;
    void FontText(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center, UFont* Font);
    TArray<FString> Wrapped(const FString& S, float Size, float MaxW) const;
    void BoundedCopy(const FString& S, float X, float Y, float Size, FLinearColor Color, float W, int MaxLines, bool Center=false);
    void CompactMenu();
    void CompactControls();
    void CompactHeader(const FString& Kicker, const FString& Title);
    void CompactFooter(FName Action, const FString& Label, FName Back=FName(TEXT("nav_home")));
    void CategoryMark(float X, float Y, int Kind);
    void SettingRows(float X, float Y, float W, bool Compact);
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
    void SideNavBtn(FName Action,const FString& Label,float Y,bool Selected);
    void ProfileChip(float X,float Y,float W,const FString& Kicker,const FString& Value,FLinearColor Accent);
    void HeroCrest(float X,float Y,float Size,int Team);
    void Tag(const FString& S,float X,float Y,bool Accent=false);
    void Toast(bool Gameplay=false);
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
    float LineH(float Size) const;
    int WrapLines(const FString& S,float Size,float MaxW) const;
    float TextWrap(const FString& S,float X,float Y,float Size,FLinearColor Color,float MaxW,bool Center=false);
    void TextFit(const FString& S,float X,float Y,float Size,FLinearColor Color,float MaxW,bool Center=false);
    void TextMid(const FString& S,float X,float Y,float BoxH,float Size,FLinearColor Color,bool Center=false);
    void Menu();
    void Home(); void Play(); void Teams(); void Matchup(); void Toss();
    void Squad(); void Store();
    void PlayerCard(float X,float Y,float W,float H,const FString& Name,const FString& Role,int Bat,int Bowl,int Field,int Team,bool Selected);
    void Future(int Kind);
    void SettingsHub(); void Help();
    void Score();
    void Controls();
    void Result();
    void Preferences();
    void Subtitle();
};
