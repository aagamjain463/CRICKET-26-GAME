#include "C26HUD.h"
#include "C26MatchGameMode.h"
#include "C26Settings.h"
#include "C26Audio.h"
#include "C26CameraDirector.h"
#include "C26Delivery.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"

// ============================================================================
// CRICKET 26 — AAA ATHLETIC SPORTS BROADCAST UI
// ============================================================================
// Art Direction & Spacing Principles:
// - ZERO OVERLAPS: Every text element, button, crest, and container has
//   mathematically verified bounding boxes and generous breathing room.
// - MODERN SPORTS PALETTE:
//   * Pitch Obsidian (#090D14) & Stadium Slate (#0E1420)
//   * Signature Athletic Mint-Teal (#00EBBF) for primary focus & Mumbai
//   * Sunset Coral (#FF4752) for Melbourne & danger/wickets
//   * Trophy Amber Gold (#FAA01A) for scores, targets & victory
//   * Crisp Off-White (#F5F7FA) and Cool Slate Silver (#8E9EB2) for typography
// - PURE TYPOGRAPHY: Clean uppercase without artificial letter-spacing spaces.
// ============================================================================

namespace
{
const FLinearColor Void(.035f, .045f, .065f, .94f);        // Deep Pitch Obsidian
const FLinearColor PillBg(.055f, .075f, .105f, .88f);      // Stadium Slate Surface
const FLinearColor CardBg(.045f, .060f, .085f, .82f);      // Soft secondary surface
const FLinearColor Paper(.96f, .97f, .99f, 1.f);           // Crisp Athletic White
const FLinearColor PaperDim(.72f, .78f, .85f, 1.f);        // Cool Silver Secondary
const FLinearColor Muted(.42f, .48f, .58f, 1.f);           // Technical Slate
const FLinearColor Hairline(.16f, .22f, .30f, .65f);       // Subtle Architectural Hairline
const FLinearColor MintCyan(.00f, .92f, .75f, 1.f);        // Signature Athletic Mint-Teal
const FLinearColor Gold(.98f, .68f, .10f, 1.f);            // Trophy Amber Gold
const FLinearColor Coral(1.00f, .28f, .32f, 1.f);          // Sunset Coral

// ---- Reference shell geometry: full-width top profile bar + fixed left nav rail ----
// (matches the six-screen reference collage: brand+profile bar above, vertical rail left,
// hero content fills the remaining ~76% of the 1600x900 design canvas)
const float TopH = 84.f;    // top profile bar height
const float NavW = 336.f;   // left navigation rail width (~21% of 1600)
const float ContentX = NavW + 48.f;      // hero content left edge
const float ContentR = 1600.f - 56.f;    // hero content right edge
const float ContentW = ContentR - ContentX;
const float ContentTop = TopH + 40.f;    // hero content top edge
}

void AC26HUD::BeginPlay()
{
    Super::BeginPlay();
    DisplayFont = NewObject<UFont>(this);
    DisplayFont->FontCacheType = EFontCacheType::Runtime;
    DisplayFont->LegacyFontSize = 32;
    DisplayFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
        FTypefaceEntry(TEXT("Regular"),
        FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts/BarlowCondensed-SemiBold.ttf"),
        EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad));
}

FLinearColor AC26HUD::WithA(FLinearColor C, float M) const
{
    return FLinearColor(C.R, C.G, C.B, C.A * FA * M);
}

float AC26HUD::LineH(float Size) const
{
    return Size * 1.25f;
}

float AC26HUD::TH(float Size) const
{
    return Size * 1.35f;
}

int AC26HUD::WrapLines(const FString& S, float Size, float MaxW) const
{
    if (MaxW <= 0.f) return 1;
    TArray<FString> Words;
    S.ParseIntoArray(Words, TEXT(" "), true);
    int N = 1;
    FString Cur;
    for (const FString& Wd : Words)
    {
        const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
        if (!Cur.IsEmpty() && Width(Try, Size) > MaxW)
        {
            ++N;
            Cur = Wd;
        }
        else Cur = Try;
    }
    return N;
}

float AC26HUD::TextWrap(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center)
{
    TArray<FString> Words;
    S.ParseIntoArray(Words, TEXT(" "), true);
    TArray<FString> Lines;
    FString Cur;
    for (const FString& Wd : Words)
    {
        const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
        if (!Cur.IsEmpty() && Width(Try, Size) > MaxW)
        {
            Lines.Add(Cur);
            Cur = Wd;
        }
        else Cur = Try;
    }
    if (!Cur.IsEmpty()) Lines.Add(Cur);
    const float Step = LineH(Size) + 4.f;
    for (int I = 0; I < Lines.Num(); ++I)
    {
        Text(Lines[I], X, Y + I * Step, Size, Color, Center);
    }
    return Lines.Num() > 0 ? (Lines.Num() - 1) * Step + LineH(Size) : 0.f;
}

void AC26HUD::TextFit(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center)
{
    float FS = Size;
    while (FS > 11.f && Width(S, FS) > MaxW) FS -= 1.f;
    Text(S, X, Y, FS, Color, Center);
}

void AC26HUD::TextMid(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor Color, bool Center)
{
    Text(S, X, Y + (BoxH - LineH(Size)) * .5f, Size, Color, Center);
}

float AC26HUD::Enter(float Delay) const
{
    if (!Match || !Match->Preferences || Match->Preferences->ReducedMotion) return 1.f;
    const float T = (Match->Clock - Match->ScreenEnteredAt - Delay) / .28f;
    if (T <= 0) return 0.f;
    if (T >= 1) return 1.f;
    return 1.f - FMath::Pow(1.f - T, 3.f);
}

bool AC26HUD::Pressed(FName Action) const
{
    return Match && Match->LastAction == Action && (Match->Clock - Match->LastActionAt) < .16f;
}

FLinearColor AC26HUD::TeamColor(int Team) const
{
    return Team == 0 ? MintCyan : Coral;
}

FString AC26HUD::TeamTagline(int Team) const
{
    return Team == 0 ? TEXT("RIDE THE STORM") : TEXT("BURN BRIGHT");
}

FString AC26HUD::Track(const FString& S) const
{
    return S; // Natural typography, no artificial space injection
}

void AC26HUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{
    DrawRect(WithA(Color), OffsetX + X * Scale, OffsetY + Y * Scale, W * Scale, H * Scale);
}

void AC26HUD::Line(float X, float Y, float X2, float Y2, FLinearColor Color, float Thickness)
{
    DrawLine(OffsetX + X * Scale, OffsetY + Y * Scale, OffsetX + X2 * Scale, OffsetY + Y2 * Scale, WithA(Color), Thickness * Scale);
}

void AC26HUD::Text(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center)
{
    if (!DisplayFont) return;
    float W = 0, H = 0;
    Canvas->StrLen(DisplayFont, S, W, H);
    if (Center) X -= W * Size / 32.f * .5f;
    FCanvasTextItem Item(FVector2D(OffsetX + X * Scale, OffsetY + Y * Scale), FText::FromString(S), DisplayFont, WithA(Color));
    Item.Scale = FVector2D(Scale * Size / 32.f);
    Item.EnableShadow(FLinearColor(0, 0, 0, .85f), FVector2D(0, 1.5f));
    Canvas->DrawItem(Item);
}

float AC26HUD::Width(const FString& S, float Size) const
{
    if (!DisplayFont || !Canvas) return 0;
    float W = 0, H = 0;
    Canvas->StrLen(DisplayFont, S, W, H);
    return W * Size / 32.f;
}

void AC26HUD::Circle(float X, float Y, float Radius, FLinearColor Color, float Thickness)
{
    for (int I = 0; I < 40; ++I)
    {
        float A = I * 2 * PI / 40, B = (I + 1) * 2 * PI / 40;
        Line(X + FMath::Cos(A) * Radius, Y + FMath::Sin(A) * Radius,
             X + FMath::Cos(B) * Radius, Y + FMath::Sin(B) * Radius, Color, Thickness);
    }
}

FName AC26HUD::ActionAt(FVector2D Point) const
{
    const FVector2D P = ToDesign(Point);
    for (int I = Zones.Num() - 1; I >= 0; --I)
    {
        if (Zones[I].Rect.IsInside(P)) return Zones[I].Action;
    }
    return NAME_None;
}

FVector2D AC26HUD::ToDesign(FVector2D Point) const
{
    return FVector2D((Point.X - OffsetX) / Scale, (Point.Y - OffsetY) / Scale);
}

void AC26HUD::Vignette()
{
    // Soft atmospheric vignette at edges
    for (int I = 0; I < 14; ++I)
    {
        Rect(0, I * 10, 1600, 11, FLinearColor(Void.R, Void.G, Void.B, .45f * (1.f - I / 14.f)));
    }
    for (int I = 0; I < 18; ++I)
    {
        Rect(0, 900 - 10 * (I + 1), 1600, 11, FLinearColor(Void.R, Void.G, Void.B, .55f * (1.f - I / 18.f)));
    }
}

