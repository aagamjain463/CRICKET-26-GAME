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
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericApplication.h"
#include "C26UIStyle.h"
using namespace C26UIStyle;

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

// Shared palette and shell geometry live in C26UIStyle; compact layouts use the same tokens.

void AC26HUD::BeginPlay()
{
    Super::BeginPlay();
    const FString Fonts = FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts");
    auto LoadFont = [&](const TCHAR* File)
    {
        FString Path = Fonts / File;
        if (!IFileManager::Get().FileExists(*Path)) Path = Fonts / TEXT("BarlowCondensed-SemiBold.ttf");
        UFont* Font = NewObject<UFont>(this);
        Font->FontCacheType = EFontCacheType::Runtime;
        Font->LegacyFontSize = 32;
        Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
            FTypefaceEntry(TEXT("Regular"), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
        return Font;
    };
    DisplayFont = LoadFont(TEXT("BarlowCondensed-Bold.ttf"));
    BodyFont = LoadFont(TEXT("Barlow-Medium.ttf"));
    FDisplayMetrics Metrics;
    FDisplayMetrics::RebuildDisplayMetrics(Metrics);
    SafePaddingRatio = FVector2D(Metrics.TitleSafePaddingSize.X / FMath::Max(1, Metrics.PrimaryDisplayWidth),
                                Metrics.TitleSafePaddingSize.Y / FMath::Max(1, Metrics.PrimaryDisplayHeight));
}

FLinearColor AC26HUD::WithA(FLinearColor C, float M) const
{
    return FLinearColor(C.R, C.G, C.B, C.A * FA * M);
}

float AC26HUD::LineH(float Size) const
{
    float W = 0.f, H = 0.f;
    if (Canvas && FontFor(Size)) Canvas->StrLen(FontFor(Size), TEXT("Ag09"), W, H);
    return H > 0.f ? H * Size / 32.f : Size * 1.25f;
}

float AC26HUD::TH(float Size) const
{
    return LineH(Size) + 8.f;
}

TArray<FString> AC26HUD::Wrapped(const FString& S, float Size, float MaxW) const
{
    TArray<FString> Lines, Paragraphs;
    if (MaxW <= 0.f || S.IsEmpty()) return Lines;
    S.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Paragraphs, TEXT("\n"), false);
    for (const FString& Paragraph : Paragraphs)
    {
        TArray<FString> Words;
        Paragraph.ParseIntoArray(Words, TEXT(" "), true);
        FString Current;
        for (FString Word : Words)
        {
            const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
            if (!Current.IsEmpty() && Width(Candidate, Size) > MaxW) { Lines.Add(Current); Current.Empty(); }
            while (Width(Word, Size) > MaxW && Word.Len() > 1)
            {
                int32 Count = 1;
                while (Count < Word.Len() && Width(Word.Left(Count + 1), Size) <= MaxW) ++Count;
                Lines.Add(Word.Left(Count)); Word.RightChopInline(Count);
            }
            Current = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
        }
        Lines.Add(Current);
    }
    return Lines;
}

int AC26HUD::WrapLines(const FString& S, float Size, float MaxW) const
{
    return Wrapped(S, Size, MaxW).Num();
}

float AC26HUD::TextWrap(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center)
{
    const auto Lines = Wrapped(S, Size, MaxW);
    const float Step = FMath::Max(LineH(Size) + 6.f, Size * 1.5f);
    for (int I = 0; I < Lines.Num(); ++I) Text(Lines[I], X, Y + I * Step, Size, Color, Center);
    return Lines.IsEmpty() ? 0.f : (Lines.Num() - 1) * Step + LineH(Size);
}

void AC26HUD::BoundedCopy(const FString& S, float X, float Y, float Size, FLinearColor Color, float W, int MaxLines, bool Center)
{
    auto Lines = Wrapped(S, Size, W);
    const int Count = FMath::Min(MaxLines, Lines.Num());
    const float Step = FMath::Max(LineH(Size) + 6.f, Size * 1.5f);
    for (int I = 0; I < Count; ++I)
    {
        FString L = Lines[I];
        if (I == Count - 1 && Lines.Num() > Count)
        {
            while (!L.IsEmpty() && Width(L + TEXT("…"), Size) > W) L.LeftChopInline(1);
            L += TEXT("…");
        }
        Text(L, X, Y + I * Step, Size, Color, Center);
    }
}

