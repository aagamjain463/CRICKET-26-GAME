#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "C26Types.h"
#include "C26HUD.generated.h"
class AC26MatchGameMode;

struct FC26HitZone{FName Action;FBox2D Rect;};

UCLASS()
class CRICKETGAME_API AC26HUD : public AHUD
{
    GENERATED_BODY()
public:
    AC26HUD();
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    FName ActionAt(FVector2D ScreenPos) const;
    FVector2D ToDesign(FVector2D ScreenPos) const;
    FVector2D FromDesign(FVector2D DesignPos) const;

    // Strict 8-Point Spacing Scale Tokens
    static constexpr float Sp4  = 4.f;
    static constexpr float Sp8  = 8.f;
    static constexpr float Sp12 = 12.f;
    static constexpr float Sp16 = 16.f;
    static constexpr float Sp20 = 20.f;
    static constexpr float Sp24 = 24.f;
    static constexpr float Sp32 = 32.f;
    static constexpr float Sp48 = 48.f;
    static constexpr float Sp64 = 64.f;

    // Semantic aliases. Every gap and every padding in the layout resolves to
    // one of these, so the same relationship always reads the same distance
    // instead of drifting between 6/10/14/22/26 as the screens were added.
    static constexpr float GapLine  = Sp8;   // consecutive lines inside one block
    static constexpr float GapItem  = Sp12;  // tightly bound items (label <-> value)
    static constexpr float GapComp  = Sp16;  // sibling components
    static constexpr float GapBlock = Sp24;  // blocks inside one card
    static constexpr float GapSect  = Sp32;  // sections on a page
    static constexpr float PadCard  = Sp32;  // feature-card inner padding
    static constexpr float PadPanel = Sp16;  // compact-panel inner padding
    static constexpr float PadEdge  = Sp12;  // minimum inset for text against a border
    static constexpr float SafeBot  = 858.f; // bottom safe line for full-width UI

    // Development visual debug flag for UI containers
    bool bUIDebug = false;

    // Drop-shadow strength behind canvas text. The broadcast gameplay HUD sits
    // over live 3D and needs it; the front end sits on a flat near-black ground
    // where it only muddies the tracked micro-labels, so Menu() zeroes it.
    float TextShadow = .80f;

    UPROPERTY() TObjectPtr<UFont> SportsFont;
    UPROPERTY() TObjectPtr<UFont> DisplayFont;
    UPROPERTY() TObjectPtr<UFont> TitleFont;
    UPROPERTY() TObjectPtr<UFont> HeavyFont;

    TArray<FC26HitZone> Zones;
    float Scale = 1, OffsetX = 0, OffsetY = 0;
    AC26MatchGameMode* Match = nullptr;

    // Line the commentary subtitle docks on. Result() lifts it clear of the
    // action row; every other phase leaves it on the default gameplay line.
    float SubtitleY = 640.f;

    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Line(float X, float Y, float X2, float Y2, FLinearColor Color, float Thickness = 1);
    void Text(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center = false, int FontChoice = 0);
    float Width(const FString& S, float Size, int FontChoice = 0) const;
    void Circle(float X, float Y, float Radius, FLinearColor Color, float Thickness = 1);
    void Button(FName Action, const FString& Label, float X, float Y, float W, float H, bool Accent = false, bool Selected = false);
    void Logo(float X, float Y, float Size, int Team);

    // ---- C26 design system ----
    float FA = 1.f;
    FLinearColor WithA(FLinearColor C, float M = 1.f) const;
    float Enter(float Delay = 0.f) const;
    bool Pressed(FName Action) const;
    void ScrimLeft(float Strength = 1.f);
    void ScrimBottom(float Strength = 1.f);
    void Btn(FName Action, const FString& Label, float X, float Y, float W, float H, int Style = 0, bool Selected = false);
    void NavBtn(FName Action, const FString& Label, float X, float Y, float W, bool Selected);
    void TopNav(int Selected);
    void TopUtility();
    void NavRail(int Selected);
    void SideNavBtn(FName Action, const FString& Label, float Y, bool Selected, const FString& IndexStr = TEXT(""));
    void ProfileChip(float X, float Y, float W, const FString& Kicker, const FString& Value, FLinearColor Accent);
    void HeroCrest(float X, float Y, float Size, int Team);
    void Tag(const FString& S, float X, float Y, bool Accent = false);
    void Toast(bool Gameplay = false);
    void Confirm();
    void BackBtn(FName Action = FName(TEXT("back")));
    void PageHead(const FString& Kick, const FString& Title, const FString& Sub);
    FLinearColor TeamColor(int Team) const;
    FString TeamTagline(int Team) const;
    FString Track(const FString& S) const;