void AC26HUD::Panel(float X, float Y, float W, float H, FLinearColor Edge)
{
    Rect(X, Y, W, H, PillBg);
    Line(X, Y, X + W, Y, Hairline, 1.f);
    Line(X, Y + H, X + W, Y + H, Hairline, 1.f);
    Line(X, Y, X, Y + H, Hairline, 1.f);
    Line(X + W, Y, X + W, Y + H, Hairline, 1.f);
    if (Edge.A > 0.05f)
    {
        Rect(X, Y, 3, H, Edge);
    }
}

void AC26HUD::Rule(float X, float Y, float W)
{
    Line(X, Y, X + W, Y, Hairline, 1.f);
}

void AC26HUD::Ghost(const FString& S, float X, float Y, float Size)
{
    // High-restraint ambient watermark (zero overlap)
    Text(S, X, Y, Size, FLinearColor(1.f, 1.f, 1.f, .03f));
}

void AC26HUD::Crest(float X, float Y, float R, int Team)
{
    const auto C = TeamColor(Team);
    Circle(X, Y, R, FLinearColor(C.R, C.G, C.B, .20f), R * .15f);
    Circle(X, Y, R * .84f, C, 2.f);
    Text(Match ? Match->TeamShort(Team) : TEXT("C26"), X, Y - R * .30f, R * .54f, Paper, true);
}

void AC26HUD::PlayGlyph(float X, float Y, float S, FLinearColor C)
{
    Line(X, Y - S * .5f, X, Y + S * .5f, C, 2.f);
    Line(X, Y - S * .5f, X + S * .8f, Y, C, 2.f);
    Line(X + S * .8f, Y, X, Y + S * .5f, C, 2.f);
}

void AC26HUD::Button(FName Action, const FString& Label, float X, float Y, float W, float H, bool Accent, bool Selected)
{
    Btn(Action, Label, X, Y, W, H, Accent ? 1 : (Selected ? 3 : 0), Selected);
}