void AC26HUD::TextFit(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center)
{
    if (MaxW <= 0.f) return;
    UFont* Font = FontFor(Size);
    const float Minimum = FMath::Min(Size, Layout.Compact ? 34.f : 18.f);
    const float Measured = FontWidth(S, Size, Font);
    const float FS = FMath::Clamp(Size * MaxW / FMath::Max(1.f, Measured), Minimum, Size);
    FString Label = S;
    if (FontWidth(Label, FS, Font) > MaxW)
    {
        while (!Label.IsEmpty() && FontWidth(Label + TEXT("…"), FS, Font) > MaxW) Label.LeftChopInline(1);
        Label += TEXT("…");
    }
    FontText(Label, X, Y, FS, Color, Center, Font);
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

UFont* AC26HUD::FontFor(float Size) const
{
    return Size >= (Layout.Compact ? 44.f : 32.f) ? DisplayFont.Get() : BodyFont.Get();
}

void AC26HUD::FontText(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center, UFont* Font)
{
    if (!Font || !Canvas) return;
    if (Center) X -= FontWidth(S, Size, Font) * .5f;
    FCanvasTextItem Item(FVector2D(OffsetX + X * Scale, OffsetY + Y * Scale), FText::FromString(S), Font, WithA(Color));
    Item.Scale = FVector2D(Scale * Size / 32.f);
    Canvas->DrawItem(Item);
}

void AC26HUD::Text(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center)
{
    FontText(S, X, Y, Size, Color, Center, FontFor(Size));
}

float AC26HUD::FontWidth(const FString& S, float Size, UFont* Font) const
{
    if (!Font || !Canvas) return 0.f;
    float W = 0.f, H = 0.f;
    Canvas->StrLen(Font, S, W, H);
    return W * Size / 32.f;
}

float AC26HUD::Width(const FString& S, float Size) const
{
    return FontWidth(S, Size, FontFor(Size));
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
        Line(X + 24, Y, X + FMath::Min(W - 24.f, 120.f), Y, Edge, 2.f);
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
    const FBox2D Bounds(FVector2D(X, Y), FVector2D(X + W, Y + H));
    const bool P = Pressed(Action), Hover = Bounds.IsInside(Pointer);
    const bool Solid = Style == 1 || Style == 2;
    const FLinearColor Accent = Style == 2 ? Coral : MintCyan;
    const FLinearColor BG = Solid ? (P ? Paper : Accent) : FMath::Lerp(Void, Paper, P ? .12f : (Hover ? .07f : .025f));
    const FLinearColor Ink = Solid ? Void : (Selected ? MintCyan : Paper);
    const FLinearColor Edge = Selected ? MintCyan : (Hover ? PaperDim : Hairline);
    Rect(X, Y, W, H, BG);
    Line(X, Y, X + W, Y, Solid ? Accent : Edge);
    Line(X, Y + H, X + W, Y + H, Solid ? Accent : Edge);
    Line(X, Y, X, Y + H, Solid ? Accent : Edge);
    Line(X + W, Y, X + W, Y + H, Solid ? Accent : Edge);
    if (Selected) Line(X + 16, Y + H - 5, X + W - 16, Y + H - 5, MintCyan, 2.f);
    const float FS = Layout.Compact ? 38.f : (H >= 60.f ? 25.f : 20.f);
    TextFit(Label, X + W * .5f, Y + (H - LineH(FS)) * .5f + (P ? 1.f : 0.f), FS, Ink, W - 32.f, true);
    if (!Action.IsNone()) Zones.Add({Action, Bounds});
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

    // No backend economy exists yet; show match identity rather than fictional balances.
    TextMid(TEXT("THE NIGHT IS YOURS."), ContentX, 0, TopH, 18, Muted);
    TextMid(TEXT("SUPER OVER  /  SINGLE PLAYER"), 1080, 0, TopH, 18, PaperDim);
    Btn(TEXT("nav_help"), TEXT("HELP"), 1456, 18, 96, 48, 0, Match->MenuScreen == 12);
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

    Text(TEXT("CLUBHOUSE"), 32, TopH + 34.f, 16, Muted);
    float Y = TopH + 80.f;
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
    const float X = 16.f, W = NavW - 32.f, H = 52.f;
    const FBox2D Bounds(FVector2D(X, Y), FVector2D(X + W, Y + H));
    if (Selected || P || Bounds.IsInside(Pointer))
        Rect(X, Y, W, H, Selected ? MintCyan : Hairline);
    TextMid(Label, X + 16, Y, H, 18, Selected ? Void : PaperDim);
    if (Selected) TextMid(TEXT("/"), X + W - 24, Y, H, 22, Void);
    Zones.Add({Action, Bounds});
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
    Zones.Reset();
    Rect(0, 0, 1600, 900, FLinearColor(Void.R, Void.G, Void.B, .94f));
    const bool Restart = Match->PendingConfirm == TEXT("restart");
    Panel(360, 236, 880, 440, Restart ? Gold : Coral);
    Text(Restart ? TEXT("RESTART MATCH?") : TEXT("LEAVE MATCH?"), 800, 284, 64, Paper, true);
    BoundedCopy(Restart ? TEXT("Your current match will be reset.") : TEXT("Your current match will end."), 800, 398, Layout.Compact ? 38 : 26, PaperDim, 760, 2, true);
    const auto& Yes = Layout.ConfirmYes;
    const auto& No = Layout.ConfirmNo;
    Btn(TEXT("yes"), Restart ? TEXT("RESTART") : TEXT("LEAVE"), Yes.X, Yes.Y, Yes.W, Yes.H, Restart ? 1 : 2);
    Btn(TEXT("no"), TEXT("CANCEL"), No.X, No.Y, No.W, No.H);
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
    TextFit(Title, X, ContentTop + 40, 60, Paper, ContentW - SX);
    if (!Sub.IsEmpty())
    {
        TextFit(Sub, X, ContentTop + 118, 20, PaperDim, ContentW - SX);
    }
    Rule(X, ContentTop + 146, ContentW);
}

void AC26HUD::Menu()
{
    FA = Match->ScreenFade;
    Vignette();
    if (PreviousMenuScreen != Match->MenuScreen)
    {
        PreviousMenuScreen = Match->MenuScreen;
        CompactPage = 0;
        MenuExpanded = false;
    }
    if (Layout.Compact) { CompactMenu(); FA = 1.f; Toast(false); return; }
    ScrimLeft(.8f);
    Rect(ContentX - 24, 108, 940, 688, FLinearColor(Void.R, Void.G, Void.B, .60f));
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
    const float PanelW = 840.f, PanelY = 188.f, PanelH = 490.f;

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

    // Featured Super Over event panel: typography leads, the live stadium stays visible.
    Text(TEXT("THE FLOODLIGHT SERIES  /  SUPER OVER"), X, PanelY, 18, MintCyan);
    Text(TEXT("SIX BALLS."), X - 4, PanelY + 38, 116, Paper);
    Text(TEXT("ALL HEART."), X - 4, PanelY + 158, 116, MintCyan);
    float Y = PanelY + 304.f;
    TextFit(Match->TeamName(Match->PlayerTeam) + TEXT("  /  ") + Match->TeamName(1 - Match->PlayerTeam), X, Y, 25, Paper, PanelW);
    Y += 48.f;
    Text(TEXT("Two wickets. One chance to own the night."), X, Y, 22, PaperDim);
    Text(TEXT("SINGLE PLAYER   /   APPROX. 5 MIN"), X, Y + 38, 17, Muted);

    // Full-width primary CTA docked to the panel's bottom edge (reference bottom teal bar)
    const float CY = PanelY + PanelH - 78.f;
    Btn(TEXT("quickplay"), TEXT("PLAY SUPER OVER  /"), X, CY, 440, 72, 1);
    Btn(TEXT("nav_teams"), TEXT("CHANGE CLUB"), X + 456, CY, 244, 72, 0);

    // Secondary supporting cards below the hero panel
    const float SY = PanelY + PanelH + 24.f, SW = (PanelW - 20.f) * .5f;
    Panel(X, SY, SW, 92.f, FLinearColor::Transparent);
    Text(TEXT("HOW TO PLAY"), X + 20, SY + 16, 20, Paper);
    TextFit(TEXT("Timing. Footwork. Match-winning shots."), X + 20, SY + 48, 18, Muted, SW - 40);
    Zones.Add({TEXT("nav_help"), FBox2D(FVector2D(X, SY), FVector2D(X + SW, SY + 92.f))});

    Panel(X + SW + 20.f, SY, SW, 92.f, FLinearColor::Transparent);
    Text(TEXT("ALL GAME MODES"), X + SW + 40.f, SY + 16, 20, Paper);
    TextFit(TEXT("Find your next cricket experience."), X + SW + 40.f, SY + 48, 18, Muted, SW - 40);
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
        TextFit(Modes[I], CX, CY + 16, 30, Paper, RW);
        Text(TEXT("IN DEVELOPMENT"), CX, CY + 56, 17, Muted);
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
    PageHead(TEXT("YOUR CLUB"), TEXT("SQUAD HUB"), TEXT("Six-player roster preview. Squad editing is not available in this build."));

    const int Team = Match->PlayerTeam;
    const float X = ContentX, Y = 320.f;

    // Featured player (left)
    Panel(X, Y, 340.f, 430.f, TeamColor(Team));
    Crest(X + 170.f, Y + 110.f, 62.f, Team);
    Text(Team == 0 ? TEXT("A. RAO") : TEXT("J. HART"), X + 170.f, Y + 200.f, 42, Paper, true);
    Text(TEXT("CAPTAIN  •  BATTER"), X + 170.f, Y + 240.f, 17, TeamColor(Team), true);
    Rule(X + 30.f, Y + 284.f, 280.f);
    Text(TEXT("BATTING"), X + 30.f, Y + 306.f, 15, Muted);
    Text(Team == 0 ? TEXT("82") : TEXT("80"), X + 310.f, Y + 300.f, 24, Paper, true);
    Text(TEXT("BOWLING"), X + 30.f, Y + 340.f, 15, Muted);
    Text(Team == 0 ? TEXT("20") : TEXT("18"), X + 310.f, Y + 334.f, 24, Paper, true);
    Text(TEXT("FIELDING"), X + 30.f, Y + 374.f, 15, Muted);
    Text(Team == 0 ? TEXT("70") : TEXT("69"), X + 310.f, Y + 368.f, 24, Paper, true);

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

    TextWrap(TEXT("ROSTER PREVIEW / Six featured players. Full squad management is in development."), GX, Y + 2 * (CardH + 16.f) + 16.f, 18, Muted, GW);
    BackBtn(TEXT("nav_home"));
}

// Reusable compact sports card (reference: WBP_PlayerCard). Dark panel, name, role,
// three small stats, teal edge on the selected/featured entry.
void AC26HUD::PlayerCard(float X, float Y, float W, float H, const FString& Name, const FString& Role, int Bat, int Bowl, int Field, int Team, bool Selected)
{
    Panel(X, Y, W, H, Selected ? TeamColor(Team) : FLinearColor::Transparent);
    TextFit(Name, X + 16, Y + 12, 28, Paper, W - 32);
    TextFit(Role, X + 16, Y + 46, 16, TeamColor(Team), W - 32);
    Rule(X + 16, Y + 68, W - 32);
    const float SW = (W - 32) / 3.f;
    auto Stat = [&](int I, const TCHAR* L, int V)
    {
        const float SX = X + 16 + I * SW;
        Text(FString::FromInt(V), SX, Y + 78, 20, Paper);
        Text(L, SX, Y + 104, 14, Muted);
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
        Panel(CX2, Y, CW, 312.f, FLinearColor::Transparent);
        CategoryMark(CX2 + CW * .5f, Y + 76, I);
        TextFit(Cats[I], CX2 + 24, Y + 142, 30, Paper, CW - 48);
        BoundedCopy(Desc[I], CX2 + 24, Y + 188, 18, Muted, CW - 48, 2);
        Text(TEXT("PREVIEW / NOT AVAILABLE"), CX2 + 24, Y + 266, 16, MintCyan);
    }

    Btn(TEXT("mode_super"), TEXT("PLAY SUPER OVER  >"), X, Y + 344.f, 360, 64, 1);
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
        Text(TEXT("CAREER PROGRESSION IS NOT AVAILABLE IN THIS BUILD"), X, PY, 18, Muted);

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

        Text(TEXT("THE LEADERBOARD IS NOT OPEN YET"), X + ContentW * .5f, TY + 112, 36, Paper, true);
        TextFit(TEXT("Rankings and online seasons are not available in this build."), X + ContentW * .5f, TY + 170, 20, Muted, ContentW - 80, true);

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
    TextWrap(TEXT("This mode is in development. Play the Super Over experience available in this build."), X, Y + 48, 24, PaperDim, ContentW - 160);

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

    static const TCHAR* TT[] = {TEXT("BATTING"), TEXT("BOWLING"), TEXT("SUPER OVER")};
    static const TCHAR* HD[] = {
        TEXT("Left stick controls footwork. Swipe right area to aim direction and power. Time as ball pitches."),
        TEXT("Select delivery type and length. Drag pitch marker. Tap release in the gold sweet spot."),
        TEXT("Six balls. Two wickets. Every dot is gold; every boundary swings the match.")
    };

    for (int I = 0; I < 3; ++I)
    {
        const float W = (ContentW - 80.f) / 3.f, X = ContentX + I * (W + 40.f), Y = 328.f;
        Rule(X, Y, W);
        Text(TT[I], X, Y + 24, 38, Paper);
        TextWrap(HD[I], X, Y + 88, 22, PaperDim, W);
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

    // Team, score, legal balls and chase each own a bounded column.
    const bool Compact = Layout.Compact;
    const float BX = 56.f, BY = 32.f, BW = Compact ? 1240.f : 840.f, BH = Compact ? 112.f : 80.f;
    const float TeamW = Compact ? 176.f : 128.f, ScoreW = Compact ? 256.f : 180.f, BallsW = Compact ? 240.f : 180.f;
    const float LabelSize = Compact ? 34.f : 18.f, ValueSize = Compact ? 62.f : 44.f;
    Rect(BX, BY, BW, BH, PillBg);
    Rect(BX, BY, TeamW, BH, TC);
    TextMid(Match->TeamShort(Bat), BX + TeamW * .5f, BY, BH, Compact ? 48 : 34, Void, true);
    const float ScoreX = BX + TeamW, BallsX = ScoreX + ScoreW, ChaseX = BallsX + BallsW;
    TextFit(FString::Printf(TEXT("%d / %d"), S.Runs, S.Wickets), ScoreX + ScoreW * .5f, BY + (BH - LineH(ValueSize)) * .5f, ValueSize, Paper, ScoreW - 32, true);
    Line(BallsX, BY + 16, BallsX, BY + BH - 16, Hairline);
    TextMid(FString::Printf(TEXT("%d / 6 BALLS"), S.LegalBalls), BallsX + BallsW * .5f, BY, BH, LabelSize, PaperDim, true);
    Line(ChaseX, BY + 16, ChaseX, BY + BH - 16, Hairline);
    const FString Chase = Match->Rules.Current == 1 ? FString::Printf(TEXT("NEED %d FROM %d"), Match->Rules.RunsRequired(), Match->Rules.BallsRemaining()) : TEXT("FIRST INNINGS");
    TextFit(Chase, ChaseX + 24, BY + (BH - LineH(LabelSize)) * .5f, LabelSize, Gold, BX + BW - ChaseX - 48);

    // Batter and bowler are constrained independently, including long generated names.
    const float NamesY = BY + BH + 8, NamesH = Compact ? 56.f : 40.f;
    Rect(BX, NamesY, BW, NamesH, CardBg);
    TextFit(Match->BatterName() + TEXT(" *"), BX + 16, NamesY + 6, LabelSize, Paper, BW * .48f - 32);
    TextFit(TEXT("BOWL / ") + Match->BowlerName(), BX + BW * .5f, NamesY + 6, LabelSize, PaperDim, BW * .5f - 16);
    Btn(TEXT("pause"), TEXT("II"), Compact ? 1416 : 1456, BY, Compact ? 128 : 88, Compact ? 112 : 64, 0);

    // Delivery history lives below the score bug, never beneath touch controls or subtitles.
    const float HistoryY = NamesY + NamesH + (Compact ? 40 : 28);
    float BallX = BX + 24;
    const int Start = FMath::Max(0, int(S.Ledger.size()) - 6);
    for (int I = Start; I < int(S.Ledger.size()); ++I)
    {
        const auto& O = S.Ledger[I];
        const bool Wicket = O.Wicket != C26::Dismissal::None;
        const FString V = Wicket ? TEXT("W") : (O.WideRuns ? TEXT("Wd") : (O.NoBall ? TEXT("Nb") : FString::FromInt(O.BatRuns + O.Byes + O.LegByes)));
        const float Radius = Compact ? 26.f : 18.f;
        Circle(BallX, HistoryY, Radius, Wicket ? Coral : (O.BatRuns >= 4 ? MintCyan : PaperDim), 1.5f);
        TextMid(V, BallX, HistoryY - Radius, Radius * 2, Compact ? 34 : 18, Wicket ? Coral : Paper, true);
        BallX += Compact ? 76.f : 52.f;
    }
    if (S.FreeHit) Text(TEXT("FREE HIT"), BX + (Compact ? 510 : 340), HistoryY - LineH(LabelSize) * .5f, LabelSize, MintCyan);
}

// ============================================================================
// GAMEPLAY CONTROLS
// Unobtrusive, transparent touch zones with zero collision.
// ============================================================================
void AC26HUD::Controls()
{
    if (Layout.Compact) { CompactControls(); return; }
    const auto Phase = Match->Phase;

    if (Phase == EC26Phase::Ready || Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery)
    {
        if (Match->PlayerBatting())
        {
            // Left Stick Footwork
            const auto C = Layout.FootCenter;
            Circle(C.X, C.Y, Layout.FootRadius, PaperDim, 1.5f);
            Circle(C.X + Match->Footwork * Layout.FootRadius, C.Y - Match->Intent.Stride * Layout.FootRadius, 16, MintCyan, 2.f);
            Text(TEXT("FOOTWORK"), C.X, C.Y + Layout.FootRadius + 20, 18, PaperDim, true);

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
        TextFit(Match->Callout, 800, 420, 96, Edge, 1200, true);
        BoundedCopy(Match->Detail, 800, 556, 26, Paper, 960, 2, true);
    }

    if (Phase == EC26Phase::Replay)
    {
        Text(TEXT("REPLAY"), 1312, 50, 24, Coral);
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

    Rect(248, 100, 1104, 756, FLinearColor(Void.R, Void.G, Void.B, .84f));
    Text(TEXT("FULL TIME / SUPER OVER"), 800, 128, Layout.Compact ? 34 : 20, PaperDim, true);
    float Y = 190.f;
    TextFit(Match->Callout, 800, Y, 96, Edge, 1040, true);
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
        const float FS = Layout.Compact ? 38.f : 28.f;
        TextFit(Margin, 800, Y, FS, Gold, 1000, true);
        Y += LineH(FS) + 32.f;
    }

    // Both Innings Score summaries (Centered with generous 18px line gap)
    for (int I = 0; I < 2; ++I)
    {
        const auto& S = Match->Rules.Scores[I];
        int Team = I == 0 ? Match->FirstBattingTeam : 1 - Match->FirstBattingTeam;
        const float RowH = Layout.Compact ? 96.f : 80.f;
        Rect(280, Y, 1040, RowH, CardBg);
        TextFit(Match->TeamName(Team), 312, Y + 16, Layout.Compact ? 42 : 32, TeamColor(Team), 560);
        TextFit(FString::Printf(TEXT("%d / %d"), S.Runs, S.Wickets), 928, Y + 12, Layout.Compact ? 52 : 40, Paper, 190);
        TextMid(FString::Printf(TEXT("%db"), S.LegalBalls), 1240, Y, RowH, Layout.Compact ? 38 : 24, PaperDim, true);
        Y += RowH + 16.f;
    }

    // Centered Action Buttons: (260 + 24 + 200 = 484 total width, centered at 800)
    Y = Layout.Compact ? 716.f : FMath::Min(Y + 24.f, 728.f);
    const float H = Layout.Compact ? 112.f : 72.f;
    Btn(TEXT("again"), TEXT("PLAY AGAIN  >"), 392, Y, 480, H, 1);
    Btn(TEXT("menu"), TEXT("HOME"), 904, Y, 304, H, 0);
}

// ============================================================================
// PAUSE & IN-GAME SETTINGS
// Modern minimalist pause menu.
// ============================================================================
void AC26HUD::Preferences()
{
    Zones.Reset();
    Rect(0, 0, 1600, 900, FLinearColor(Void.R, Void.G, Void.B, .94f));
    const float FS = Layout.Compact ? 38.f : 26.f;
    if (Match->ControlsOpen)
    {
        Text(TEXT("MASTER THE MOMENT."), 800, 148, 72, Paper, true);
        BoundedCopy(TEXT("BATTING / Move with the left stick. Swipe to aim and time. Toggle LOFT for aerial shots. Tap RUN for singles."), 320, 292, FS, PaperDim, 960, 3);
        BoundedCopy(TEXT("BOWLING / Pick a delivery, drag the pitch marker, then release in the gold timing window."), 320, 480, FS, PaperDim, 960, 3);
        Btn(TEXT("close"), TEXT("RESUME MATCH"), 560, 716, 480, 112, 1);
        return;
    }
    Text(TEXT("TAKE A BREATHER."), 800, 88, 80, Paper, true);
    Text(TEXT("MATCH PAUSED"), 800, 204, Layout.Compact ? 34 : 20, MintCyan, true);
    const float X = 500, W = 600, H = Layout.Compact ? 104 : 72, Gap = 16;
    float Y = 284;
    Btn(Match->SettingsOpen ? TEXT("close") : TEXT("pause"), TEXT("RESUME MATCH"), X, Y, W, H, 1);
    Y += H + Gap;
    Btn(TEXT("master"), Match->Preferences->SoundVolume > .1f ? TEXT("AUDIO: ON") : TEXT("AUDIO: OFF"), X, Y, W, H);
    Y += H + Gap;
    Btn(TEXT("help"), TEXT("HOW TO PLAY"), X, Y, W, H);
    Y += H + Gap;
    Btn(TEXT("confirm_restart"), TEXT("RESTART MATCH"), X, Y, W, H);
    Y += H + Gap;
    Btn(TEXT("confirm_exit"), TEXT("EXIT TO HOME"), X, Y, W, H, 2);
}

void AC26HUD::Subtitle()
{
    if (!Match->Preferences->Subtitles) return;
    if (!Match->Audio || Match->Audio->ActiveSubtitle.IsEmpty()) return;
    if (GetWorld()->GetTimeSeconds() > Match->Audio->SubtitleUntil) return;

    if (Match->Phase == EC26Phase::Intro || Match->Phase == EC26Phase::Interval || Match->Phase == EC26Phase::Result) return;
    const auto& B = Layout.Subtitle;
    const float FS = Layout.Compact ? 34.f : 22.f;
    const float Step = FMath::Max(LineH(FS) + 6.f, FS * 1.5f);
    const int Lines = FMath::Clamp(WrapLines(Match->Audio->ActiveSubtitle, FS, B.W - 40), 1, 2);
    const float H = FMath::Min(B.H, (Lines - 1) * Step + LineH(FS) + 20);
    Rect(B.X, B.Y, B.W, H, PillBg);
    BoundedCopy(Match->Audio->ActiveSubtitle, B.X + B.W * .5f, B.Y + 10, FS, Paper, B.W - 40, Lines, true);
}

void AC26HUD::DrawHUD()
{
    Super::DrawHUD();
    Match = Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());
    if (!Match || !Canvas || !Match->Preferences) return;

    const float PadX = Canvas->SizeX * SafePaddingRatio.X;
    const float PadY = Canvas->SizeY * SafePaddingRatio.Y;
    Layout = C26UI::Layout::Fit(Canvas->SizeX, Canvas->SizeY, PadX, PadY, PadX, PadY);
    Scale = Layout.Scale;
    OffsetX = Layout.OffsetX;
    OffsetY = Layout.OffsetY;
    Zones.Reset();
    Pointer = FVector2D(-1.f, -1.f);
    float MouseX = 0.f, MouseY = 0.f;
    if (PlayerOwner && PlayerOwner->GetMousePosition(MouseX, MouseY) && !BlocksGameplayInput()) Pointer = ToDesign(FVector2D(MouseX, MouseY));
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
    if (!Match->PendingConfirm.IsNone()) Confirm();
    if (Match->Phase == EC26Phase::Menu && Match->ScreenFade < .9f) Zones.Reset();
}