    // ---- premium presentational helpers (measure-aware, no overlaps) ----
    void Vignette();
    void Panel(float X, float Y, float W, float H, FLinearColor Edge);
    void Rule(float X, float Y, float W);
    void Ghost(const FString& S, float X, float Y, float Size);
    void Crest(float X, float Y, float R, int Team);
    void PlayGlyph(float X, float Y, float S, FLinearColor C);
    float TH(float Size) const;
    float LineH(float Size) const;
    int WrapLines(const FString& S, float Size, float MaxW, int FontChoice = 0) const;
    float TextWrap(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center = false, int FontChoice = 0);
    void TextFit(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center = false, int FontChoice = 0);
    void TextMid(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor Color, bool Center = false, int FontChoice = 0);
    void TextMidFit(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor Color, float MaxW, bool Center = false, int FontChoice = 0);
    void StatBar(float X, float Y, float W, float H, float Pct, FLinearColor FillColor);

    // ---- CRICKET 26 // FRONT-END SHELL ------------------------------------
    // One shell for every hub screen: near-black ground, centred wordmark, a
    // flat text rail on the left and a single content bay to its right. There
    // is no card chrome and no imagery -- structure is hairlines and spacing,
    // and Mint appears at most three times on a screen.
    void Shell(int Selected);
    void Backdrop();
    void Wordmark();
    void ProfileBar();
    void Rail(int Selected);
    void SoftGlow(float CX, float CY, float RX, float RY, FLinearColor C, float Strength);
    void Disc(float CX, float CY, float R, FLinearColor C);

    // Tracked type. The canvas has no letter-spacing, so tracked labels are
    // drawn a glyph at a time with an explicit em advance; every micro-label in
    // the front end goes through this.
    void TextT(const FString& S, float X, float Y, float Size, FLinearColor C, float TrackEm, int Font, bool Center = false);
    float WidthT(const FString& S, float Size, float TrackEm, int Font) const;
    void TextTMid(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor C, float TrackEm, int Font, bool Center = false);

    void Eyebrow(const FString& S, float X, float Y);
    void Headline(const FString& A, const FString& B, float X, float Y, float Size);
    void HairRule(float X, float Y, float W, float A = .065f);
    void PrimaryBtn(FName Action, const FString& Label, float X, float Y, float W, float H);
    void GhostBtn(FName Action, const FString& Label, float X, float Y, float W, float H);
    float BtnW(const FString& Label, float Size, float Pad) const;
    void Toggle(FName Action, float X, float Y, bool On);
    void Slider(FName Action, float X, float Y, float W, float Pct);
    void MicroBar(float X, float Y, float W, float Pct);
    void StatLine(const FString& Label, const FString& Value, float X, float Y, FLinearColor VC);

    void Menu();
    void Home(); void Play(); void Teams(); void Matchup(); void Toss();
    void Squad(); void Store();
    void Career(); void Leaderboards(); void Multiplayer();
    void PlayerCard(float X, float Y, float W, float H, const FString& Name, const FString& Role, int Bat, int Bowl, int Field, int Team, bool Selected);
    void Future(int Kind);
    void SettingsHub(); void Help();
    void Score();
    void Controls();
    void Result();
    void ScorecardTable(float X, float Y, float W, float H);
    void Preferences();
    void Subtitle();
    void DrawUIDebug();

    // ---- Major Gameplay Control Overhaul: WCC3-Style Gestures & Pre-Bounce Decals ----
    void DrawBounceIndicator();
    void DrawBowlingTarget();
    void DrawBattingGestureCue();
    void DrawControlDebug();

    // ---- Control Systems Overhaul ----
    void DrawDeliveryHistory();
    void DrawFieldPlanning();
    void DrawFieldingHUD();
    void DrawBattingTimingMeter();

    // ---- Presentation & Cinematic Overlays ----
    void DrawPresentationOverlay();
    void DrawPresentationDebug();
};