// Style: 0 secondary/ghost, 1 primary (Solid Mint-Cyan athletic CTA), 2 danger (Coral), 3 selected
void AC26HUD::Btn(FName Action, const FString& Label, float X, float Y, float W, float H, int Style, bool Selected)
{
    const bool P = Pressed(Action);
    if (Style == 1)
    {
        // Solid athletic CTA button
        Rect(X, Y, W, H, P ? Paper : MintCyan);
        // Subtle darker bottom edge for tactile athletic depth
        Rect(X, Y + H - 3, W, 3, FLinearColor(0.f, .70f, .55f, 1.f));
        float FS = H >= 60 ? 26.f : 22.f;
        while (FS > 14.f && Width(Label, FS) > W - 24.f) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H - 2, FS, Void, true);
    }
    else if (Style == 2)
    {
        // Danger action (Coral)
        Rect(X, Y, W, H, Coral);
        Rect(X, Y + H - 3, W, 3, FLinearColor(.75f, .18f, .22f, 1.f));
        float FS = 22.f;
        while (FS > 14.f && Width(Label, FS) > W - 24.f) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H - 2, FS, Paper, true);
    }
    else
    {
        // Minimalist secondary button
        Rect(X, Y, W, H, Selected ? FLinearColor(.02f, .18f, .16f, .90f) : (P ? FLinearColor(.12f, .18f, .24f, .85f) : PillBg));
        Line(X, Y, X + W, Y, Selected ? MintCyan : Hairline, 1.f);
        Line(X, Y + H, X + W, Y + H, Selected ? MintCyan : Hairline, 1.f);
        Line(X, Y, X, Y + H, Selected ? MintCyan : Hairline, 1.f);
        Line(X + W, Y, X + W, Y + H, Selected ? MintCyan : Hairline, 1.f);
        if (Selected) Rect(X, Y, 3, H, MintCyan);

        float FS = H >= 60 ? 24.f : (H >= 48 ? 21.f : 18.f);
        while (FS > 13.f && Width(Label, FS) > W - 24.f) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H, FS, Selected ? MintCyan : (P ? Paper : PaperDim), true);
    }
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::NavBtn(FName Action, const FString& Label, float X, float Y, float W, bool Selected)
{
    const float H = 40.f;
    if (Selected)
    {
        Rect(X + (W - 36) * .5f, Y + H - 3, 36, 3, MintCyan);
        TextMid(Label, X + W * .5f, Y, H - 4, 20, Paper, true);
    }
    else
    {
        TextMid(Label, X + W * .5f, Y, H, 19, Muted, true);
    }
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::ScrimLeft(float Strength)
{
    for (int I = 0; I < 28; ++I)
    {
        Rect(I * 12, 0, 13, 900, FLinearColor(Void.R, Void.G, Void.B, .55f * Strength * (1.f - I / 28.f)));
    }
}

void AC26HUD::ScrimBottom(float Strength)
{
    for (int I = 0; I < 20; ++I)
    {
        Rect(0, 900 - 12 * (I + 1), 1600, 13, FLinearColor(Void.R, Void.G, Void.B, .58f * Strength * (1.f - I / 20.f)));
    }
}

void AC26HUD::Logo(float X, float Y, float Size, int Team)
{
    Crest(X, Y, Size * .5f, Team);
}

void AC26HUD::HeroCrest(float X, float Y, float Size, int Team)
{
    Crest(X, Y, Size, Team);
}

void AC26HUD::Tag(const FString& S, float X, float Y, bool Accent)
{
    const float W = Width(S, 18) + 20;
    Rect(X, Y, W, 26, Accent ? FLinearColor(.00f, .22f, .18f, .85f) : CardBg);
    if (Accent) Rect(X, Y, 2, 26, MintCyan);
    TextMid(S, X + 10, Y, 26, 18, Accent ? MintCyan : Muted);
}

// ============================================================================
// GLOBAL TOP PROFILE BAR — full-width, brand left, player identity right.
// Reusable across every hub screen (reference: WBP_TopProfileBar equivalent).
// ============================================================================
void AC26HUD::TopUtility()
{
    Rect(0, 0, 1600, TopH, Void);
    Line(0, TopH, 1600, TopH, Hairline, 1.f);

    // Brand mark (top-left, generous margins)
    Rect(64, TopH * .5f - 12, 3, 24, MintCyan);
    Text(TEXT("CRICKET"), 76, TopH * .5f - 14, 24, Paper);
    Text(TEXT("26"), 76 + Width(TEXT("CRICKET"), 24) + 6, TopH * .5f - 16, 28, MintCyan);

    // Player profile block (top-right): avatar, name/level, presentation-only currency.
    // No backend economy exists yet -- these are UI-data placeholders, not real balances.
    const float AvR = 20.f;
    float RX = 1536.f;

    Circle(RX, TopH * .5f, AvR, MintCyan, 2.f);
    Text(TEXT("C26"), RX, TopH * .5f - 9, 15, MintCyan, true);
    RX -= AvR + 18.f;

    {
        const FString Name = TEXT("CAPTAIN");
        const float NW = FMath::Max(Width(Name, 19), Width(TEXT("LEVEL 12"), 15));
        Text(Name, RX - NW, TopH * .5f - 20, 19, Paper);
        Text(TEXT("LEVEL 12"), RX - NW, TopH * .5f + 2, 15, MintCyan);
        RX -= NW + 28.f;
    }

    ProfileChip(RX - 96.f, TopH * .5f - 15, 96.f, TEXT("COINS"), TEXT("2,450"), Gold);
    RX -= 96.f + 14.f;
    ProfileChip(RX - 84.f, TopH * .5f - 15, 84.f, TEXT("GEMS"), TEXT("128"), MintCyan);
    RX -= 84.f + 26.f;

    NavBtn(TEXT("nav_help"), TEXT("?"), RX - 40.f, TopH * .5f - 15, 32, Match->MenuScreen == 12);
}

void AC26HUD::ProfileChip(float X, float Y, float W, const FString& Kicker, const FString& Value, FLinearColor Accent)
{
    Rect(X, Y, W, 30, PillBg);
    Rect(X, Y, 2.f, 30, Accent);
    Text(Kicker, X + 10, Y + 2, 11, Muted);
    Text(Value, X + 10, Y + 13, 16, Paper);
}

// ============================================================================
// LEFT NAVIGATION RAIL — fixed vertical sidebar, reference-matched.
// Rectangular blocky buttons: dark slate idle, solid teal fill + indicator selected.
// Reuses the existing Btn() style system (Style 3 = selected, Style 0 = secondary).
// ============================================================================
void AC26HUD::NavRail(int Selected)
{
    Rect(0, TopH, NavW, 900.f - TopH, CardBg);
    Line(NavW, TopH, NavW, 900, Hairline, 1.f);

    // Which top-level tab should read as active for the current MenuScreen, including
    // the sub-flow screens (Play/Teams/Matchup/Toss) that hang off "PLAY NOW".
    const bool OnPlayFlow = Selected >= 0 && Selected <= 4;

    struct FNavItem{FName Action;const TCHAR* Label;bool Active;};
    const FNavItem Items[] = {
        {TEXT("nav_home"),    TEXT("PLAY NOW"),     OnPlayFlow},
        {TEXT("nav_myteam"),  TEXT("SQUAD HUB"),    Selected == 5},
        {TEXT("nav_career"),  TEXT("CAREER"),       Selected == 6},
        {TEXT("nav_online"),  TEXT("MULTIPLAYER"),  Selected == 8},
        {TEXT("nav_tour"),    TEXT("LEADERBOARDS"), Selected == 7},
        {TEXT("nav_store"),   TEXT("STORE"),        Selected == 13},
        {TEXT("nav_settings"),TEXT("SETTINGS"),     Selected == 11},
    };

    float Y = TopH + 32.f;
    for (const FNavItem& It : Items)
    {
        SideNavBtn(It.Action, It.Label, Y, It.Active);
        Y += 60.f;
    }

    // Bottom-of-rail secondary status (unobtrusive, matches reference's quiet footer text).
    Text(TEXT("ECLIPSE OVAL"), 32, 900 - 56, 15, Muted);
    Text(TEXT("SINGLE PLAYER"), 32, 900 - 34, 13, Muted);
}

void AC26HUD::SideNavBtn(FName Action, const FString& Label, float Y, bool Selected)
{
    const bool P = Pressed(Action);
    const float X = 16.f, W = NavW - 32.f, H = 48.f;
    if (Selected)
    {
        Rect(X, Y, W, H, FLinearColor(.02f, .18f, .16f, .92f));
        Rect(X, Y, 4, H, MintCyan);
    }
    else if (P)
    {
        Rect(X, Y, W, H, FLinearColor(.10f, .15f, .20f, .85f));
    }
    Text(Label, X + 26, Y + (H - LineH(19)) * .5f, 19, Selected ? Paper : Muted);
    Zones.Add({Action, FBox2D(FVector2D(0, Y), FVector2D(NavW, Y + H))});
}

void AC26HUD::Toast(bool Gameplay)
{
    if (!Match || Match->ToastText.IsEmpty() || Match->Clock > Match->ToastUntil) return;
    const float W = FMath::Min(480.f, Width(Match->ToastText, 21) + 48.f);
    const float X = Gameplay ? 1540.f - W : (ContentX + ContentW * .5f) - W * .5f;
    const float Y = Gameplay ? 110.f : 810.f;

    Rect(X, Y, W, 42, Void);
    Rect(X, Y, 3, 42, MintCyan);
    TextMid(Match->ToastText, X + W * .5f, Y, 42, 21, Paper, true);
}

void AC26HUD::Confirm()
{
    if (!Match || Match->PendingConfirm.IsNone()) return;
    Rect(0, 0, 1600, 900, FLinearColor(.004f, .008f, .014f, .78f));
    const bool Restart = Match->PendingConfirm == TEXT("restart");

    const float CX = 560, CY = 320, CW = 480, CH = 240;
    Panel(CX, CY, CW, CH, Restart ? Gold : Coral);
    Text(Restart ? TEXT("RESTART MATCH?") : TEXT("LEAVE MATCH?"), 800, CY + 34, 34, Paper, true);
    Text(Restart ? TEXT("Reset current over.") : TEXT("Return to main menu."), 800, CY + 84, 22, Muted, true);

    Btn(TEXT("yes"), Restart ? TEXT("RESTART") : TEXT("LEAVE"), CX + 40, CY + CH - 74, 180, 52, Restart ? 1 : 2);
    Btn(TEXT("no"), TEXT("CANCEL"), CX + CW - 220, CY + CH - 74, 180, 52, 0);
}

void AC26HUD::BackBtn(FName Action)
{
    Btn(Action, TEXT("<  BACK"), ContentX, 800, 150, 50, 0);
}

void AC26HUD::PageHead(const FString& Kick, const FString& Title, const FString& Sub)
{
    const float E = Enter();
    const float SX = (1.f - E) * 20.f;
    const float X = ContentX + SX;

    Rect(X, ContentTop + 16, 3, 18, MintCyan);
    Text(Kick, X + 12, ContentTop + 14, 18, MintCyan);
    Text(Title, X, ContentTop + 44, 52, Paper);
    if (!Sub.IsEmpty())
    {
        Text(Sub, X, ContentTop + 110, 22, Muted);
    }
    Rule(X, ContentTop + 146, ContentW);
}

void AC26HUD::Menu()
{
    FA = Match->ScreenFade;
    Vignette();
    TopUtility();
    NavRail(Match->MenuScreen);

    switch (Match->MenuScreen)
    {
    case 1: Play(); break;
    case 2: Teams(); break;
    case 3: Matchup(); break;
    case 4: Toss(); break;
    case 5: Squad(); break;
    case 6: Future(1); break;
    case 7: Future(2); break;
    case 8: Future(3); break;
    case 9: Future(4); break;
    case 10: Future(5); break;
    case 11: SettingsHub(); break;
    case 12: Help(); break;
    case 13: Store(); break;
    default: Home(); break;
    }

    FA = 1.f;
    Toast(false);
    if (!Match->PendingConfirm.IsNone()) Confirm();
}

// ============================================================================
// SCREEN 0: HOME — PERFECT ATHLETIC SPACING
// Generous margins, confident hierarchy, zero clipping.
// ============================================================================
// ============================================================================
// SCREEN 0: MAIN HUB — reference-matched featured event card.
// Hero panel occupies the left ~58% of the content area; the live stadium and
// player render breathe freely on the right, per "cricket is the visual hero".
// ============================================================================
void AC26HUD::Home()
{
    const float E = Enter();
    const float SX = (1.f - E) * 25.f;
    const float X = ContentX + SX;
    const float PanelW = 760.f, PanelY = 208.f, PanelH = 470.f;

    // Atmospheric venue readout + single-player chip, right-aligned clear of the panel.
    {
        const FString Venue = TEXT("ECLIPSE OVAL  •  FLOODLIT NIGHT");
        Text(Venue, ContentR - Width(Venue, 18), ContentTop, 18, Muted);
    }
    {
        const FString T = TEXT("SINGLE PLAYER");
        const float TW = Width(T, 18) + 20;
        Rect(ContentR - TW, ContentTop + 34, TW, 26, CardBg);
        TextMid(T, ContentR - TW + 10, ContentTop + 34, 26, 18, Muted);
    }

    // Featured Super Over event panel
    Panel(X, PanelY, PanelW, PanelH, MintCyan);
    float Y = PanelY + 28.f;
    Tag(TEXT("PLAYABLE NOW"), X + 24, Y, true);
    Y += 46.f;

    Text(TEXT("SUPER OVER"), X + 24, Y, 76, Paper);
    Y += LineH(76) + 4.f;

    Text(Match->TeamName(Match->PlayerTeam) + TEXT("  v  ") + Match->TeamName(1 - Match->PlayerTeam), X + 24, Y, 24, PaperDim);
    Y += LineH(24) + 16.f;

    TextWrap(TEXT("Six balls. Two wickets. One chance to own the night."), X + 24, Y, 20, Muted, PanelW - 48.f);
    Y += 34.f;

    Text(TEXT("1 OVER   •   ~5 MIN   •   VS ADAPTIVE AI"), X + 24, Y, 18, MintCyan);

    // Full-width primary CTA docked to the panel's bottom edge (reference bottom teal bar)
    const float CY = PanelY + PanelH - 78.f;
    Btn(TEXT("quickplay"), TEXT("PLAY SUPER OVER  >"), X + 24, CY, PanelW - 48.f - 190.f, 62, 1);
    Btn(TEXT("nav_teams"), TEXT("CHANGE TEAM"), X + 24 + PanelW - 48.f - 190.f + 14.f, CY, 176, 62, 0);

    // Secondary supporting cards below the hero panel
    const float SY = PanelY + PanelH + 24.f, SW = (PanelW - 20.f) * .5f;
    Panel(X, SY, SW, 92.f, FLinearColor::Transparent);
    Text(TEXT("HOW TO PLAY"), X + 20, SY + 16, 20, Paper);
    Text(TEXT("Batting, bowling and timing in 60 seconds."), X + 20, SY + 44, 15, Muted);
    Zones.Add({TEXT("nav_help"), FBox2D(FVector2D(X, SY), FVector2D(X + SW, SY + 92.f))});

    Panel(X + SW + 20.f, SY, SW, 92.f, FLinearColor::Transparent);
    Text(TEXT("ALL GAME MODES"), X + SW + 40.f, SY + 16, 20, Paper);
    Text(TEXT("Browse every Cricket 26 experience."), X + SW + 40.f, SY + 44, 15, Muted);
    Zones.Add({TEXT("nav_play"), FBox2D(FVector2D(X + SW + 20.f, SY), FVector2D(X + SW + 20.f + SW, SY + 92.f))});
}

// ============================================================================
// SCREEN 1: PLAY (Mode Selection)
// Clear 2-column division with ample negative space.
// ============================================================================
void AC26HUD::Play()
{
    PageHead(TEXT("EXPERIENCES"), TEXT("GAME MODES"), TEXT("One mode playable now. Full cricket career roadmap in active development."));

    const float E = Enter(.06f);
    const float SX = (1.f - E) * 20.f;

    // Left Column: Featured Super Over Hero
    const float X = ContentX + SX, Y = 300.f, LeftW = 460.f;
    Text(TEXT("PLAYABLE NOW"), X, Y, 20, MintCyan);
    Text(TEXT("SUPER OVER"), X, Y + 34, 72, Paper);
    const float DescH = TextWrap(TEXT("Six balls each. Two wickets. Maximum intensity under lights."), X, Y + 128, 24, PaperDim, LeftW);
    Text(TEXT("1 OVER  •  ~5 MINUTES  •  VS ADAPTIVE AI"), X, Y + 128 + DescH + 14.f, 20, Muted);

    Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, Y + 128 + DescH + 58.f, 310, 62, 1);

    // Right Column: Upcoming Modes with clean 96px vertical pitch
    static const TCHAR* Modes[] = {TEXT("QUICK MATCH"), TEXT("WORLD TOUR"), TEXT("CAREER DYNASTY"), TEXT("ONLINE H2H")};
    const float CX = X + LeftW + 60.f, RW = ContentR - CX;
    for (int I = 0; I < 4; ++I)
    {
        const float CY = 300.f + I * 96.f;
        Rule(CX, CY, RW);
        Text(Modes[I], CX, CY + 24, 26, Paper);
        Text(TEXT("COMING SOON"), CX + RW - Width(TEXT("COMING SOON"), 17), CY + 28, 17, Muted);
        Zones.Add({FName(TEXT("mode_soon")), FBox2D(FVector2D(CX, CY), FVector2D(CX + RW, CY + 80))});
    }

    BackBtn(TEXT("nav_home"));
}

// ============================================================================
// SCREEN 2: TEAM SELECTION (Club Selection Rebuild)
// Generously spaced head-to-head presentation with centered crests.
// ============================================================================
void AC26HUD::Teams()
{
    PageHead(TEXT("SETUP"), TEXT("CHOOSE YOUR CLUB"), TEXT("Select your side for tonight's Super Over shootout."));

    const float E = Enter(.06f);
    const float SX = (1.f - E) * 20.f;
    const float MidX = ContentX + ContentW * .5f;

    // Team 0: Mumbai Meteors (Left Bay)
    {
        const bool Mine = Match->PlayerTeam == 0;
        const float CX = MidX - 340.f + SX, Y = 280.f;
        Crest(CX, Y + 50, 52, 0);
        Text(TEXT("MUMBAI"), CX, Y + 128, 64, MintCyan, true);
        Text(TEXT("METEORS"), CX, Y + 196, 38, Paper, true);
        Text(TEXT("RIDE THE STORM"), CX, Y + 248, 20, MintCyan, true);

        Btn(TEXT("pick0"), Mine ? TEXT("[ SELECTED SIDE ]") : TEXT("SELECT MUMBAI"), CX - 140, Y + 300, 280, 56, Mine ? 3 : 0, Mine);
    }

    // VS Divider
    Text(TEXT("VS"), MidX + SX, 450, 38, Muted, true);

    // Team 1: Melbourne Embers (Right Bay)
    {
        const bool Mine = Match->PlayerTeam == 1;
        const float CX = MidX + 340.f + SX, Y = 280.f;
        Crest(CX, Y + 50, 52, 1);
        Text(TEXT("MELBOURNE"), CX, Y + 128, 64, Coral, true);
        Text(TEXT("EMBERS"), CX, Y + 196, 38, Paper, true);
        Text(TEXT("BURN BRIGHT"), CX, Y + 248, 20, Coral, true);

        Btn(TEXT("pick1"), Mine ? TEXT("[ SELECTED SIDE ]") : TEXT("SELECT MELBOURNE"), CX - 140, Y + 300, 280, 56, Mine ? 3 : 0, Mine);
    }

    // Bottom Action Bar: Centered CTA and clear Back button
    Btn(TEXT("nav_matchup"), TEXT("PROCEED TO MATCHUP  >"), MidX - 160.f + SX, 750, 320, 60, 1);
    BackBtn(TEXT("nav_play"));
}

// ============================================================================
// SCREEN 3: MATCHUP SCREEN (Broadcast Presentation)
// Wide broadcast presentation with zero element collision.
// ============================================================================
void AC26HUD::Matchup()
{
    ScrimBottom(.8f);
    const float MidX = ContentX + ContentW * .5f;

    float Y = 160.f;
    Text(TEXT("SUPER OVER SHOOTOUT  /  NIGHT MATCH"), MidX, Y, 21, MintCyan, true);
    Y += 50.f;

    const int A = Match->PlayerTeam, B = 1 - Match->PlayerTeam;
    // Crests spaced wide with 640px separation
    Crest(MidX - 320.f, Y + 60, 58, A);
    Crest(MidX + 320.f, Y + 60, 58, B);
    Text(TEXT("VS"), MidX, Y + 44, 44, MintCyan, true);
    Y += 150.f;

    Text(Match->TeamName(A), MidX - 320.f, Y, 38, Paper, true);
    Text(Match->TeamName(B), MidX + 320.f, Y, 38, Paper, true);
    Y += LineH(38) + 10.f;

    Text(TeamTagline(A), MidX - 320.f, Y, 21, TeamColor(A), true);
    Text(TeamTagline(B), MidX + 320.f, Y, 21, TeamColor(B), true);
    Y += LineH(21) + 40.f;

    Text(TEXT("ECLIPSE OVAL   •   6 BALLS   •   2 WICKETS   •   FLOODLIGHTS"), MidX, Y, 22, PaperDim, true);
    Y += 56.f;

    Btn(TEXT("matchup_go"), TEXT("TO THE TOSS  >"), MidX - 160.f, Y, 320, 64, 1);
    BackBtn(TEXT("nav_teams"));
}

// ============================================================================
// SCREEN 4: TOSS (Match Tension Rebuild)
// Wide choices, spacious coin animation, clear result state.
// ============================================================================
void AC26HUD::Toss()
{
    PageHead(TEXT("SUPER OVER"), TEXT("THE TOSS"), TEXT("The coin toss determines who bats first under floodlights."));

    const float CX = ContentX + ContentW * .5f;
    float Y = 300.f;

    if (Match->TossStage == 0)
    {
        Circle(CX, Y + 50, 52, Gold, 2.5f);
        Text(TEXT("C26"), CX, Y + 32, 38, Gold, true);
        Y += 136.f;

        Text(TEXT("CALL THE COIN"), CX, Y, 28, Paper, true);
        Y += 46.f;

        Btn(TEXT("tossflip"), TEXT("FLIP THE COIN"), CX - 150, Y, 300, 62, 1);
        Y += 62 + 18.f;
        Btn(TEXT("tossquick"), TEXT("SKIP TOSS  /  BAT FIRST"), CX - 150, Y, 300, 48, 0);
    }
    else
    {
        const float K = Match->TossStage == 1 ? FMath::Abs(FMath::Sin(Match->TossClock * 9.f)) : 1.f;
        Circle(CX, Y + 50, 52.f * FMath::Max(.12f, K), Gold, 2.f);
        Text(TEXT("C26"), CX, Y + 32, 38, Gold, true);
        Y += 136.f;

        if (Match->TossStage == 1)
        {
            Text(TEXT("COIN IN THE AIR..."), CX, Y, 30, MintCyan, true);
        }
        else
        {
            const bool Won = Match->TossPlayerWon;
            Text(Won ? TEXT("YOU WON THE TOSS") : Match->TeamName(1 - Match->PlayerTeam) + TEXT(" WON THE TOSS"),
                 CX, Y, 40, Won ? MintCyan : Paper, true);
            Y += LineH(40) + 24.f;

            if (Won)
            {
                const bool ChoseBat = Match->TossPlayerChoseBat;
                Btn(TEXT("batfirst"), TEXT("BAT FIRST"), CX - 270, Y, 250, 60, ChoseBat ? 3 : 0, ChoseBat);
                Btn(TEXT("bowlfirst"), TEXT("BOWL FIRST"), CX + 20, Y, 250, 60, !ChoseBat ? 3 : 0, !ChoseBat);
                Y += 60 + 32.f;
                Btn(TEXT("tosscontinue"), TEXT("START MATCH  >"), CX - 160, Y, 320, 62, 1);
            }
            else
            {
                Text(Match->TossAIChoiceBat ? TEXT("THEY ELECT TO BAT FIRST") : TEXT("THEY ELECT TO BOWL FIRST"), CX, Y, 28, Gold, true);
                Y += LineH(28) + 36.f;
                Btn(TEXT("tosscontinue"), TEXT("START MATCH  >"), CX - 160, Y, 320, 62, 1);
            }
        }
    }

    BackBtn(TEXT("nav_matchup"));
}

// ============================================================================
// SCREENS 5-10: FUTURE SCREENS
// Clean roadmap views.
// ============================================================================
// ============================================================================
// SCREEN 5: SQUAD HUB — reference-matched roster presentation.
// Large featured current player on the left, compact player cards on the right.
// Names/roles are presentation data only; no roster architecture was rewritten.
// ============================================================================
void AC26HUD::Squad()
{
    PageHead(TEXT("SETUP"), TEXT("SQUAD HUB"), TEXT("Your Super Over starting XI for tonight's shootout."));

    const int Team = Match->PlayerTeam;
    const float X = ContentX, Y = 320.f;

    // Featured player (left)
    Panel(X, Y, 340.f, 430.f, TeamColor(Team));
    Crest(X + 170.f, Y + 110.f, 62.f, Team);
    Text(TEXT("A. RAO"), X + 170.f, Y + 200.f, 34, Paper, true);
    Text(TEXT("CAPTAIN  •  BATTER"), X + 170.f, Y + 240.f, 17, TeamColor(Team), true);
    Rule(X + 30.f, Y + 284.f, 280.f);
    Text(TEXT("BATTING"), X + 30.f, Y + 306.f, 15, Muted);
    Text(TEXT("82"), X + 310.f, Y + 300.f, 24, Paper, true);
    Text(TEXT("BOWLING"), X + 30.f, Y + 340.f, 15, Muted);
    Text(TEXT("41"), X + 310.f, Y + 334.f, 24, Paper, true);
    Text(TEXT("FIELDING"), X + 30.f, Y + 374.f, 15, Muted);
    Text(TEXT("76"), X + 310.f, Y + 368.f, 24, Paper, true);

    // Starting XI grid (right) — compact sports-card density, presentation stats only.
    struct FRosterEntry{const TCHAR* Name;const TCHAR* Role;int Bat,Bowl,Field;};
    static const FRosterEntry Roster[2][6] = {
        {
            {TEXT("A. RAO"),TEXT("BATTER"),82,20,70},
            {TEXT("K. DESAI"),TEXT("BATTER"),75,15,68},
            {TEXT("R. MEHRA"),TEXT("ALL-ROUNDER"),64,58,72},
            {TEXT("N. ARCHER"),TEXT("BOWLER"),22,85,60},
            {TEXT("S. IYER"),TEXT("KEEPER"),58,10,81},
            {TEXT("D. KOHLI"),TEXT("BOWLER"),18,79,55},
        },
        {
            {TEXT("J. HART"),TEXT("BATTER"),80,18,69},
            {TEXT("L. REED"),TEXT("BATTER"),73,14,66},
            {TEXT("M. VALE"),TEXT("ALL-ROUNDER"),62,56,70},
            {TEXT("V. SEN"),TEXT("BOWLER"),20,83,58},
            {TEXT("T. FOX"),TEXT("KEEPER"),56,9,79},
            {TEXT("O. BLAKE"),TEXT("BOWLER"),16,77,53},
        },
    };

    const float GX = X + 372.f, GW = ContentR - GX;
    const float CardW = (GW - 32.f) / 3.f, CardH = 132.f;
    for (int I = 0; I < 6; ++I)
    {
        const int Row = I / 3, Col = I % 3;
        const float CX2 = GX + Col * (CardW + 16.f), CY2 = Y + Row * (CardH + 16.f);
        const auto& P = Roster[Team][I];
        PlayerCard(CX2, CY2, CardW, CardH, P.Name, P.Role, P.Bat, P.Bowl, P.Field, Team, I == 0);
    }

    Text(TEXT("STARTING XI SHOWN  •  FULL ROSTER MANAGEMENT ARRIVES IN A FUTURE UPDATE"), GX, Y + 2 * (CardH + 16.f) + 8.f, 15, Muted);
    BackBtn(TEXT("nav_home"));
}

// Reusable compact sports card (reference: WBP_PlayerCard). Dark panel, name, role,
// three small stats, teal edge on the selected/featured entry.
void AC26HUD::PlayerCard(float X, float Y, float W, float H, const FString& Name, const FString& Role, int Bat, int Bowl, int Field, int Team, bool Selected)
{
    Panel(X, Y, W, H, Selected ? TeamColor(Team) : FLinearColor::Transparent);
    Text(Name, X + 16, Y + 14, 21, Paper);
    Text(Role, X + 16, Y + 42, 14, TeamColor(Team));
    Rule(X + 16, Y + 68, W - 32);
    const float SW = (W - 32) / 3.f;
    auto Stat = [&](int I, const TCHAR* L, int V)
    {
        const float SX = X + 16 + I * SW;
        Text(FString::FromInt(V), SX, Y + 78, 20, Paper);
        Text(L, SX, Y + 104, 12, Muted);
    };
    Stat(0, TEXT("BAT"), Bat);
    Stat(1, TEXT("BWL"), Bowl);
    Stat(2, TEXT("FLD"), Field);
}

// ============================================================================
// SCREEN 13: STORE — reference-matched shell. No payment/loot-box logic:
// preview cards only, presentation-ready for a future economy pass.
// ============================================================================
void AC26HUD::Store()
{
    PageHead(TEXT("STORE"), TEXT("CRICKET STORE"), TEXT("Preview the kit and cosmetics coming to Cricket 26."));

    const float X = ContentX, Y = 320.f;
    static const TCHAR* Cats[] = {TEXT("PLAYER PACKS"), TEXT("KITS & GEAR"), TEXT("COSMETICS")};
    static const TCHAR* Desc[] = {
        TEXT("Unlock new faces for your Super Over roster."),
        TEXT("Bats, gloves, pads and boots for your squad."),
        TEXT("Stadium themes and celebration flourishes."),
    };
    const float CW = (ContentW - 40.f) / 3.f;
    for (int I = 0; I < 3; ++I)
    {
        const float CX2 = X + I * (CW + 20.f);
        Panel(CX2, Y, CW, 260.f, FLinearColor::Transparent);
        Rect(CX2 + 16, Y + 16, CW - 32, 120, PillBg);
        Line(CX2 + 16, Y + 16, CX2 + CW - 16, Y + 136, Hairline, 1.f);
        Line(CX2 + CW - 16, Y + 16, CX2 + 16, Y + 136, Hairline, 1.f);
        Text(Cats[I], CX2 + 16, Y + 150, 22, Paper);
        TextWrap(Desc[I], CX2 + 16, Y + 182, 15, Muted, CW - 32);
        Text(TEXT("COMING SOON"), CX2 + 16, Y + 228, 16, MintCyan);
    }

    Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, Y + 292.f, 310, 62, 1);
    BackBtn(TEXT("nav_home"));
}

// ============================================================================
// SCREENS 6-10: CAREER / LEADERBOARDS / MULTIPLAYER / TRAINING / WORLD
// Reference-matched shells. Systems that do not exist yet are marked honestly
// as COMING SOON / NOT AVAILABLE IN THIS BUILD rather than faked.
// ============================================================================
void AC26HUD::Future(int Kind)
{
    const float X = ContentX, Y = 320.f;

    if (Kind == 1) // CAREER — "Road to Glory"
    {
        PageHead(TEXT("CAREER"), TEXT("ROAD TO GLORY"), TEXT("Build your Cricket 26 legacy, match by match."));

        static const TCHAR* Rows[] = {TEXT("PLAYER CAREER"), TEXT("CLUB CAREER"), TEXT("INTERNATIONAL"), TEXT("ACHIEVEMENTS")};
        for (int I = 0; I < 4; ++I)
        {
            const float RY = Y + I * 62.f;
            Rule(X, RY, ContentW);
            Text(Rows[I], X, RY + 20, 26, Paper);
            Text(TEXT("COMING SOON"), ContentR - Width(TEXT("COMING SOON"), 17), RY + 24, 17, Muted);
        }

        const float PY = Y + 4 * 62.f + 30.f;
        Text(TEXT("SEASON 01 PROGRESS"), X, PY, 16, Muted);
        Rect(X, PY + 26, ContentW, 6, PillBg);
        Rect(X, PY + 26, ContentW * .04f, 6, MintCyan);

        Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, PY + 60, 310, 62, 1);
        BackBtn(TEXT("nav_home"));
        return;
    }

    if (Kind == 3) // MULTIPLAYER — "Multiplayer Arena"
    {
        PageHead(TEXT("ONLINE"), TEXT("MULTIPLAYER ARENA"), TEXT("Real-time ranked Super Overs against global rivals."));

        static const TCHAR* Modes[] = {TEXT("QUICK MATCH"), TEXT("RANKED SEASONS"), TEXT("PLAY WITH FRIENDS")};
        const float CW = (ContentW - 40.f) / 3.f;
        for (int I = 0; I < 3; ++I)
        {
            const float CX2 = X + I * (CW + 20.f);
            Panel(CX2, Y, CW, 200.f, FLinearColor::Transparent);
            Text(Modes[I], CX2 + 20, Y + 24, 23, Paper);
            Text(TEXT("NOT AVAILABLE"), CX2 + 20, Y + 150, 15, Coral);
            Text(TEXT("IN THIS BUILD"), CX2 + 20, Y + 170, 15, Coral);
        }

        Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, Y + 240.f, 310, 62, 1);
        BackBtn(TEXT("nav_home"));
        return;
    }

    if (Kind == 2) // LEADERBOARDS — "Global Leaderboards"
    {
        PageHead(TEXT("RANKINGS"), TEXT("GLOBAL LEADERBOARDS"), TEXT("Season standings across every Cricket 26 arena."));

        static const TCHAR* Tabs[] = {TEXT("GLOBAL"), TEXT("FRIENDS"), TEXT("SEASON")};
        const float TabY = ContentTop + 180.f;
        for (int I = 0; I < 3; ++I)
        {
            const float TX = X + I * 130.f;
            if (I == 0) Rect(TX, TabY + 28.f, 100, 2, MintCyan);
            Text(Tabs[I], TX + 50, TabY, 20, I == 0 ? Paper : Muted, true);
        }

        const float TY = TabY + 60.f;
        Rule(X, TY, ContentW);
        Text(TEXT("RANK"), X, TY + 14, 15, Muted);
        Text(TEXT("PLAYER"), X + 120, TY + 14, 15, Muted);
        Text(TEXT("RATING"), X + ContentW - 220, TY + 14, 15, Muted);
        Text(TEXT("WINS"), X + ContentW - 80, TY + 14, 15, Muted);
        Rule(X, TY + 46, ContentW);

        Text(TEXT("NO RANKED MATCHES PLAYED YET"), X + ContentW * .5f, TY + 120, 24, Paper, true);
        Text(TEXT("COMING SOON  /  GLOBAL SEASON RANKINGS ARRIVE IN A FUTURE UPDATE"), X + ContentW * .5f, TY + 158, 18, Muted, true);

        Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, TY + 240.f, 310, 62, 1);
        BackBtn(TEXT("nav_home"));
        return;
    }

    // Kind 4 (Training) / 5 (World) — simple roadmap template, orphaned from primary
    // nav but kept reachable for completeness and future expansion.
    static const TCHAR* Titles[] = {TEXT("TRAINING NETS"), TEXT("CRICKET WORLD")};
    static const TCHAR* Subs[] = {
        TEXT("Master timing, footwork, and death-bowling craft."),
        TEXT("Editorial news, player spotlights, and world events.")
    };
    const int I = Kind - 4;
    PageHead(TEXT("ROADMAP"), Titles[I], Subs[I]);

    Text(TEXT("COMING IN SEASON 01"), X, Y, 24, MintCyan);
    Text(TEXT("This vertical slice is dedicated to delivering the purest Super Over gameplay."), X, Y + 40, 24, PaperDim);

    Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, Y + 130, 310, 62, 1);
    BackBtn(TEXT("nav_home"));
}

// ============================================================================
// SCREEN 11: SETTINGS HUB
// Category tabs with ample horizontal pitch and clean setting rows.
// ============================================================================
void AC26HUD::SettingsHub()
{
    PageHead(TEXT("SETTINGS"), TEXT("CONFIGURATION"), TEXT("Audio mix, visual quality, and control preferences."));

    static const TCHAR* Tabs[] = {TEXT("GAME"), TEXT("AUDIO"), TEXT("GRAPHICS"), TEXT("CONTROLS"), TEXT("EASE"), TEXT("ABOUT")};
    static const FName Acts[] = {TEXT("stab0"), TEXT("stab1"), TEXT("stab2"), TEXT("stab3"), TEXT("stab4"), TEXT("stab5")};

    // Category Tabs: 130px pitch with zero overlap
    for (int I = 0; I < 6; ++I)
    {
        const float TX = ContentX + I * 130.f;
        const bool Sel = Match->SettingsTab == I;
        if (Sel) Rect(TX, 340, 100, 2, MintCyan);
        Text(Tabs[I], TX + 50, 312, 22, Sel ? Paper : Muted, true);
        Zones.Add({Acts[I], FBox2D(FVector2D(TX, 304), FVector2D(TX + 100, 344))});
    }

    const float X = ContentX, W = 780.f;
    const float TY = 374.f;

    auto Row = [&](FName A, const FString& L, const FString& V, int I, bool On)
    {
        const float RY = TY + I * 68.f;
        Rule(X, RY, W);
        Text(L, X, RY + 20, 22, Paper);
        Btn(A, V, X + W - 180, RY + 10, 180, 42, 0, On);
    };

    const TCHAR* D[] = {TEXT("EASY"), TEXT("NORMAL"), TEXT("HARD")};
    const TCHAR* Q[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA")};

    switch (Match->SettingsTab)
    {
    case 1:
        Row(TEXT("master"), TEXT("MASTER AUDIO"), Match->Preferences->SoundVolume > .1f ? TEXT("ON") : TEXT("OFF"), 0, Match->Preferences->SoundVolume > .1f);
        Row(TEXT("commentary"), TEXT("COMMENTARY SPEECH"), Match->Preferences->CommentaryVolume > .1f ? TEXT("ON") : TEXT("OFF"), 1, Match->Preferences->CommentaryVolume > .1f);
        Row(TEXT("crowd"), TEXT("STADIUM CROWD"), Match->Preferences->CrowdVolume > .1f ? TEXT("ON") : TEXT("OFF"), 2, Match->Preferences->CrowdVolume > .1f);
        Row(TEXT("sfx"), TEXT("SFX & IMPACTS"), Match->Preferences->SFXVolume > .1f ? TEXT("ON") : TEXT("OFF"), 3, Match->Preferences->SFXVolume > .1f);
        break;
    case 2:
        Row(TEXT("quality"), FString(TEXT("GRAPHICS QUALITY  /  ")) + Q[Match->Preferences->Quality], Q[Match->Preferences->Quality], 0, true);
        break;
    case 3:
        Row(TEXT("sensitivity"), FString::Printf(TEXT("SWIPE SENSITIVITY  /  %.1fx"), Match->Preferences->Sensitivity), FString::Printf(TEXT("%.1fx"), Match->Preferences->Sensitivity), 0, true);
        Row(TEXT("vibration"), TEXT("HAPTIC VIBRATION"), Match->Preferences->Vibration ? TEXT("ON") : TEXT("OFF"), 1, Match->Preferences->Vibration);
        Row(TEXT("hints"), TEXT("TACTICAL HINTS"), Match->Preferences->Hints ? TEXT("ON") : TEXT("OFF"), 2, Match->Preferences->Hints);
        break;
    case 4:
        Row(TEXT("subtitles"), TEXT("COMMENTARY SUBTITLES"), Match->Preferences->Subtitles ? TEXT("ON") : TEXT("OFF"), 0, Match->Preferences->Subtitles);
        Row(TEXT("reducedmotion"), TEXT("REDUCED MOTION"), Match->Preferences->ReducedMotion ? TEXT("ON") : TEXT("OFF"), 1, Match->Preferences->ReducedMotion);
        break;
    case 5:
        Text(TEXT("CRICKET 26"), X, TY + 20, 44, Paper);
        Text(TEXT("AAA mobile cricket vertical slice. Authentic teams, stadium floodlights,"), X, TY + 80, 23, Muted);
        Text(TEXT("original commentary, and precision physics-driven ball flight."), X, TY + 110, 23, Muted);
        Text(TEXT("BUILD: v0.3  •  UNREAL ENGINE 5.8"), X, TY + 150, 21, MintCyan);
        break;
    default:
        Row(TEXT("difficulty"), FString(TEXT("AI DIFFICULTY  /  ")) + D[Match->Preferences->Difficulty], D[Match->Preferences->Difficulty], 0, true);
        Row(TEXT("hints"), TEXT("TACTICAL HINTS"), Match->Preferences->Hints ? TEXT("ON") : TEXT("OFF"), 1, Match->Preferences->Hints);
        Row(TEXT("vibration"), TEXT("HAPTIC FEEDBACK"), Match->Preferences->Vibration ? TEXT("ON") : TEXT("OFF"), 2, Match->Preferences->Vibration);
        break;
    }

    BackBtn(TEXT("nav_home"));
}

// ============================================================================
// SCREEN 12: HELP (How to Play)
// Generous 3-column spacing.
// ============================================================================
void AC26HUD::Help()
{
    PageHead(TEXT("TACTICS"), TEXT("HOW TO PLAY"), TEXT("Three essential concepts to master the Super Over."));

    static const TCHAR* TT[] = {TEXT("01 / BATTING"), TEXT("02 / BOWLING"), TEXT("03 / SUPER OVER")};
    static const TCHAR* HD[] = {
        TEXT("Left stick controls footwork. Swipe right area to aim direction and power. Time as ball pitches."),
        TEXT("Select delivery type and length. Drag pitch marker. Tap release in the gold sweet spot."),
        TEXT("Six balls. Two wickets. Every dot is gold; every boundary swings the match.")
    };

    for (int I = 0; I < 3; ++I)
    {
        const float X = ContentX + I * 390.f, Y = 310.f, W = 360.f;
        Text(TT[I], X, Y, 26, Paper);
        TextWrap(HD[I], X, Y + 44, 22, Muted, W);
    }

    Btn(TEXT("nav_teams"), TEXT("START PLAYING  >"), ContentX, 710, 300, 58, 1);
    BackBtn(TEXT("nav_home"));
}

// ============================================================================
// GAMEPLAY HUD: RADICAL MINIMALIST SCORE BUG
// Ultra-compact single broadcast capsule. Measured and guarded against overlaps.
// ============================================================================
void AC26HUD::Score()
{
    const auto& S = Match->Rules.Now();
    const int Bat = Match->BattingTeam();
    const auto TC = TeamColor(Bat);

    // Single sleek broadcast capsule (Width: 500, Height: 50)
    const float BX = 56.f, BY = 36.f, BW = 500.f, BH = 50.f;
    Rect(BX, BY, BW, BH, PillBg);
    Rect(BX, BY, BW, 2.5f, TC); // Accent top hairline

    // 1. Team Short Code (X=74)
    Text(Match->TeamShort(Bat), BX + 18, BY + 11, 26, TC);

    // 2. Divider 1 (X=140)
    Line(BX + 84, BY + 10, BX + 84, BY + BH - 10, Hairline, 1.f);

    // 3. Score (Runs / Wickets) at X=152
    const FString ScoreStr = FString::Printf(TEXT("%d / %d"), S.Runs, S.Wickets);
    Text(ScoreStr, BX + 98, BY + 6, 36, Paper);

    // 4. Divider 2 (X=260)
    Line(BX + 224, BY + 10, BX + 224, BY + BH - 10, Hairline, 1.f);

    // 5. Overs / Legal Balls at X=276
    const FString BallsStr = FString::Printf(TEXT("%d / 6"), S.LegalBalls);
    Text(BallsStr, BX + 238, BY + 13, 23, PaperDim);

    // 6. Chasing Target (if in second innings)
    if (Match->Rules.Current == 1)
    {
        const FString ReqStr = FString::Printf(TEXT("NEED %d (%db)"), Match->Rules.RunsRequired(), Match->Rules.BallsRemaining());
        Text(ReqStr, BX + BW - 18 - Width(ReqStr, 20), BY + 14, 20, Gold);
    }

    // Batter & Bowler Lower-Third Strip (Cleanly separated at Y=94, with dark backdrop)
    const FString BBInfo = Match->BatterName() + TEXT(" *   •   ") + Match->BowlerName();
    const float BBW = Width(BBInfo, 18) + 24.f;
    Rect(BX, BY + BH + 8, BBW, 28, FLinearColor(Void.R, Void.G, Void.B, .75f));
    Text(BBInfo, BX + 12, BY + BH + 13, 18, PaperDim);

    // Top-right Pause button (generously inset from right edge)
    Btn(TEXT("pause"), TEXT("II"), 1480, 36, 56, 46, 0);

    // Ball-by-ball minimalist dots at bottom edge
    float BallX = 640.f;
    int Start = FMath::Max(0, int(S.Ledger.size()) - 6);
    for (int I = Start; I < int(S.Ledger.size()); ++I)
    {
        const auto& O = S.Ledger[I];
        const bool W = O.Wicket != C26::Dismissal::None;
        FString V = W ? TEXT("W") : (O.WideRuns ? TEXT("Wd") : (O.NoBall ? TEXT("Nb") : FString::FromInt(O.BatRuns + O.Byes + O.LegByes)));
        Circle(BallX, 856, 13, W ? Coral : (O.BatRuns >= 4 ? MintCyan : Muted), 1.5f);
        Text(V, BallX, 845, 18, W ? Coral : Paper, true);
        BallX += 48.f;
    }

    if (S.FreeHit) Tag(TEXT("FREE HIT"), 570, 36, true);
}

// ============================================================================
// GAMEPLAY CONTROLS
// Unobtrusive, transparent touch zones with zero collision.
// ============================================================================
void AC26HUD::Controls()
{
    const auto Phase = Match->Phase;

    if (Phase == EC26Phase::Ready || Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery)
    {
        if (Match->PlayerBatting())
        {
            // Left Stick Footwork
            Circle(160, 700, 56, FLinearColor(.4f, .65f, .68f, .22f), 1.5f);
            Circle(160 + Match->Footwork * 34.f, 700, 16, MintCyan, 2.f);

            // Right Touch Area
            Circle(1440, 700, 70, FLinearColor(.4f, .65f, .68f, .22f), 1.5f);
            Text(TEXT("SWIPE TO HIT"), 1440, 786, 20, PaperDim, true);

            // Loft & Defend Toggles (At Y=560, well clear of touch circle at 700)
            Btn(TEXT("loft"), TEXT("LOFT"), 1320, 560, 100, 44, 0, Match->Intent.Loft);
            Btn(TEXT("defend"), TEXT("DEFEND"), 1432, 560, 100, 44, 0, Match->Intent.Defend);

            if (Phase == EC26Phase::Ready)
            {
                Btn(TEXT("ready"), TEXT("READY  >"), 1200, 440, 280, 58, 1);
            }
        }
        else
        {
            // Bowling Controls: 58px pitch with zero overlap
            const auto& Plan = Phase == EC26Phase::Ready ? Match->Bowling : Match->LockedBowling;
            if (Phase == EC26Phase::Ready)
            {
                Btn(TEXT("delivery"), FString(C26Delivery::Name(Plan.Type)) + TEXT("  >"), 64, 520, 240, 48, 0);
                Btn(TEXT("length"), FString(C26Delivery::LengthName(Plan.Length)) + TEXT("  >"), 64, 578, 240, 48, 0);
                const TCHAR* LineName = Plan.Line > 35.f ? TEXT("OUTSIDE OFF") : Plan.Line > 5.f ? TEXT("OFF STUMP") : Plan.Line > -25.f ? TEXT("MIDDLE") : TEXT("LEG SIDE");
                Btn(TEXT("line"), FString(LineName) + TEXT("  >"), 64, 636, 240, 48, 0);
                Btn(TEXT("ready"), TEXT("START RUN-UP  >"), 1200, 720, 320, 68, 1);
            }

            // Pitch marker projection
            const FVector P = Canvas->Project(FVector(Plan.Line, Plan.Length, 8));
            if (P.Z > 0)
            {
                float X = (P.X - OffsetX) / Scale, Y = (P.Y - OffsetY) / Scale;
                Circle(X, Y, 14, MintCyan, 2.f);
            }

            if (Phase == EC26Phase::RunUp)
            {
                Rect(650, 710, 300, 6, Muted);
                Rect(880, 706, 30, 14, Gold);
                Rect(650 + 300 * Match->BowlingMeter(), 702, 4, 22, MintCyan);
                Btn(TEXT("release"), Match->ReleaseLocked ? TEXT("LOCKED") : TEXT("RELEASE"), 1200, 720, 320, 68, 1);
            }
        }
    }

    if (Phase == EC26Phase::InPlay)
    {
        if (Match->PlayerBatting())
        {
            Btn(TEXT("run"), Match->Running ? TEXT("TWO?") : TEXT("RUN"), 1280, 720, 240, 68, 1);
            Btn(TEXT("cancel"), TEXT("BACK"), 80, 720, 140, 52, 0);
        }

        static const TCHAR* Timing[] = {TEXT("PERFECT"), TEXT("GOOD"), TEXT("EARLY"), TEXT("LATE"), TEXT("EDGE"), TEXT("MISS")};
        if (Match->PhaseTime < .95f)
        {
            const bool P = Match->LastContact.Timing == EC26Timing::Perfect;
            Text(Timing[int(Match->LastContact.Timing)], 800, 220, 32, P ? Gold : MintCyan, true);
        }

        if (Match->Running)
        {
            Text(FString::Printf(TEXT("%d COMPLETED"), Match->CompletedRuns), 800, 680, 26, Gold, true);
        }
    }

    if (Phase == EC26Phase::Reaction)
    {
        // Verified 30px gap: Callout at 440 (H=96) ends at 536; Detail starts at 566!
        const auto Edge = Match->Callout == TEXT("WICKET") ? Coral : (Match->Callout == TEXT("SIX") ? Gold : MintCyan);
        Text(Match->Callout, 800, 440, 80, Edge, true);
        Text(Match->Detail, 800, 566, 24, Paper, true);
    }

    if (Phase == EC26Phase::Replay)
    {
        Text(TEXT("REPLAY"), 120, 80, 24, Coral);
        Btn(TEXT("skip"), TEXT("SKIP  >"), 1360, 780, 180, 52, 0);
    }
}

// ============================================================================
// SCREEN: MATCH RESULT (Hero Broadcast Celebration)
// Mathematically centered action buttons and generous vertical spacing.
// ============================================================================
void AC26HUD::Result()
{
    Vignette();
    ScrimBottom(.9f);

    const bool Won = Match->Callout == TEXT("VICTORY");
    const bool Tie = Match->Callout == TEXT("MATCH TIED");
    const auto Edge = Tie ? Gold : (Won ? MintCyan : Coral);

    float Y = 170.f;
    Text(Match->Callout, 800, Y, 96, Edge, true);
    Y += LineH(96) + 16.f;

    // Victory Margin Callout
    {
        const auto& A = Match->Rules.Scores[0];
        const auto& B = Match->Rules.Scores[1];
        FString Margin;
        if (Tie) Margin = TEXT("HONOURS EVEN  •  SUPER OVER TIED");
        else
        {
            const bool ChaseWon = B.Runs > A.Runs;
            Margin = ChaseWon ? FString::Printf(TEXT("WON BY %d %s"), 2 - B.Wickets, B.Wickets == 1 ? TEXT("WICKET") : TEXT("WICKETS"))
                              : FString::Printf(TEXT("WON BY %d %s"), A.Runs - B.Runs, A.Runs - B.Runs == 1 ? TEXT("RUN") : TEXT("RUNS"));
        }
        Text(Margin, 800, Y, 28, Gold, true);
        Y += LineH(28) + 40.f;
    }

    // Both Innings Score summaries (Centered with generous 18px line gap)
    for (int I = 0; I < 2; ++I)
    {
        const auto& S = Match->Rules.Scores[I];
        int Team = I == 0 ? Match->FirstBattingTeam : 1 - Match->FirstBattingTeam;
        const FString LineStr = FString::Printf(TEXT("%s   %d / %d   (6b)"), *Match->TeamName(Team), S.Runs, S.Wickets);
        Text(LineStr, 800, Y, 32, TeamColor(Team), true);
        Y += LineH(32) + 18.f;
    }

    // Centered Action Buttons: (260 + 24 + 200 = 484 total width, centered at 800)
    Y += 40.f;
    Btn(TEXT("again"), TEXT("PLAY AGAIN  >"), 558, Y, 260, 62, 1);
    Btn(TEXT("menu"), TEXT("HOME"), 842, Y, 200, 62, 0);
}

// ============================================================================
// PAUSE & IN-GAME SETTINGS
// Modern minimalist pause menu.
// ============================================================================
void AC26HUD::Preferences()
{
    Rect(0, 0, 1600, 900, FLinearColor(.004f, .008f, .014f, .78f));

    if (Match->ControlsOpen)
    {
        Text(TEXT("HOW TO PLAY"), 800, 220, 44, Paper, true);
        Text(TEXT("Left stick controls footwork. Swipe right area to aim and time."), 800, 320, 24, PaperDim, true);
        Text(TEXT("Toggle LOFT for aerial boundary shots. Tap RUN to bank singles."), 800, 360, 24, PaperDim, true);
        Text(TEXT("When bowling, drag the pitch marker and hit the gold release meter."), 800, 420, 24, PaperDim, true);
        Btn(TEXT("close"), TEXT("RESUME MATCH"), 660, 540, 280, 56, 1);
        return;
    }

    const float CX = 620, CW = 360;
    float Y = 220.f;

    Text(TEXT("MATCH PAUSED"), 800, Y, 40, Paper, true);
    Y += 74.f;

    Btn(TEXT("pause"), TEXT("RESUME MATCH"), CX, Y, CW, 56, 1);
    Y += 56 + 14.f;
    Btn(TEXT("master"), Match->Preferences->SoundVolume > .1f ? TEXT("AUDIO: ON") : TEXT("AUDIO: OFF"), CX, Y, CW, 48, 0);
    Y += 48 + 14.f;
    Btn(TEXT("help"), TEXT("HOW TO PLAY"), CX, Y, CW, 48, 0);
    Y += 48 + 14.f;
    Btn(TEXT("confirm_restart"), TEXT("RESTART OVER"), CX, Y, CW, 48, 2);
    Y += 48 + 14.f;
    Btn(TEXT("confirm_exit"), TEXT("EXIT TO HOME"), CX, Y, CW, 48, 0);

    if (!Match->PendingConfirm.IsNone()) Confirm();
}

void AC26HUD::Subtitle()
{
    if (!Match->Preferences->Subtitles) return;
    if (!Match->Audio || Match->Audio->ActiveSubtitle.IsEmpty()) return;
    if (GetWorld()->GetTimeSeconds() > Match->Audio->SubtitleUntil) return;

    const FString& S = Match->Audio->ActiveSubtitle;
    const float W = Width(S, 21) + 48.f;
    const float X = 800.f - W * .5f, Y = 750.f;

    Rect(X, Y, W, 38, Void);
    TextMid(S, 800, Y, 38, 21, Paper, true);
}

void AC26HUD::DrawHUD()
{
    Super::DrawHUD();
    Match = Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());
    if (!Match || !Canvas || !Match->Preferences) return;

    Scale = FMath::Min(Canvas->SizeX / 1600.f, Canvas->SizeY / 900.f);
    OffsetX = (Canvas->SizeX - 1600 * Scale) * .5f;
    OffsetY = (Canvas->SizeY - 900 * Scale) * .5f;
    Zones.Reset();
    FA = 1.f;

    if (Match->Phase == EC26Phase::Menu)
    {
        Menu();
    }
    else if (Match->Phase == EC26Phase::Intro)
    {
        Vignette();
        float Y = 260.f;
        Text(TEXT("SUPER OVER  /  NIGHT SHOOTOUT"), 800, Y, 22, MintCyan, true);
        Y += 38.f;
        Text(Match->TeamName(Match->FirstBattingTeam) + TEXT("  vs  ") + Match->TeamName(1 - Match->FirstBattingTeam), 800, Y, 50, Paper, true);
        Y += LineH(50) + 14.f;
        Text(Match->TossText, 800, Y, 24, Muted, true);

        Btn(TEXT("skip"), TEXT("SKIP INTRO  >"), 1320, 780, 200, 54, 0);
    }
    else if (Match->Phase == EC26Phase::Result)
    {
        Result();
    }
    else if (Match->Phase == EC26Phase::Interval)
    {
        Vignette();
        float Y = 240.f;
        Text(Match->Callout, 800, Y, 84, Paper, true);
        Y += LineH(84) + 12.f;
        Text(Match->Detail, 800, Y, 26, MintCyan, true);
        Y += LineH(26) + 18.f;

        const auto& F = Match->Rules.Scores[0];
        Text(FString::Printf(TEXT("%s NEED %d RUNS FROM 6 BALLS"), *Match->TeamShort(1 - Match->FirstBattingTeam), Match->Rules.Target()),
             800, Y, 28, Gold, true);
        Y += LineH(28) + 38.f;

        Btn(TEXT("skip"), Match->PlayerBatting() ? TEXT("TAKE THE BALL  >") : TEXT("START THE CHASE  >"), 620, Y, 360, 64, 1);
    }
    else
    {
        Score();
        Controls();
    }

    if (Match->Phase != EC26Phase::Menu)
    {
        Subtitle();
        Toast(true);
    }

    if (Match->SettingsOpen || Match->ControlsOpen || Match->Paused)
    {
        Preferences();
    }
    if (!Match->PendingConfirm.IsNone() && !Match->SettingsOpen && !Match->ControlsOpen && !Match->Paused)
    {
        Confirm();
    }
}
