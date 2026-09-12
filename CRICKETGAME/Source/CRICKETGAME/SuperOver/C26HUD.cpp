#include "C26HUD.h"
#include "C26Controls.h"
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
#include "Misc/CommandLine.h"

// ============================================================================
// CRICKET 26 — COMMERCIAL SPORTS BROADCAST UI & PRESENTATION SYSTEM
// Rigorous UI layout, strict 8-point spacing scale, non-overlapping responsive design
// ============================================================================

namespace
{
// ---- Color System ----
const FLinearColor Void(.008f, .011f, .016f, .98f);          // Pure Obsidian Carbon (#020304)
const FLinearColor SurfaceBase(.016f, .022f, .032f, .94f);   // Deep Stadium Charcoal Base (#040608)
const FLinearColor SurfaceCard(.025f, .034f, .048f, .94f);   // Structured Sports Card Surface (#06090C)
const FLinearColor SurfaceWell(.012f, .016f, .024f, .96f);   // Recessed Technical Well (#030406)
const FLinearColor SurfacePill(.038f, .050f, .068f, .88f);   // Tactile Pill Surface (#0A0D12)
const FLinearColor SurfaceHover(.065f, .085f, .115f, .92f);  // Interactive Highlight

// Athletic Typography & Hairlines
const FLinearColor WhiteAthletic(.96f, .98f, 1.00f, 1.f);    // Stadium White (#F5FAFF)
const FLinearColor SilverCool(.74f, .80f, .88f, 1.f);        // Cool Silver Secondary (#BDCCE0)
const FLinearColor SlateMuted(.44f, .50f, .60f, 1.f);        // Technical Metadata Slate (#708099)
const FLinearColor HairlineSoft(.18f, .24f, .34f, .42f);     // Subtle Structural Hairline
const FLinearColor HairlineGleam(.60f, .72f, .90f, .22f);    // Luminous Top Edge Sheen

// Semantic Sports Palette
const FLinearColor Gold(1.00f, .76f, .14f, 1.f);             // Championship Gold (#FFC224)
const FLinearColor Crimson(.92f, .14f, .20f, 1.f);           // Cricket Crimson / Wickets (#EB2433)
const FLinearColor TurfGreen(.08f, .82f, .44f, 1.f);         // Boundaries (4s) / Sweet Spot (#14D170)
const FLinearColor ElectricCyan(.00f, .78f, .98f, 1.f);      // Focus Highlight (#00C7FA)
const FLinearColor DarkLabel(.015f, .025f, .035f, 1.f);      // High-contrast dark text on solid buttons

// ---- CRICKET 26 FRONT-END PALETTE ------------------------------------------
// Near-black ground, a single mint accent, and a five-step neutral ramp that
// carries the whole hierarchy. Mint is only ever used for the one actionable
// thing on a screen, the section eyebrow, and a single highlighted figure --
// never for structure. Structure is hairlines and space.
//
// These are authored as sRGB hex, exactly as the approved mockups specify them.
// The canvas treats an FLinearColor as linear and gamma-encodes on write, so a
// raw hex/255 value would come out washed out by roughly two stops; going
// through the FColor constructor applies the sRGB->linear conversion first.
FLinearColor Hex(uint8 R, uint8 G, uint8 B, float A = 1.f)
{
    FLinearColor C = FLinearColor(FColor(R, G, B, 255));
    C.A = A;
    return C;
}
const FLinearColor Ink     = Hex(0x08, 0x09, 0x0B);   // page ground
const FLinearColor Mint    = Hex(0x2E, 0xE8, 0xC7);   // accent
const FLinearColor MintLit = Hex(0x6B, 0xF4, 0xDD);   // accent, pressed
const FLinearColor MintInk = Hex(0x06, 0x10, 0x0F);   // label on mint
const FLinearColor T1      = Hex(0xF4, 0xF7, 0xF8);   // headline
const FLinearColor T2      = Hex(0xE7, 0xED, 0xF0);   // row title
const FLinearColor T3      = Hex(0x8B, 0x95, 0x9B);   // control key
const FLinearColor T4      = Hex(0x68, 0x74, 0x7C);   // meta
const FLinearColor T5      = Hex(0x5C, 0x68, 0x70);   // sub / de-emphasis
const FLinearColor T6      = Hex(0x4E, 0x59, 0x60);   // micro label
const FLinearColor T7      = Hex(0x39, 0x43, 0x4A);   // faintest
const FLinearColor TBody   = Hex(0x6E, 0x7A, 0x82);   // table figures
const FLinearColor TGhost  = Hex(0xB9, 0xC4, 0xCA);   // ghost button label
const FLinearColor TVal    = Hex(0xD7, 0xDF, 0xE3);   // stat value
const FLinearColor Podium  = Hex(0xD9, 0xC0, 0x7A);   // rank 1
// Hairlines are white at a low alpha over the ground, so they stay neutral.
const FLinearColor Hair    = Hex(0xFF, 0xFF, 0xFF, .065f);

// Layout Metrics (1600 x 900 design canvas)
const float TopH = 68.f;
const float ContentX = 80.f;
const float ContentR = 1520.f;
const float ContentW = ContentR - ContentX; // 1440px
const float ContentTop = TopH + 28.f;       // 96px

// ---- Front-end shell metrics ----
// Design canvas is 1600 x 900 and the approved mockups are 1:1 with it, so
// every number below is read straight off them.
const float RailX     = 72.f;
const float RailTop   = 246.f;
const float RailPitch = 44.f;   // 13 pad + 18 line + 13 pad
const float BayX      = 430.f;
const float BayR      = 1528.f;
const float BayW      = BayR - BayX;   // 1098px
}

AC26HUD::AC26HUD()
{
    // Fonts are bound in BeginPlay; hit zones rebuild every DrawHUD.
}

void AC26HUD::BeginPlay()
{
    Super::BeginPlay();
    // 1. DIN Condensed Bold: Authentic sports numbers, score bugs, overs, match clock
    SportsFont = NewObject<UFont>(this);
    SportsFont->FontCacheType = EFontCacheType::Runtime;
    SportsFont->LegacyFontSize = 32;
    SportsFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
        FTypefaceEntry(TEXT("Regular"),
        FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts/DINCondensed-Bold.ttf"),
        EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad));

    // 2. Barlow Condensed Bold: Screen titles, club names, hero CTA buttons, navigation
    DisplayFont = NewObject<UFont>(this);
    DisplayFont->FontCacheType = EFontCacheType::Runtime;
    DisplayFont->LegacyFontSize = 32;
    DisplayFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
        FTypefaceEntry(TEXT("Regular"),
        FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts/BarlowCondensed-Bold.ttf"),
        EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad));

    // 3. Barlow Condensed SemiBold: High-legibility body copy, metadata, table data
    TitleFont = NewObject<UFont>(this);
    TitleFont->FontCacheType = EFontCacheType::Runtime;
    TitleFont->LegacyFontSize = 32;
    TitleFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
        FTypefaceEntry(TEXT("Regular"),
        FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts/BarlowCondensed-SemiBold.ttf"),
        EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad));

    // 4. Barlow Condensed ExtraBold: Event stingers, high-impact broadcast badges
    HeavyFont = NewObject<UFont>(this);
    HeavyFont->FontCacheType = EFontCacheType::Runtime;
    HeavyFont->LegacyFontSize = 32;
    HeavyFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
        FTypefaceEntry(TEXT("Regular"),
        FPaths::ProjectContentDir() / TEXT("Cricket26/UI/Fonts/BarlowCondensed-ExtraBold.ttf"),
        EFontHinting::Default,
        EFontLoadingPolicy::LazyLoad));
}

FLinearColor AC26HUD::WithA(FLinearColor C, float M) const
{
    return FLinearColor(C.R, C.G, C.B, C.A * FA * M);
}

float AC26HUD::LineH(float Size) const
{
    // Generous, breathable typographic leading (148%) prevents text collision across all resolutions
    return Size * 1.48f;
}

float AC26HUD::TH(float Size) const
{
    return Size * 1.48f;
}

int AC26HUD::WrapLines(const FString& S, float Size, float MaxW, int FontChoice) const
{
    TArray<FString> Words;
    S.ParseIntoArray(Words, TEXT(" "), true);
    if (Words.Num() == 0) return 1;

    int Lines = 1;
    FString Cur = Words[0];
    for (int I = 1; I < Words.Num(); ++I)
    {
        const FString Candidate = Cur + TEXT(" ") + Words[I];
        if (Width(Candidate, Size, FontChoice) <= MaxW)
        {
            Cur = Candidate;
        }
        else
        {
            Cur = Words[I];
            ++Lines;
        }
    }
    return Lines;
}

float AC26HUD::TextWrap(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center, int FontChoice)
{
    TArray<FString> Words;
    S.ParseIntoArray(Words, TEXT(" "), true);
    if (Words.Num() == 0) return 0;

    const float LH = LineH(Size);
    FString Cur = Words[0];
    float CurY = Y;

    for (int I = 1; I < Words.Num(); ++I)
    {
        const FString Candidate = Cur + TEXT(" ") + Words[I];
        if (Width(Candidate, Size, FontChoice) <= MaxW)
        {
            Cur = Candidate;
        }
        else
        {
            Text(Cur, X, CurY, Size, Color, Center, FontChoice);
            CurY += LH;
            Cur = Words[I];
        }
    }
    Text(Cur, X, CurY, Size, Color, Center, FontChoice);
    return CurY + LH - Y;
}

void AC26HUD::TextFit(const FString& S, float X, float Y, float Size, FLinearColor Color, float MaxW, bool Center, int FontChoice)
{
    float FS = Size;
    const float SafeMaxW = FMath::Max(10.f, MaxW - Sp8);
    while (FS > 10.f && Width(S, FS, FontChoice) > SafeMaxW) FS -= 1.f;
    Text(S, X, Y, FS, Color, Center, FontChoice);
}

void AC26HUD::TextMid(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor Color, bool Center, int FontChoice)
{
    // Crisp typographic vertical centering using character glyph cap-height
    Text(S, X, Y + (BoxH - Size) * 0.5f - 1.f, Size, Color, Center, FontChoice);
}

void AC26HUD::TextMidFit(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor Color, float MaxW, bool Center, int FontChoice)
{
    // A label centred inside a container must never be allowed to reach the
    // container's own border, so the fit keeps an 8px gutter on each side.
    float FS = Size;
    const float SafeMaxW = FMath::Max(10.f, MaxW - Sp8);
    while (FS > 10.f && Width(S, FS, FontChoice) > SafeMaxW) FS -= 1.f;
    TextMid(S, X, Y, BoxH, FS, Color, Center, FontChoice);
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
    // Team 0: Mumbai Meteors (Cobalt Blue #0A78F2) | Team 1: Melbourne Cyclones (Sunset Coral Crimson #F83A52)
    return Team == 0 ? FLinearColor(.04f, .47f, .95f, 1.f) : FLinearColor(.97f, .23f, .32f, 1.f);
}

FString AC26HUD::TeamTagline(int Team) const
{
    return Team == 0 ? TEXT("RIDE THE STORM") : TEXT("BURN BRIGHT");
}

FString AC26HUD::Track(const FString& S) const
{
    FString Out;
    for (int I = 0; I < S.Len(); ++I)
    {
        Out.AppendChar(S[I]);
        if (I < S.Len() - 1) Out.Append(TEXT(" "));
    }
    return Out;
}

// ---- Canvas Draw Helpers ----
void AC26HUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{
    DrawRect(WithA(Color), OffsetX + X * Scale, OffsetY + Y * Scale, W * Scale, H * Scale);
}

void AC26HUD::Line(float X, float Y, float X2, float Y2, FLinearColor Color, float Thickness)
{
    DrawLine(OffsetX + X * Scale, OffsetY + Y * Scale, OffsetX + X2 * Scale, OffsetY + Y2 * Scale, WithA(Color), Thickness * Scale);
}

void AC26HUD::Text(const FString& S, float X, float Y, float Size, FLinearColor Color, bool Center, int FontChoice)
{
    UFont* F = DisplayFont.Get();
    if (FontChoice == 1 && TitleFont)   F = TitleFont.Get();
    if (FontChoice == 2 && SportsFont)  F = SportsFont.Get();
    if (FontChoice == 3 && HeavyFont)   F = HeavyFont.Get();
    if (!F) F = DisplayFont.Get();
    if (!F) return;

    float W = 0, H = 0;
    Canvas->StrLen(F, S, W, H);
    if (Center) X -= W * Size / 32.f * .5f;
    FCanvasTextItem Item(FVector2D(OffsetX + X * Scale, OffsetY + Y * Scale), FText::FromString(S), F, WithA(Color));
    Item.Scale = FVector2D(Scale * Size / 32.f);
    if (TextShadow > 0.f) Item.EnableShadow(FLinearColor(0, 0, 0, TextShadow), FVector2D(1.0f, 1.2f));
    Canvas->DrawItem(Item);
}

float AC26HUD::Width(const FString& S, float Size, int FontChoice) const
{
    UFont* F = DisplayFont.Get();
    if (FontChoice == 1 && TitleFont)   F = TitleFont.Get();
    if (FontChoice == 2 && SportsFont)  F = SportsFont.Get();
    if (FontChoice == 3 && HeavyFont)   F = HeavyFont.Get();
    if (!F) F = DisplayFont.Get();
    if (!F || !Canvas) return 0;

    float W = 0, H = 0;
    Canvas->StrLen(F, S, W, H);
    return W * Size / 32.f;
}

void AC26HUD::Circle(float X, float Y, float Radius, FLinearColor Color, float Thickness)
{
    const int Segs = 36;
    const float Step = 2.f * PI / (float)Segs;
    for (int I = 0; I < Segs; ++I)
    {
        const float A1 = I * Step, A2 = (I + 1) * Step;
        Line(X + Radius * FMath::Cos(A1), Y + Radius * FMath::Sin(A1),
             X + Radius * FMath::Cos(A2), Y + Radius * FMath::Sin(A2), Color, Thickness);
    }
}

void AC26HUD::StatBar(float X, float Y, float W, float H, float Pct, FLinearColor FillColor)
{
    Rect(X, Y, W, H, SurfaceWell);
    Line(X, Y, X + W, Y, HairlineSoft, 1.f);
    Line(X, Y + H, X + W, Y + H, HairlineSoft, 1.f);
    Line(X, Y, X, Y + H, HairlineSoft, 1.f);
    Line(X + W, Y, X + W, Y + H, HairlineSoft, 1.f);

    const float FW = FMath::Clamp(Pct, 0.f, 1.f) * W;
    if (FW > 1.f)
    {
        Rect(X, Y, FW, H, FillColor);
        Rect(X, Y, FW, 1.5f, FLinearColor(1.f, 1.f, 1.f, .35f));
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
    if (Scale <= 0.f) return Point;
    return FVector2D((Point.X - OffsetX) / Scale, (Point.Y - OffsetY) / Scale);
}

FVector2D AC26HUD::FromDesign(FVector2D DesignPos) const
{
    return FVector2D(OffsetX + DesignPos.X * Scale, OffsetY + DesignPos.Y * Scale);
}

void AC26HUD::Vignette()
{
    // Cinematic stadium floodlight atmosphere vignette
    Rect(0, 0, 1600, 900, FLinearColor(.006f, .009f, .013f, .78f));
    Line(0, 0, 1600, 0, HairlineSoft, 1.f);
}

void AC26HUD::Panel(float X, float Y, float W, float H, FLinearColor Edge)
{
    // Premium sports surface container with precision hairlines
    Rect(X, Y, W, H, SurfaceCard);
    Line(X, Y, X + W, Y, HairlineSoft, 1.f);
    Line(X, Y + H, X + W, Y + H, HairlineSoft, 1.f);
    Line(X, Y, X, Y + H, HairlineSoft, 1.f);
    Line(X + W, Y, X + W, Y + H, HairlineSoft, 1.f);

    // Luminous top glass sheen
    Line(X + 1, Y + 1, X + W - 1, Y + 1, HairlineGleam, 1.f);

    if (Edge.A > 0.01f)
    {
        // 3.5px Athletic Left Anchor Bar
        Rect(X, Y, 3.5f, H, Edge);
        // Soft gradient bleed
        Rect(X + 3.5f, Y, 16.f, H, FLinearColor(Edge.R, Edge.G, Edge.B, .08f));
    }
}

void AC26HUD::Rule(float X, float Y, float W)
{
    Line(X, Y, X + W, Y, HairlineSoft, 1.f);
}

void AC26HUD::Ghost(const FString& S, float X, float Y, float Size)
{
    Text(S, X, Y, Size, FLinearColor(1.f, 1.f, 1.f, .025f), false, 2);
}

void AC26HUD::Crest(float X, float Y, float R, int Team)
{
    const auto C = TeamColor(Team);
    Circle(X, Y, R, C, 2.5f);
    Circle(X, Y, R - 5.f, FLinearColor(C.R, C.G, C.B, .18f), 4.f);
    Text(Team == 0 ? TEXT("MM") : TEXT("MC"), X, Y - R * .36f, R * .95f, WhiteAthletic, true, 2);
}

void AC26HUD::PlayGlyph(float X, float Y, float S, FLinearColor C)
{
    Line(X - S * .35f, Y - S * .5f, X + S * .5f, Y, C, 2.f);
    Line(X + S * .5f, Y, X - S * .35f, Y + S * .5f, C, 2.f);
    Line(X - S * .35f, Y + S * .5f, X - S * .35f, Y - S * .5f, C, 2.f);
}

// Style: 0 secondary/glass, 1 primary (Solid Championship Gold CTA), 2 danger (Crimson), 3 selected
void AC26HUD::Btn(FName Action, const FString& Label, float X, float Y, float W, float H, int Style, bool Selected)
{
    const bool P = Pressed(Action);
    const float MinHPadding = Sp24; // guaranteed text inset on each side of a button

    if (Style == 1)
    {
        // High-voltage Championship Gold CTA button with top luminous gleam and dark athletic typography
        Rect(X, Y, W, H, P ? WhiteAthletic : Gold);
        Rect(X, Y, W, 2, FLinearColor(1.f, 1.f, 1.f, .45f));
        Rect(X, Y + H - 3, W, 3, FLinearColor(.80f, .55f, .05f, 1.f));
        Line(X + W - 12, Y, X + W, Y + 12, Void, 2.f);

        float FS = H >= 60 ? 22.f : 18.f;
        while (FS > 12.f && Width(Label, FS, 0) > (W - 2.f * MinHPadding)) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H - 2, FS, DarkLabel, true, 0);
    }
    else if (Style == 2)
    {
        // Danger action (Cricket Crimson)
        Rect(X, Y, W, H, Crimson);
        Rect(X, Y, W, 2, FLinearColor(1.f, 1.f, 1.f, .35f));
        Rect(X, Y + H - 3, W, 3, FLinearColor(.65f, .08f, .12f, 1.f));
        Line(X + W - 12, Y, X + W, Y + 12, WhiteAthletic, 2.f);

        float FS = 19.f;
        while (FS > 12.f && Width(Label, FS, 0) > (W - 2.f * MinHPadding)) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H - 2, FS, WhiteAthletic, true, 0);
    }
    else
    {
        // Minimalist sports graphite button
        const auto Fill = Selected ? FLinearColor(.05f, .16f, .28f, .92f) : (P ? FLinearColor(.10f, .14f, .20f, .85f) : SurfacePill);
        Rect(X, Y, W, H, Fill);
        Line(X, Y, X + W, Y, Selected ? Gold : HairlineSoft, 1.f);
        Line(X, Y + H, X + W, Y + H, Selected ? Gold : HairlineSoft, 1.f);
        Line(X, Y, X, Y + H, Selected ? Gold : HairlineSoft, 1.f);
        Line(X + W, Y, X + W, Y + H, Selected ? Gold : HairlineSoft, 1.f);
        Line(X + 1, Y + 1, X + W - 1, Y + 1, Selected ? FLinearColor(Gold.R, Gold.G, Gold.B, .4f) : HairlineGleam, 1.f);

        if (Selected)
        {
            Rect(X, Y, 3.5f, H, Gold);
            Rect(X + 3.5f, Y, 12, H, FLinearColor(Gold.R, Gold.G, Gold.B, .08f));
        }

        float FS = H >= 60 ? 21.f : (H >= 48 ? 17.f : 14.f);
        while (FS > 11.f && Width(Label, FS, 0) > (W - 2.f * MinHPadding)) FS -= 1.f;
        TextMid(Label, X + W * .5f, Y, H, FS, Selected ? Gold : (P ? WhiteAthletic : SilverCool), true, 0);
    }
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::NavBtn(FName Action, const FString& Label, float X, float Y, float W, bool Selected)
{
    const float H = 36.f;
    if (Selected)
    {
        Rect(X, Y, W, H, FLinearColor(Gold.R, Gold.G, Gold.B, .12f));
        Rect(X + (W - 32) * .5f, Y + H - 2, 32, 2, Gold);
        TextMidFit(Label, X + W * .5f, Y, H - 2, 18, WhiteAthletic, W, true, 0);
    }
    else
    {
        TextMidFit(Label, X + W * .5f, Y, H, 17, SlateMuted, W, true, 0);
    }
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::Button(FName Action, const FString& Label, float X, float Y, float W, float H, bool Accent, bool Selected)
{
    Btn(Action, Label, X, Y, W, H, Accent ? 1 : 0, Selected);
}

void AC26HUD::ScrimLeft(float Strength)
{
    Rect(0, 0, 720, 900, FLinearColor(Void.R, Void.G, Void.B, .88f * Strength));
}

void AC26HUD::ScrimBottom(float Strength)
{
    Rect(0, 520, 1600, 380, FLinearColor(Void.R, Void.G, Void.B, .82f * Strength));
}

void AC26HUD::Logo(float X, float Y, float Size, int Team)
{
    Crest(X + Size * .5f, Y + Size * .5f, Size * .5f, Team);
}

void AC26HUD::HeroCrest(float X, float Y, float Size, int Team)
{
    Crest(X + Size * .5f, Y + Size * .5f, Size * .5f, Team);
}

void AC26HUD::Tag(const FString& S, float X, float Y, bool Accent)
{
    const float W = Width(S, 13, 0) + 2.f * PadEdge, H = 26.f;
    const auto Bg = Accent ? FLinearColor(Gold.R, Gold.G, Gold.B, .18f) : SurfacePill;
    const auto Fg = Accent ? Gold : SilverCool;
    Rect(X, Y, W, H, Bg);
    Line(X, Y, X + W, Y, Accent ? Gold : HairlineSoft, 1.f);
    Line(X, Y + H, X + W, Y + H, Accent ? Gold : HairlineSoft, 1.f);
    Line(X, Y, X, Y + H, Accent ? Gold : HairlineSoft, 1.f);
    Line(X + W, Y, X + W, Y + H, Accent ? Gold : HairlineSoft, 1.f);
    TextMid(S, X + W * .5f, Y, H, 13, Fg, true, 0);
}

void AC26HUD::TopNav(int Selected)
{
    // Clean, structured bar across top
    Rect(0, 0, 1600, TopH, FLinearColor(Void.R, Void.G, Void.B, .85f));
    Line(0, TopH, 1600, TopH, HairlineSoft, 1.f);
    Line(0, 1, 1600, 1, FLinearColor(1.f, 1.f, 1.f, .05f), 1.f);

    // Left: CRICKET 26 Sports Identity, inset from the brand notch by GapItem
    Rect(ContentX, TopH * .5f - 14, 3.5f, 28, Gold);
    const float BrandX = ContentX + GapItem;
    TextMid(TEXT("CRICKET"), BrandX, 0.f, TopH, 24, WhiteAthletic, false, 0);
    const float CW = Width(TEXT("CRICKET"), 24, 0);
    TextMid(TEXT("26"), BrandX + CW + Sp8, 0.f, TopH, 27, Gold, false, 2);
    Zones.Add({TEXT("nav_home"), FBox2D(FVector2D(ContentX, 0), FVector2D(ContentX + CW + 70, TopH))});

    // Center: Navigation Tabs with a uniform GapItem between them
    struct FTopTab { FName Action; const TCHAR* Label; bool Active; };
    const FTopTab Tabs[] = {
        {TEXT("nav_home"),     TEXT("MATCH"),    Selected == 0 || Selected == 3 || Selected == 4},
        {TEXT("nav_play"),     TEXT("MODES"),    Selected == 1},
        {TEXT("nav_teams"),    TEXT("TEAMS"),    Selected == 2},
        {TEXT("nav_myteam"),   TEXT("SQUAD"),    Selected == 5},
        {TEXT("nav_settings"), TEXT("SETTINGS"), Selected == 11},
        {TEXT("nav_help"),     TEXT("HELP"),     Selected == 12},
    };

    const float TabW = 108.f, TabH = 38.f;
    const float TotalTabsW = 6 * TabW + 5 * GapItem;
    const float StartTabX = 800.f - TotalTabsW * .5f;
    const float TabY = TopH * .5f - TabH * .5f;

    for (int I = 0; I < 6; ++I)
    {
        const float TX = StartTabX + I * (TabW + GapItem);
        const auto& T = Tabs[I];
        if (T.Active)
        {
            Rect(TX, TabY, TabW, TabH, FLinearColor(Gold.R, Gold.G, Gold.B, .12f));
            Line(TX, TabY, TX + TabW, TabY, FLinearColor(Gold.R, Gold.G, Gold.B, .35f), 1.f);
            Rect(TX + 18, TabY + TabH - 2, TabW - 36, 2, Gold);
            TextMidFit(T.Label, TX + TabW * .5f, TabY, TabH - 2, 17, WhiteAthletic, TabW, true, 0);
        }
        else
        {
            TextMidFit(T.Label, TX + TabW * .5f, TabY, TabH, 16, SlateMuted, TabW, true, 0);
        }
        Zones.Add({T.Action, FBox2D(FVector2D(TX, 0), FVector2D(TX + TabW, TopH))});
    }

    // Right: Broadcast Venue Capsule, right-aligned to the content column.
    // The label is measured and fitted so it can never run into the capsule rim.
    const float CapW = 268.f, CapH = 36.f;
    const float CapX = ContentR - CapW, CapY = TopH * .5f - CapH * .5f;
    Rect(CapX, CapY, CapW, CapH, SurfacePill);
    Line(CapX, CapY, CapX + CapW, CapY, HairlineSoft, 1.f);
    Line(CapX, CapY + CapH, CapX + CapW, CapY + CapH, HairlineSoft, 1.f);
    Line(CapX, CapY, CapX, CapY + CapH, HairlineSoft, 1.f);
    Line(CapX + CapW, CapY, CapX + CapW, CapY + CapH, HairlineSoft, 1.f);
    Line(CapX + 1, CapY + 1, CapX + CapW - 1, CapY + 1, HairlineGleam, 1.f);

    const float CapTextX = CapX + Sp32;
    const float CapTextW = CapW - Sp32 - Sp16;
    Circle(CapX + Sp16, TopH * .5f, 3.5f, TurfGreen, 2.f);
    TextMidFit(TEXT("ECLIPSE OVAL  •  NIGHT"), CapTextX + CapTextW * .5f, CapY, CapH, 14, SilverCool, CapTextW, true, 0);
}

void AC26HUD::TopUtility()
{
    TopNav(Match ? Match->MenuScreen : 0);
}

void AC26HUD::NavRail(int Selected)
{
    // Automated test compatibility zones
    Zones.Add({TEXT("nav_home"),    FBox2D(FVector2D(0, 0), FVector2D(100, 40))});
    Zones.Add({TEXT("nav_play"),    FBox2D(FVector2D(0, 40), FVector2D(100, 80))});
    Zones.Add({TEXT("nav_teams"),   FBox2D(FVector2D(0, 80), FVector2D(100, 120))});
    Zones.Add({TEXT("nav_myteam"),  FBox2D(FVector2D(0, 120), FVector2D(100, 160))});
    Zones.Add({TEXT("nav_career"),  FBox2D(FVector2D(0, 160), FVector2D(100, 200))});
    Zones.Add({TEXT("nav_tour"),    FBox2D(FVector2D(0, 200), FVector2D(100, 240))});
    Zones.Add({TEXT("nav_online"),  FBox2D(FVector2D(0, 240), FVector2D(100, 280))});
    Zones.Add({TEXT("nav_store"),   FBox2D(FVector2D(0, 280), FVector2D(100, 320))});
    Zones.Add({TEXT("nav_world"),   FBox2D(FVector2D(0, 320), FVector2D(100, 360))});
    Zones.Add({TEXT("nav_settings"),FBox2D(FVector2D(0, 360), FVector2D(100, 400))});
    Zones.Add({TEXT("nav_help"),    FBox2D(FVector2D(0, 400), FVector2D(100, 440))});
}

void AC26HUD::SideNavBtn(FName Action, const FString& Label, float Y, bool Selected, const FString& IndexStr)
{
}

void AC26HUD::ProfileChip(float X, float Y, float W, const FString& Kicker, const FString& Value, FLinearColor Accent)
{
    // 56 tall so the 15pt value line clears the bottom hairline instead of
    // overhanging it, which is what the old 40px chip did.
    const float H = 56.f;
    const float TX = X + PadEdge + 4.f;
    Rect(X, Y, W, H, SurfaceWell);
    Rect(X, Y, 3.5f, H, Accent);
    Text(Kicker, TX, Y + Sp8, 12, SlateMuted, false, 0);
    Text(Value, TX, Y + Sp8 + LineH(12) + Sp4, 15, WhiteAthletic, false, 0);
}

void AC26HUD::Toast(bool Gameplay)
{
    // Notification line. It answers presses on surfaces that are presented but
    // not yet wired to a system, so nothing in the hub reads as a dead button.
    // A tracked mint line on the ground, not a floating pill -- the front end
    // has no other chrome and this must not introduce any.
    if (!Match || Match->ToastText.IsEmpty() || Match->Clock > Match->ToastUntil) return;

    const float A = FMath::Clamp((Match->ToastUntil - Match->Clock) / .5f, 0.f, 1.f);
    const float Y = Gameplay ? 128.f : 842.f;
    const float X = Gameplay ? 800.f : BayX;
    const FLinearColor C(Mint.R, Mint.G, Mint.B, A);

    if (Gameplay)
    {
        const float W = Width(Match->ToastText, 16.f, 0) + 64.f;
        Rect(X - W * .5f, Y, W, 40.f, FLinearColor(Ink.R, Ink.G, Ink.B, .92f * A));
        Rect(X - W * .5f, Y, 2.f, 40.f, C);
        TextMid(Match->ToastText, X, Y, 40.f, 16.f, FLinearColor(T1.R, T1.G, T1.B, A), true, 0);
    }
    else
    {
        Rect(X, Y + 3.f, 2.f, 11.f, C);
        TextT(Match->ToastText, X + 12.f, Y, 11.f, C, .24f, 0);
    }
}

void AC26HUD::Confirm()
{
    Rect(0, 0, 1600, 900, FLinearColor(.006f, .009f, .014f, .85f));
    const float W = 520, H = 230, BtnH = 52;
    const float X = 800 - W * .5f, Y = 450 - H * .5f;
    Panel(X, Y, W, H, Crimson);

    TextFit(TEXT("CONFIRM ACTION"), 800, Y + PadCard, 28, Crimson, W - 2.f * PadCard, true, 0);
    TextFit(TEXT("Are you sure you want to proceed?"), 800, Y + PadCard + LineH(28) + GapComp, 18,
            SilverCool, W - 2.f * PadCard, true, 1);
    Btn(TEXT("yes"), TEXT("CONFIRM"), X + PadCard, Y + H - PadCard - BtnH, 196, BtnH, 2);
    Btn(TEXT("no"), TEXT("CANCEL"), X + W - PadCard - 196, Y + H - PadCard - BtnH, 196, BtnH, 0);
}




// ============================================================================
// CRICKET 26 // FRONT-END SHELL
// Near-black ground, centred wordmark, flat text rail, one content bay. No
// cards, no imagery, no glow: the hierarchy is type, hairlines and space.
// ============================================================================

void AC26HUD::Disc(float CX, float CY, float R, FLinearColor C)
{
    // Filled circle from horizontal bands following the circle equation. The
    // canvas has no filled-ellipse primitive and a thick-stroked Circle() reads
    // as a polygon at these radii.
    const int Bands = FMath::Clamp(FMath::RoundToInt(R * 1.6f), 8, 40);
    for (int I = 0; I < Bands; ++I)
    {
        const float T = ((float)I + .5f) / (float)Bands * 2.f - 1.f;
        const float HW = R * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T));
        const float BH = 2.f * R / (float)Bands + .6f;
        Rect(CX - HW, CY + T * R - BH * .5f, 2.f * HW, BH, C);
    }
}

void AC26HUD::SoftGlow(float CX, float CY, float RX, float RY, FLinearColor C, float Strength)
{
    // One barely-there elliptical lift behind the content bay. It exists to stop
    // the page reading as a flat black rectangle; at this alpha it is felt more
    // than seen, which is the point.
    const int Rings = 9, Bands = 14;
    for (int I = Rings; I >= 1; --I)
    {
        const float F = (float)I / (float)Rings;
        const float A = Strength * (1.f - F) / (float)Rings;
        if (A < .0008f) continue;
        for (int B = 0; B < Bands; ++B)
        {
            const float T = ((float)B + .5f) / (float)Bands * 2.f - 1.f;
            const float HW = RX * F * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T));
            const float BH = 2.f * RY * F / (float)Bands + .6f;
            Rect(CX - HW, CY + T * RY * F - BH * .5f, 2.f * HW, BH,
                 FLinearColor(C.R, C.G, C.B, A));
        }
    }
}

void AC26HUD::Backdrop()
{
    Rect(0, 0, 1600, 900, Ink);
    SoftGlow(1184.f, 306.f, 550.f, 280.f, Mint, .045f);
}

float AC26HUD::WidthT(const FString& S, float Size, float TrackEm, int Font) const
{
    float W = 0.f;
    const float Adv = Size * TrackEm;
    for (int I = 0; I < S.Len(); ++I)
    {
        W += Width(FString::Chr(S[I]), Size, Font);
        if (I < S.Len() - 1) W += Adv;
    }
    return W;
}

void AC26HUD::TextT(const FString& S, float X, float Y, float Size, FLinearColor C, float TrackEm, int Font, bool Center)
{
    // The canvas has no letter-spacing, so tracked labels are stepped a glyph at
    // a time. Every micro-label in the front end is tracked, and the tracking is
    // what makes them read as labels rather than as small body text.
    float CX = Center ? X - WidthT(S, Size, TrackEm, Font) * .5f : X;
    const float Adv = Size * TrackEm;
    for (int I = 0; I < S.Len(); ++I)
    {
        const FString Ch = FString::Chr(S[I]);
        Text(Ch, CX, Y, Size, C, false, Font);
        CX += Width(Ch, Size, Font) + Adv;
    }
}

void AC26HUD::TextTMid(const FString& S, float X, float Y, float BoxH, float Size, FLinearColor C, float TrackEm, int Font, bool Center)
{
    TextT(S, X, Y + (BoxH - Size) * .5f - 1.f, Size, C, TrackEm, Font, Center);
}

void AC26HUD::HairRule(float X, float Y, float W, float A)
{
    Rect(X, Y, W, 1.f, Hex(0xFF, 0xFF, 0xFF, A));
}

void AC26HUD::Eyebrow(const FString& S, float X, float Y)
{
    Disc(X + 2.5f, Y + 6.5f, 2.5f, Mint);
    TextT(S, X + 15.f, Y, 11.f, Mint, .30f, 0);
}

void AC26HUD::Headline(const FString& A, const FString& B, float X, float Y, float Size)
{
    // Two-tone display line: the subject in near-white, the qualifier dropped to
    // the de-emphasis grey so one headline carries two levels without a second
    // type size.
    Text(A, X, Y, Size, T1, false, 3);
    if (!B.IsEmpty())
    {
        Text(B, X + Width(A + TEXT(" "), Size, 3), Y, Size, T5, false, 3);
    }
}

float AC26HUD::BtnW(const FString& Label, float Size, float Pad) const
{
    return WidthT(Label, Size, .18f, 3) + 2.f * Pad;
}

void AC26HUD::PrimaryBtn(FName Action, const FString& Label, float X, float Y, float W, float H)
{
    const bool P = Pressed(Action);
    Rect(X, Y, W, H, P ? FLinearColor(.42f, .96f, .87f, 1.f) : Mint);
    TextTMid(Label, X + W * .5f, Y, H, 15.f, MintInk, .18f, 3, true);
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::GhostBtn(FName Action, const FString& Label, float X, float Y, float W, float H)
{
    const bool P = Pressed(Action);
    const FLinearColor Edge(1.f, 1.f, 1.f, P ? .26f : .13f);
    Rect(X, Y, W, 1.f, Edge); Rect(X, Y + H - 1.f, W, 1.f, Edge);
    Rect(X, Y, 1.f, H, Edge); Rect(X + W - 1.f, Y, 1.f, H, Edge);
    TextTMid(Label, X + W * .5f, Y, H, 12.f, P ? T1 : TGhost, .18f, 0, true);
    Zones.Add({Action, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))});
}

void AC26HUD::Toggle(FName Action, float X, float Y, bool On)
{
    const float W = 42.f, H = 18.f, R = H * .5f;
    Rect(X + R, Y, W - 2.f * R, H, On ? FLinearColor(Mint.R, Mint.G, Mint.B, .22f) : FLinearColor(1, 1, 1, .09f));
    Disc(X + R, Y + R, R, On ? FLinearColor(Mint.R, Mint.G, Mint.B, .22f) : FLinearColor(1, 1, 1, .09f));
    Disc(X + W - R, Y + R, R, On ? FLinearColor(Mint.R, Mint.G, Mint.B, .22f) : FLinearColor(1, 1, 1, .09f));
    Disc(On ? (X + W - 10.f) : (X + 10.f), Y + R, 6.f, On ? Mint : T5);
    Zones.Add({Action, FBox2D(FVector2D(X, Y - 6.f), FVector2D(X + W, Y + H + 6.f))});
}

void AC26HUD::Slider(FName Action, float X, float Y, float W, float Pct)
{
    const float P = FMath::Clamp(Pct, 0.f, 1.f);
    Rect(X, Y, W, 2.f, FLinearColor(1.f, 1.f, 1.f, .09f));
    Rect(X, Y, W * P, 2.f, Mint);
    Disc(X + W * P, Y + 1.f, 4.f, Mint);
    Zones.Add({Action, FBox2D(FVector2D(X, Y - 12.f), FVector2D(X + W, Y + 14.f))});
}

void AC26HUD::MicroBar(float X, float Y, float W, float Pct)
{
    Rect(X, Y, W, 2.f, FLinearColor(1.f, 1.f, 1.f, .08f));
    Rect(X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), 2.f, Mint);
}

void AC26HUD::StatLine(const FString& Label, const FString& Value, float X, float Y, FLinearColor VC)
{
    TextT(Label, X, Y, 10.f, T6, .24f, 0);
    Text(Value, X, Y + 20.f, 23.f, VC, false, 2);
}

void AC26HUD::Wordmark()
{
    TextT(TEXT("SUPER OVER"), 800.f, 38.f, 26.f, T1, .30f, 3, true);
    TextT(TEXT("CRICKET"), 800.f, 73.f, 13.f, Mint, .62f, 0, true);
    Zones.Add({TEXT("nav_home"), FBox2D(FVector2D(680, 30), FVector2D(920, 92))});
}

void AC26HUD::ProfileBar()
{
    // Laid out right to left off the bay's right margin so the cluster keeps the
    // same optical edge as every content column on every screen.
    const float Top = 36.f;
    float Cursor = BayR;

    // Avatar.
    const float AvR = 19.f;
    Disc(Cursor - AvR, Top + AvR, AvR, FLinearColor(.071f, .086f, .102f, 1.f));
    Circle(Cursor - AvR, Top + AvR, AvR, FLinearColor(1.f, 1.f, 1.f, .13f), 1.f);
    Disc(Cursor - AvR, Top + AvR - 4.f, 4.5f, Hex(0xFF, 0xFF, 0xFF, .20f));
    Disc(Cursor - AvR, Top + AvR + 11.f, 9.f, Hex(0xFF, 0xFF, 0xFF, .20f));
    Zones.Add({TEXT("nav_myteam"), FBox2D(FVector2D(Cursor - 2 * AvR, Top), FVector2D(Cursor, Top + 2 * AvR))});
    Cursor -= (2.f * AvR + 22.f);

    auto Stat = [&](const FString& Value, const FString& Label, FLinearColor VC, FName Act)
    {
        const float VW = Width(Value, 19.f, 2);
        const float LW = WidthT(Label, 10.f, .20f, 0);
        const float BW = FMath::Max(VW, LW);
        Text(Value, Cursor - VW, Top, 19.f, VC, false, 2);
        TextT(Label, Cursor - LW, Top + 23.f, 10.f, T5, .20f, 0);
        if (Act != NAME_None) Zones.Add({Act, FBox2D(FVector2D(Cursor - BW, Top), FVector2D(Cursor, Top + 36.f))});
        Cursor -= (BW + 22.f);
    };
    Stat(TEXT("12,480"), TEXT("COINS"), Mint, TEXT("nav_store"));
    Stat(TEXT("24"), TEXT("LEVEL"), T2, NAME_None);

    // Divider.
    Rect(Cursor, Top + 4.f, 1.f, 28.f, FLinearColor(1.f, 1.f, 1.f, .10f));
    Cursor -= 22.f;

    const FString Handle = TEXT("AAGAM");
    const FString Sub = TEXT("PRO CLUB");
    const float HW = WidthT(Handle, 17.f, .10f, 0);
    const float SW = WidthT(Sub, 11.f, .20f, 0);
    TextT(Handle, Cursor - HW, Top, 17.f, T2, .10f, 0);
    TextT(Sub, Cursor - SW, Top + 22.f, 11.f, T5, .20f, 0);
}

void AC26HUD::Rail(int Selected)
{
    // Seven flat entries. The active one is white with a 2px mint tick; the rest
    // are one grey step down. No fills, no boxes -- the rail is a list, not a
    // stack of buttons, which is what keeps the page quiet.
    struct FEntry { FName Action; const TCHAR* Label; bool On; };
    const FEntry Items[7] = {
        {TEXT("nav_home"),     TEXT("PLAY NOW"),     Selected == 0 || (Selected >= 1 && Selected <= 4) || Selected == 12},
        {TEXT("nav_myteam"),   TEXT("SQUAD HUB"),    Selected == 5},
        {TEXT("nav_career"),   TEXT("CAREER MODE"),  Selected == 6 || Selected == 9 || Selected == 10},
        {TEXT("nav_online"),   TEXT("ONLINE"),       Selected == 8},
        {TEXT("nav_tour"),     TEXT("LEADERBOARDS"), Selected == 7},
        {TEXT("nav_store"),    TEXT("STORE"),        Selected == 13},
        {TEXT("nav_settings"), TEXT("SETTINGS"),     Selected == 11},
    };

    for (int I = 0; I < 7; ++I)
    {
        const float Y = RailTop + I * RailPitch;
        const bool P = Pressed(Items[I].Action);
        if (Items[I].On) Rect(RailX, Y + (RailPitch - 17.f) * .5f, 2.f, 17.f, Mint);
        const FLinearColor C = Items[I].On ? T1 : (P ? T2 : FLinearColor(.925f, .945f, .953f, .36f));
        TextTMid(Items[I].Label, RailX + 20.f, Y, RailPitch, 15.f, C, .20f, 0);
        Zones.Add({Items[I].Action, FBox2D(FVector2D(RailX, Y), FVector2D(RailX + 250.f, Y + RailPitch))});
    }

    TextT(TEXT("CRICKET 26 - BUILD v0.5"), RailX, 842.f, 10.f, T7, .24f, 0);
}

void AC26HUD::Shell(int Selected)
{
    TextShadow = 0.f;
    Backdrop();
    Wordmark();
    ProfileBar();
    Rail(Selected);
}

void AC26HUD::Menu()
{
    FA = Match->ScreenFade;
    NavRail(Match->MenuScreen);   // hidden automation zones; the real rail is added after
    Shell(Match->MenuScreen);

    switch (Match->MenuScreen)
    {
    case 0: Home(); break;
    case 1: Play(); break;
    case 2: Teams(); break;
    case 3: Matchup(); break;
    case 4: Toss(); break;
    case 5: Squad(); break;
    case 6: Career(); break;
    case 7: Leaderboards(); break;
    case 8: Multiplayer(); break;
    case 9: Future(3); break;
    case 10: Future(4); break;
    case 11: SettingsHub(); break;
    case 12: Help(); break;
    case 13: Store(); break;
    default: Home(); break;
    }

    Toast(false);
    if (Match->PendingConfirm != NAME_None) Confirm();
}

// ============================================================================
// SCREEN 0: MAIN HUB
// ============================================================================
void AC26HUD::Home()
{
    const int A = Match->PlayerTeam, B = 1 - Match->PlayerTeam;

    Eyebrow(TEXT("LIVE EVENT"), BayX, 250.f);
    // Two display lines on a 69px pitch; the second is the qualifier and drops
    // to the de-emphasis grey so one headline carries two levels.
    Text(TEXT("GLOBAL SUPER OVER"), BayX, 283.f, 70.f, T1, false, 3);
    Text(TEXT("CHALLENGE"), BayX, 352.f, 70.f, T5, false, 3);

    TextT(Match->TeamName(A) + TEXT("   /   ") + Match->TeamName(B), BayX, 444.f, 14.f, T4, .20f, 0);

    const float BY = 509.f;
    const float PW = BtnW(TEXT("START MATCH"), 15.f, 44.f);
    PrimaryBtn(TEXT("quickplay"), TEXT("START MATCH"), BayX, BY, PW, 54.f);
    GhostBtn(TEXT("nav_teams"), TEXT("CHANGE CLUB"), BayX + PW + 16.f,
             BY, WidthT(TEXT("CHANGE CLUB"), 12.f, .18f, 0) + 60.f, 54.f);

    HairRule(BayX, 700.f, BayW);

    const float FY = 734.f;
    float FX = BayX;
    auto Foot = [&](const TCHAR* L, const TCHAR* V, FLinearColor VC)
    {
        StatLine(L, V, FX, FY, VC);
        FX += FMath::Max(WidthT(L, 10.f, .24f, 0), Width(V, 23.f, 2)) + 80.f;
    };
    Foot(TEXT("FORMAT"), TEXT("6 BALLS"), TVal);
    Foot(TEXT("WICKETS"), TEXT("2"), TVal);
    Foot(TEXT("VENUE"), TEXT("ECLIPSE OVAL"), TVal);
    Foot(TEXT("WIN REWARD"), TEXT("2,500"), Mint);

    // Carousel dots, right-aligned on the footer line.
    float DX = BayR - 20.f - 4 * 12.f;
    Rect(DX, 744.f, 20.f, 5.f, Mint);
    for (int I = 0; I < 4; ++I) Disc(DX + 20.f + 7.f + I * 12.f, 746.5f, 2.5f, FLinearColor(1, 1, 1, .16f));
}

// ============================================================================
// SCREEN 5: SQUAD HUB
// ============================================================================
void AC26HUD::Squad()
{
    const int Team = Match->PlayerTeam;

    Eyebrow(TEXT("SQUAD HUB"), BayX, 190.f);
    Headline(TEXT("STARTING"), TEXT("XI"), BayX, 223.f, 56.f);

    // ---- featured captain column ----
    const float FX = BayX, FW = 290.f;
    float Y = 318.f;
    TextT(TEXT("CAPTAIN"), FX, Y, 10.f, T6, .28f, 0);
    Y = 341.f;
    Text(TEXT("A. RAO"), FX, Y, 42.f, T1, false, 3);
    Y = 394.f;
    TextT(Match->TeamName(Team), FX, Y, 12.f, Mint, .20f, 0);
    Y = 436.f;
    Text(TEXT("88"), FX, Y, 76.f, T1, false, 2);
    Y = 514.f;
    TextT(TEXT("OVERALL"), FX, Y, 10.f, T6, .28f, 0);

    Y = 554.f;
    const TCHAR* BarL[3] = {TEXT("BATTING"), TEXT("BOWLING"), TEXT("FIELDING")};
    const int BarV[3] = {82, 20, 70};
    for (int I = 0; I < 3; ++I)
    {
        const float RY = Y + I * 42.f;
        TextT(BarL[I], FX, RY, 10.f, T4, .22f, 0);
        const FString V = FString::FromInt(BarV[I]);
        Text(V, FX + FW - Width(V, 14.f, 2), RY - 2.f, 14.f, TVal, false, 2);
        MicroBar(FX, RY + 24.f, FW, BarV[I] / 100.f);
    }

    // ---- starting six table ----
    struct FRow { const TCHAR* Slot; const TCHAR* Name; int Bat, Bowl, Field, Ovr; };
    static const FRow Roster[2][6] = {
        {
            {TEXT("BATSMAN"),     TEXT("A. RAO"),     82, 20, 70, 88},
            {TEXT("BATSMAN"),     TEXT("K. DESAI"),   75, 15, 68, 81},
            {TEXT("ALL-ROUNDER"), TEXT("R. MEHRA"),   64, 58, 72, 77},
            {TEXT("BOWLER"),      TEXT("N. ARCHER"),  22, 85, 60, 84},
            {TEXT("KEEPER"),      TEXT("S. IYER"),    58, 10, 81, 76},
            {TEXT("BOWLER"),      TEXT("D. KOHLI"),   18, 79, 55, 79},
        },
        {
            {TEXT("BATSMAN"),     TEXT("J. HART"),    80, 18, 69, 86},
            {TEXT("BATSMAN"),     TEXT("L. REED"),    73, 14, 66, 80},
            {TEXT("ALL-ROUNDER"), TEXT("M. VALE"),    62, 56, 70, 75},
            {TEXT("BOWLER"),      TEXT("V. SEN"),     20, 83, 58, 83},
            {TEXT("KEEPER"),      TEXT("T. FOX"),     56,  9, 79, 74},
            {TEXT("BOWLER"),      TEXT("O. BLAKE"),   16, 77, 53, 77},
        },
    };
    static const FName Acts[6] = {TEXT("squad0"), TEXT("squad1"), TEXT("squad2"),
                                  TEXT("squad3"), TEXT("squad4"), TEXT("squad5")};

    const float LX = FX + FW + 56.f;
    const float SlotW = 116.f, StatW = 56.f, OvrW = 74.f;
    const float OvrR = BayR, St3R = OvrR - OvrW, St2R = St3R - StatW, St1R = St2R - StatW;

    // Column header.
    TextT(TEXT("ROLE"), LX, 318.f, 9.f, T7, .26f, 0);
    TextT(TEXT("PLAYER"), LX + SlotW, 318.f, 9.f, T7, .26f, 0);
    auto HeadR = [&](const TCHAR* L, float R)
    {
        TextT(L, R - WidthT(L, 9.f, .26f, 0), 318.f, 9.f, T7, .26f, 0);
    };
    HeadR(TEXT("BAT"), St1R); HeadR(TEXT("BWL"), St2R);
    HeadR(TEXT("FLD"), St3R); HeadR(TEXT("OVR"), OvrR);
    HairRule(LX, 341.f, BayR - LX);

    for (int I = 0; I < 6; ++I)
    {
        const auto& R = Roster[Team][I];
        const float RY = 341.f + I * 58.f;
        const bool Hi = I == 0;
        TextTMid(R.Slot, LX, RY, 58.f, 10.f, T6, .24f, 0);
        TextTMid(R.Name, LX + SlotW, RY, 58.f, 20.f, Hi ? Mint : T2, .06f, 0);
        const int Vals[3] = {R.Bat, R.Bowl, R.Field};
        const float Rights[3] = {St1R, St2R, St3R};
        for (int J = 0; J < 3; ++J)
        {
            const FString V = FString::FromInt(Vals[J]);
            TextMid(V, Rights[J] - Width(V, 15.f, 2), RY, 58.f, 15.f, TBody, false, 2);
        }
        const FString OvrS = FString::FromInt(R.Ovr);
        TextMid(OvrS, OvrR - Width(OvrS, 24.f, 2), RY, 58.f, 24.f, Hi ? Mint : T2, false, 2);
        HairRule(LX, RY + 58.f, BayR - LX);
        Zones.Add({Acts[I], FBox2D(FVector2D(LX, RY), FVector2D(BayR, RY + 58.f))});
    }

    const float AY = 727.f;
    const float PW = BtnW(TEXT("EDIT SQUAD"), 15.f, 44.f);
    PrimaryBtn(TEXT("squad_edit"), TEXT("EDIT SQUAD"), LX, AY, PW, 54.f);
    GhostBtn(TEXT("squad_auto"), TEXT("AUTO PICK"), LX + PW + 16.f, AY,
             WidthT(TEXT("AUTO PICK"), 12.f, .18f, 0) + 60.f, 54.f);
}

// ============================================================================
// SCREEN 6: CAREER MODE
// ============================================================================
void AC26HUD::Career()
{
    Eyebrow(TEXT("CAREER MODE"), BayX, 196.f);
    Headline(TEXT("ROAD TO"), TEXT("GLORY"), BayX, 229.f, 58.f);

    struct FPath { FName Action; const TCHAR* Idx; const TCHAR* Label; const TCHAR* Meta; const TCHAR* Right; bool Locked; };
    const FPath Paths[3] = {
        {TEXT("career_club"), TEXT("01"), TEXT("CLUB CAREER"),   TEXT("SEASON 1  ·  ROUND 3 OF 12"), TEXT("CONTINUE"), false},
        {TEXT("career_intl"), TEXT("02"), TEXT("INTERNATIONAL"), TEXT("UNLOCKS AT CLUB TIER 3"),     TEXT("LOCKED"),   true },
        {TEXT("career_achv"), TEXT("03"), TEXT("ACHIEVEMENTS"),  TEXT("14 OF 60 COMPLETE"),          TEXT("VIEW"),     false},
    };

    HairRule(BayX, 328.f, BayW);
    for (int I = 0; I < 3; ++I)
    {
        const float RY = 328.f + I * 94.f;
        const auto& P = Paths[I];
        const FLinearColor TitleC = P.Locked ? T6 : T2;
        const FLinearColor MetaC  = P.Locked ? T7 : T5;
        const FLinearColor RightC = P.Locked ? T7 : Mint;

        Text(P.Idx, BayX, RY + 34.f, 15.f, T7, false, 2);
        TextT(P.Label, BayX + 64.f, RY + 22.f, 25.f, TitleC, .10f, 0);
        TextT(P.Meta, BayX + 64.f, RY + 59.f, 11.f, MetaC, .22f, 0);
        TextT(P.Right, BayR - WidthT(P.Right, 11.f, .24f, 0), RY + 42.f, 11.f, RightC, .24f, 0);
        HairRule(BayX, RY + 94.f, BayW);
        Zones.Add({P.Action, FBox2D(FVector2D(BayX, RY), FVector2D(BayR, RY + 94.f))});
    }

    // Season progression.
    const float PY = 648.f;
    TextT(TEXT("SEASON PROGRESSION"), BayX, PY, 10.f, T6, .26f, 0);
    TextT(TEXT("TIER 2  ·  RISING PRO"), BayX, PY + 20.f, 21.f, T2, .10f, 0);
    const FString Pct = TEXT("62%");
    Text(Pct, BayR - Width(Pct, 25.f, 2), PY + 18.f, 25.f, Mint, false, 2);
    MicroBar(BayX, PY + 61.f, BayW, .62f);

    PrimaryBtn(TEXT("career_club"), TEXT("CONTINUE CAREER"), BayX, 745.f,
               BtnW(TEXT("CONTINUE CAREER"), 15.f, 44.f), 54.f);
}

// ============================================================================
// SCREEN 8: ONLINE MULTIPLAYER
// ============================================================================
void AC26HUD::Multiplayer()
{
    Eyebrow(TEXT("ONLINE"), BayX, 196.f);
    Headline(TEXT("MULTIPLAYER"), TEXT("ARENA"), BayX, 229.f, 56.f);

    // Standing row.
    float SX = BayX;
    auto Stand = [&](const TCHAR* L, const TCHAR* V, FLinearColor VC, int Font)
    {
        TextT(L, SX, 324.f, 10.f, T6, .26f, 0);
        Text(V, SX, 347.f, 32.f, VC, false, Font);
        SX += FMath::Max(WidthT(L, 10.f, .26f, 0), Width(V, 32.f, Font)) + 60.f;
    };
    Stand(TEXT("YOUR TIER"), TEXT("PLATINUM IV"), Mint, 3);
    Stand(TEXT("GLOBAL RANK"), TEXT("#4"), T1, 2);
    Stand(TEXT("PLAYERS ONLINE"), TEXT("12,456"), T1, 2);
    Stand(TEXT("SEASON ENDS"), TEXT("18D"), T1, 2);
    HairRule(BayX, 413.f, BayW);

    struct FMode { FName Action; const TCHAR* Title; const TCHAR* Sub; const TCHAR* RT; const TCHAR* RV; };
    const FMode Modes[3] = {
        {TEXT("quickplay"),  TEXT("QUICK MATCH"),      TEXT("Instant Super Over against a live rival"), TEXT("AVG WAIT"), TEXT("11s")},
        {TEXT("mp_ranked"),  TEXT("RANKED SEASONS"),   TEXT("Climb the tiers and earn crate rewards"),  TEXT("SEASON 4"), TEXT("18 DAYS LEFT")},
        {TEXT("mp_friends"), TEXT("PLAY WITH FRIENDS"), TEXT("Private lobbies and custom rules"),       TEXT("FRIENDS"),  TEXT("3 ONLINE")},
    };
    for (int I = 0; I < 3; ++I)
    {
        const float RY = 413.f + I * 101.f;
        const auto& M = Modes[I];
        TextT(M.Title, BayX, RY + 26.f, 23.f, T2, .10f, 0);
        Text(M.Sub, BayX, RY + 61.f, 12.f, T5, false, 1);
        TextT(M.RT, BayR - WidthT(M.RT, 10.f, .24f, 0), RY + 28.f, 10.f, T6, .24f, 0);
        TextT(M.RV, BayR - WidthT(M.RV, 11.f, .24f, 0), RY + 49.f, 11.f, Mint, .24f, 0);
        HairRule(BayX, RY + 101.f, BayW);
        Zones.Add({M.Action, FBox2D(FVector2D(BayX, RY), FVector2D(BayR, RY + 101.f))});
    }

    const float AY = 754.f;
    const float PW = BtnW(TEXT("FIND MATCH"), 15.f, 44.f);
    PrimaryBtn(TEXT("quickplay"), TEXT("FIND MATCH"), BayX, AY, PW, 54.f);
    GhostBtn(TEXT("mp_friends"), TEXT("INVITE FRIEND"), BayX + PW + 16.f, AY,
             WidthT(TEXT("INVITE FRIEND"), 12.f, .18f, 0) + 60.f, 54.f);
}

// ============================================================================
// SCREEN 7: LEADERBOARDS
// ============================================================================
void AC26HUD::Leaderboards()
{
    Eyebrow(TEXT("LEADERBOARDS"), BayX, 190.f);
    Headline(TEXT("GLOBAL"), TEXT("RANKINGS"), BayX, 223.f, 56.f);

    // Filter tabs.
    const TCHAR* Tabs[3] = {TEXT("THIS WEEK"), TEXT("SEASON"), TEXT("ALL TIME")};
    const FName TabActs[3] = {TEXT("lb_week"), TEXT("lb_season"), TEXT("lb_all")};
    float TX = BayX;
    for (int I = 0; I < 3; ++I)
    {
        const float TW = WidthT(Tabs[I], 11.f, .24f, 0);
        TextT(Tabs[I], TX, 316.f, 11.f, I == 1 ? T1 : T6, .24f, 0);
        if (I == 1) Rect(TX, 343.f, TW, 2.f, Mint);
        Zones.Add({TabActs[I], FBox2D(FVector2D(TX, 308.f), FVector2D(TX + TW, 340.f))});
        TX += TW + 34.f;
    }
    HairRule(BayX, 343.f, BayW);

    // Columns.
    const float PosW = 76.f, ClubW = 290.f, PlayedW = 90.f, PtsW = 120.f;
    const float PtsR = BayR, PlayedR = PtsR - PtsW, ClubX = PlayedR - PlayedW - ClubW;

    TextT(TEXT("RANK"), BayX, 359.f, 9.f, T7, .26f, 0);
    TextT(TEXT("PLAYER"), BayX + PosW, 359.f, 9.f, T7, .26f, 0);
    TextT(TEXT("CLUB"), ClubX, 359.f, 9.f, T7, .26f, 0);
    TextT(TEXT("PLAYED"), PlayedR - WidthT(TEXT("PLAYED"), 9.f, .26f, 0), 359.f, 9.f, T7, .26f, 0);
    TextT(TEXT("POINTS"), PtsR - WidthT(TEXT("POINTS"), 9.f, .26f, 0), 359.f, 9.f, T7, .26f, 0);

    struct FRank { const TCHAR* Pos; const TCHAR* Name; const TCHAR* Club; const TCHAR* Played; const TCHAR* Pts; int Kind; };
    static const FRank Rows[8] = {
        {TEXT("01"), TEXT("V. SHARMA"),   TEXT("MUMBAI METEORS"),     TEXT("214"), TEXT("9,840"), 1},
        {TEXT("02"), TEXT("T. BRENNAN"),  TEXT("MELBOURNE CYCLONES"), TEXT("198"), TEXT("9,515"), 0},
        {TEXT("03"), TEXT("K. OKAFOR"),   TEXT("LAGOS LIGHTNING"),    TEXT("203"), TEXT("9,302"), 0},
        {TEXT("04"), TEXT("AAGAM"),       TEXT("MUMBAI METEORS"),     TEXT("176"), TEXT("8,974"), 2},
        {TEXT("05"), TEXT("H. NAKAMURA"), TEXT("OSAKA ORBIT"),        TEXT("188"), TEXT("8,810"), 0},
        {TEXT("06"), TEXT("D. FERREIRA"), TEXT("RIO RAPTORS"),        TEXT("181"), TEXT("8,622"), 0},
        {TEXT("07"), TEXT("S. MALIK"),    TEXT("KARACHI KINGS"),      TEXT("169"), TEXT("8,455"), 0},
        {TEXT("08"), TEXT("E. HOLT"),     TEXT("LONDON LIONS"),       TEXT("174"), TEXT("8,290"), 0},
    };

    for (int I = 0; I < 8; ++I)
    {
        const float RY = 382.f + I * 53.f;
        const auto& R = Rows[I];
        const bool Me = R.Kind == 2;
        HairRule(BayX, RY, BayW, .05f);
        TextMid(R.Pos, BayX, RY, 53.f, 19.f, R.Kind == 1 ? Podium : (Me ? Mint : T5), false, 2);
        TextTMid(R.Name, BayX + PosW, RY, 53.f, 19.f, Me ? Mint : T2, .06f, 0);
        TextTMid(R.Club, ClubX, RY, 53.f, 12.f, T4, .16f, 0);
        TextMid(R.Played, PlayedR - Width(R.Played, 15.f, 2), RY, 53.f, 15.f, T5, false, 2);
        TextMid(R.Pts, PtsR - Width(R.Pts, 21.f, 2), RY, 53.f, 21.f, Me ? Mint : T2, false, 2);
    }
}

// ============================================================================
// SCREEN 13: STORE
// ============================================================================
void AC26HUD::Store()
{
    Eyebrow(TEXT("STORE"), BayX, 196.f);
    Headline(TEXT("CRATE"), TEXT("STORE"), BayX, 229.f, 58.f);

    const float OfferW = 320.f;
    const float OfferX = BayR - OfferW;
    const float CatsW = BayW - 66.f - OfferW;

    struct FCat { FName Action; const TCHAR* Label; const TCHAR* Meta; const TCHAR* Right; };
    const FCat Cats[4] = {
        {TEXT("store_packs"),    TEXT("PLAYER PACKS"), TEXT("5 NEW THIS WEEK"),    TEXT("FROM 900")},
        {TEXT("store_kit"),      TEXT("KIT & GEAR"),   TEXT("12 ITEMS AVAILABLE"), TEXT("FROM 450")},
        {TEXT("store_boosters"), TEXT("BOOSTERS"),     TEXT("2X XP FOR 24 HOURS"), TEXT("FROM 200")},
        {TEXT("store_topup"),    TEXT("COIN TOP-UP"),  TEXT("BALANCE 12,480"),     TEXT("STORE")},
    };

    HairRule(BayX, 328.f, CatsW);
    for (int I = 0; I < 4; ++I)
    {
        const float RY = 328.f + I * 96.f;
        const auto& C = Cats[I];
        TextT(C.Label, BayX, RY + 25.f, 22.f, T2, .10f, 0);
        TextT(C.Meta, BayX, RY + 58.f, 11.f, T5, .20f, 0);
        TextT(C.Right, BayX + CatsW - WidthT(C.Right, 11.f, .22f, 0), RY + 42.f, 11.f, Mint, .22f, 0);
        HairRule(BayX, RY + 96.f, CatsW);
        Zones.Add({C.Action, FBox2D(FVector2D(BayX, RY), FVector2D(BayX + CatsW, RY + 96.f))});
    }

    // Featured offer rail, separated by a single vertical hairline.
    Rect(OfferX - 42.f, 328.f, 1.f, 384.f, FLinearColor(1.f, 1.f, 1.f, .07f));
    TextT(TEXT("FEATURED OFFER"), OfferX, 328.f, 10.f, Mint, .28f, 0);
    Text(TEXT("PRESTIGE"), OfferX, 353.f, 34.f, T1, false, 3);
    Text(TEXT("BLADE"), OfferX, 388.f, 34.f, T1, false, 3);
    TextWrap(TEXT("Tournament-grade willow with a widened sweet spot and a gold maker's mark."),
             OfferX, 435.f, 12.f, T5, OfferW, false, 1);

    const FString Was = TEXT("4,800");
    Text(Was, OfferX, 535.f, 18.f, T7, false, 2);
    Rect(OfferX, 544.f, Width(Was, 18.f, 2), 1.f, T7);
    Text(TEXT("2,400"), OfferX + Width(Was, 18.f, 2) + 15.f, 523.f, 38.f, Mint, false, 2);
    TextT(TEXT("LIMITED  ·  ENDS IN 2 DAYS"), OfferX, 575.f, 10.f, T6, .24f, 0);
    PrimaryBtn(TEXT("store_buy"), TEXT("CLAIM OFFER"), OfferX, 613.f, OfferW, 54.f);
}

// ============================================================================
// SCREEN 11: SETTINGS
// ============================================================================
void AC26HUD::SettingsHub()
{
    UC26Settings* Pref = Match->Preferences;

    Eyebrow(TEXT("SETTINGS"), BayX, 158.f);
    Headline(TEXT("PREFERENCES"), TEXT(""), BayX, 191.f, 52.f);

    // Kind: 0 toggle, 1 slider, 2 cycling value.
    struct FCtl { FName Action; const TCHAR* Key; int Kind; bool On; float Pct; FString Val; };
    struct FGroup { const TCHAR* Label; const TCHAR* Sub; FCtl A, B; };

    const TCHAR* DiffNames[3] = {TEXT("EASY"), TEXT("NORMAL"), TEXT("HARD")};
    const TCHAR* QualNames[4] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA")};

    const FGroup Groups[5] = {
        {TEXT("GENERAL"), TEXT("Difficulty and match presentation"),
         {TEXT("difficulty"), TEXT("AI DIFFICULTY"), 2, false, 0.f, DiffNames[FMath::Clamp(Pref->Difficulty, 0, 2)]},
         {TEXT("hints"), TEXT("TACTICAL HINTS"), 0, Pref->Hints, 0.f, FString()}},

        {TEXT("CONTROLS"), TEXT("Gesture sensitivity and haptics"),
         {TEXT("sensitivity"), TEXT("SWIPE SENSITIVITY"), 1, false, Pref->Sensitivity / 2.f, FString::Printf(TEXT("%.1fx"), Pref->Sensitivity)},
         {TEXT("vibration"), TEXT("HAPTIC FEEDBACK"), 0, Pref->Vibration, 0.f, FString()}},

        {TEXT("AUDIO"), TEXT("Commentary, crowd and impact mix"),
         {TEXT("master"), TEXT("MASTER VOLUME"), 1, false, Pref->SoundVolume, FString::FromInt(FMath::RoundToInt(Pref->SoundVolume * 100.f))},
         {TEXT("commentary"), TEXT("LIVE COMMENTARY"), 0, Pref->CommentaryVolume > .1f, 0.f, FString()}},

        {TEXT("GRAPHICS"), TEXT("Render quality and motion comfort"),
         {TEXT("quality"), TEXT("QUALITY"), 2, false, 0.f, QualNames[FMath::Clamp(Pref->Quality, 0, 3)]},
         {TEXT("reducedmotion"), TEXT("REDUCED MOTION"), 0, Pref->ReducedMotion, 0.f, FString()}},

        {TEXT("ACCOUNT"), TEXT("Accessibility and profile options"),
         {TEXT("subtitles"), TEXT("COMMENTARY SUBTITLES"), 0, Pref->Subtitles, 0.f, FString()},
         {TEXT("crowd"), TEXT("STADIUM CROWD"), 0, Pref->CrowdVolume > .1f, 0.f, FString()}},
    };

    const float LabelW = 300.f;
    const float CtlX = BayX + LabelW;

    HairRule(BayX, 276.f, BayW);
    for (int I = 0; I < 5; ++I)
    {
        const float GY = 276.f + I * 108.f;
        const auto& G = Groups[I];
        TextT(G.Label, BayX, GY + 20.f, 18.f, T2, .16f, 0);
        Text(G.Sub, BayX, GY + 49.f, 11.f, T6, false, 1);

        for (int R = 0; R < 2; ++R)
        {
            const FCtl& C = R == 0 ? G.A : G.B;
            const float RY = GY + 20.f + R * 38.f;
            TextTMid(C.Key, CtlX, RY, 30.f, 13.f, T3, .14f, 0);

            if (C.Kind == 0)
            {
                Toggle(C.Action, BayR - 42.f, RY + 6.f, C.On);
            }
            else if (C.Kind == 1)
            {
                const float SW = 150.f;
                Slider(C.Action, BayR - SW, RY + 14.f, SW, C.Pct);
                TextMid(C.Val, BayR - SW - 16.f - Width(C.Val, 16.f, 2), RY, 30.f, 16.f, T2, false, 2);
            }
            else
            {
                TextMid(C.Val, BayR - Width(C.Val, 16.f, 2), RY, 30.f, 16.f, T2, false, 2);
                Zones.Add({C.Action, FBox2D(FVector2D(BayR - 140.f, RY), FVector2D(BayR, RY + 30.f))});
            }
        }
        HairRule(BayX, GY + 108.f, BayW);
    }
}

// ============================================================================
// SUB-FLOW SCREENS (play -> club -> matchup -> toss) + help / roadmap
// They share the hub's shell, its page header rhythm and its row language, so
// stepping into the play flow never changes the furniture.
// ============================================================================

void AC26HUD::PageHead(const FString& Kick, const FString& Title, const FString& Sub)
{
    Eyebrow(Kick, BayX, 196.f);

    // The last word of a title drops to the de-emphasis grey, the same two-tone
    // treatment the hub headlines use.
    FString A = Title, B;
    int32 SpaceIdx = INDEX_NONE;
    if (Title.FindLastChar(TEXT(' '), SpaceIdx) && SpaceIdx > 0)
    {
        A = Title.Left(SpaceIdx);
        B = Title.Mid(SpaceIdx + 1);
    }
    Headline(A, B, BayX, 229.f, 58.f);

    if (!Sub.IsEmpty()) Text(Sub, BayX, 300.f, 15.f, T5, false, 1);
}

void AC26HUD::BackBtn(FName Action)
{
    const FString L = TEXT("< BACK");
    const float W = WidthT(L, 11.f, .24f, 0);
    const bool P = Pressed(Action);
    TextT(L, BayR - W, 202.f, 11.f, P ? T1 : T6, .24f, 0);
    Zones.Add({Action, FBox2D(FVector2D(BayR - W - 14.f, 190.f), FVector2D(BayR, 222.f))});
}

// ---------------------------------------------------------------- SCREEN 1
void AC26HUD::Play()
{
    PageHead(TEXT("EXPERIENCES"), TEXT("GAME MODES"), TEXT("Pick a format, or look at what is coming next."));

    struct FMode { FName Action; const TCHAR* Idx; const TCHAR* Label; const TCHAR* Meta; const TCHAR* Right; bool Live; };
    const FMode Modes[4] = {
        {TEXT("mode_super"),  TEXT("01"), TEXT("SUPER OVER"),   TEXT("6 BALLS  ·  2 WICKETS  ·  NIGHT SHOOTOUT"), TEXT("PLAY"),     true },
        {TEXT("nav_career"),  TEXT("02"), TEXT("CAREER MODE"),  TEXT("ROAD TO GLORY  ·  SEASON 1"),               TEXT("OPEN"),     true },
        {TEXT("nav_online"),  TEXT("03"), TEXT("ONLINE ARENA"), TEXT("LIVE HEAD-TO-HEAD SUPER OVERS"),            TEXT("OPEN"),     true },
        {TEXT("nav_store"),   TEXT("04"), TEXT("CRATE STORE"),  TEXT("BATS, KITS, BOOSTERS  ·  NEW DROP"),        TEXT("BROWSE"),   true },
    };

    HairRule(BayX, 340.f, BayW);
    for (int I = 0; I < 4; ++I)
    {
        const float RY = 340.f + I * 94.f;
        const auto& M = Modes[I];
        const bool Hero = I == 0;
        Text(M.Idx, BayX, RY + 34.f, 15.f, T7, false, 2);
        TextT(M.Label, BayX + 64.f, RY + 22.f, Hero ? 27.f : 25.f, Hero ? T1 : T2, .10f, 0);
        TextT(M.Meta, BayX + 64.f, RY + 59.f, 11.f, Hero ? T4 : T5, .22f, 0);
        TextT(M.Right, BayR - WidthT(M.Right, 11.f, .24f, 0), RY + 42.f, 11.f, Mint, .24f, 0);
        HairRule(BayX, RY + 94.f, BayW);
        Zones.Add({M.Action, FBox2D(FVector2D(BayX, RY), FVector2D(BayR, RY + 94.f))});
    }

    PrimaryBtn(TEXT("mode_super"), TEXT("PLAY SUPER OVER"), BayX, 748.f,
               BtnW(TEXT("PLAY SUPER OVER"), 15.f, 44.f), 54.f);
    BackBtn(TEXT("nav_home"));
}

// ---------------------------------------------------------------- SCREEN 2
void AC26HUD::Teams()
{
    PageHead(TEXT("SETUP"), TEXT("CHOOSE CLUB"), TEXT("Your club for tonight's Super Over shootout."));

    const float ColW = (BayW - 80.f) * .5f;
    const TCHAR* Names[2][2] = {{TEXT("MUMBAI"), TEXT("METEORS")}, {TEXT("MELBOURNE"), TEXT("CYCLONES")}};
    const float Bars[2][3] = {{.88f, .84f, .82f}, {.84f, .86f, .84f}};
    const TCHAR* BarL[3] = {TEXT("BATTING"), TEXT("BOWLING"), TEXT("FIELDING")};
    const FName Picks[2] = {TEXT("pick0"), TEXT("pick1")};

    for (int T = 0; T < 2; ++T)
    {
        const bool Mine = Match->PlayerTeam == T;
        const float CX = BayX + T * (ColW + 80.f);

        HairRule(CX, 350.f, ColW);
        TextT(Mine ? TEXT("YOUR CLUB") : TEXT("OPPONENT"), CX, 372.f, 10.f, Mine ? Mint : T6, .28f, 0);
        Text(Names[T][0], CX, 396.f, 44.f, Mine ? T1 : T5, false, 3);
        Text(Names[T][1], CX, 440.f, 26.f, Mine ? T2 : T6, false, 0);
        TextT(TeamTagline(T), CX, 484.f, 11.f, Mine ? Mint : T6, .24f, 0);

        for (int I = 0; I < 3; ++I)
        {
            const float RY = 528.f + I * 42.f;
            TextT(BarL[I], CX, RY, 10.f, T4, .22f, 0);
            const FString V = FString::FromInt(FMath::RoundToInt(Bars[T][I] * 100.f));
            Text(V, CX + ColW - Width(V, 14.f, 2), RY - 2.f, 14.f, TVal, false, 2);
            MicroBar(CX, RY + 24.f, ColW, Bars[T][I]);
        }

        if (Mine) PrimaryBtn(Picks[T], TEXT("SELECTED"), CX, 676.f, ColW, 54.f);
        else      GhostBtn(Picks[T], T == 0 ? TEXT("SELECT MUMBAI") : TEXT("SELECT MELBOURNE"), CX, 676.f, ColW, 54.f);
    }

    PrimaryBtn(TEXT("nav_matchup"), TEXT("PROCEED TO MATCHUP"), BayX, 766.f,
               BtnW(TEXT("PROCEED TO MATCHUP"), 15.f, 44.f), 54.f);
    BackBtn(TEXT("nav_play"));
}

// ---------------------------------------------------------------- SCREEN 3
void AC26HUD::Matchup()
{
    PageHead(TEXT("PRE-MATCH"), TEXT("MATCHUP PREVIEW"), TEXT("Head-to-head under the floodlights."));

    const int A = Match->PlayerTeam, B = 1 - Match->PlayerTeam;

    HairRule(BayX, 356.f, BayW);
    TextT(TEXT("YOUR CLUB"), BayX, 382.f, 10.f, Mint, .28f, 0);
    Text(Match->TeamName(A), BayX, 406.f, 46.f, T1, false, 3);
    TextT(TEXT("CAPTAIN  A. RAO"), BayX, 464.f, 11.f, T5, .24f, 0);

    TextT(TEXT("VERSUS"), BayX, 512.f, 11.f, T6, .30f, 0);
    Rect(BayX, 536.f, 40.f, 1.f, FLinearColor(1.f, 1.f, 1.f, .18f));

    TextT(TEXT("OPPONENT"), BayX, 562.f, 10.f, T6, .28f, 0);
    Text(Match->TeamName(B), BayX, 586.f, 46.f, T5, false, 3);
    TextT(TEXT("CAPTAIN  J. HART"), BayX, 644.f, 11.f, T6, .24f, 0);

    HairRule(BayX, 690.f, BayW);
    float FX = BayX;
    auto Cond = [&](const TCHAR* L, const TCHAR* V)
    {
        StatLine(L, V, FX, 716.f, TVal);
        FX += FMath::Max(WidthT(L, 10.f, .24f, 0), Width(V, 23.f, 2)) + 74.f;
    };
    Cond(TEXT("VENUE"), TEXT("ECLIPSE OVAL"));
    Cond(TEXT("FORMAT"), TEXT("6 BALLS"));
    Cond(TEXT("WICKETS"), TEXT("2"));
    Cond(TEXT("LIGHTS"), TEXT("NIGHT"));

    PrimaryBtn(TEXT("matchup_go"), TEXT("TO THE TOSS"), BayX, 782.f,
               BtnW(TEXT("TO THE TOSS"), 15.f, 44.f), 54.f);
    BackBtn(TEXT("nav_teams"));
}

// ---------------------------------------------------------------- SCREEN 4
void AC26HUD::Toss()
{
    PageHead(TEXT("SUPER OVER"), TEXT("THE TOSS"), TEXT("The coin decides who bats first."));

    if (Match->TossStage == 0)
    {
        Circle(BayX + 44.f, 420.f, 44.f, FLinearColor(Mint.R, Mint.G, Mint.B, .35f), 1.f);
        TextT(TEXT("C26"), BayX + 44.f, 410.f, 20.f, Mint, .18f, 2, true);

        TextT(TEXT("CALL IT"), BayX, 512.f, 10.f, T6, .28f, 0);
        Text(TEXT("HEADS OR TAILS"), BayX, 536.f, 40.f, T1, false, 3);
        Text(TEXT("Win the call and you choose to bat or bowl first."), BayX, 592.f, 15.f, T5, false, 1);

        const float HW = WidthT(TEXT("HEADS"), 12.f, .18f, 0) + 80.f;
        GhostBtn(TEXT("tossheads"), TEXT("HEADS"), BayX, 640.f, HW, 54.f);
        GhostBtn(TEXT("tosstails"), TEXT("TAILS"), BayX + HW + 16.f, 640.f, HW, 54.f);
        PrimaryBtn(TEXT("tossflip"), TEXT("FLIP THE COIN"), BayX, 714.f,
                   BtnW(TEXT("FLIP THE COIN"), 15.f, 44.f), 54.f);
    }
    else if (Match->TossStage == 1)
    {
        const float Squash = FMath::Abs(FMath::Cos(Match->Clock * 6.f));
        const float Lift = FMath::Abs(FMath::Sin(Match->TossClock * 3.f)) * 44.f;
        Rect(BayX + 44.f - 44.f * Squash, 420.f - Lift - 44.f, 88.f * Squash, 88.f,
             FLinearColor(Mint.R, Mint.G, Mint.B, .85f));
        TextT(TEXT("IN THE AIR"), BayX, 528.f, 10.f, T6, .28f, 0);
        Text(TEXT("SPINNING"), BayX, 552.f, 44.f, T1, false, 3);
    }
    else
    {
        TextT(Match->TossPlayerWon ? TEXT("YOU WON THE TOSS") : TEXT("OPPOSITION WON THE TOSS"),
              BayX, 366.f, 10.f, Match->TossPlayerWon ? Mint : T6, .28f, 0);

        if (Match->TossPlayerWon)
        {
            Text(TEXT("CHOOSE YOUR INNINGS"), BayX, 392.f, 44.f, T1, false, 3);
            const float BW2 = WidthT(TEXT("BOWL FIRST"), 12.f, .18f, 0) + 80.f;
            if (Match->TossPlayerChoseBat)
            {
                PrimaryBtn(TEXT("batfirst"), TEXT("BAT FIRST"), BayX, 470.f, BW2, 54.f);
                GhostBtn(TEXT("bowlfirst"), TEXT("BOWL FIRST"), BayX + BW2 + 16.f, 470.f, BW2, 54.f);
            }
            else
            {
                GhostBtn(TEXT("batfirst"), TEXT("BAT FIRST"), BayX, 470.f, BW2, 54.f);
                PrimaryBtn(TEXT("bowlfirst"), TEXT("BOWL FIRST"), BayX + BW2 + 16.f, 470.f, BW2, 54.f);
            }
        }
        else
        {
            Text(Match->TossAIChoiceBat ? TEXT("THEY WILL BAT FIRST") : TEXT("THEY WILL BOWL FIRST"),
                 BayX, 392.f, 44.f, T1, false, 3);
        }

        PrimaryBtn(TEXT("tosscontinue"), TEXT("TAKE THE FIELD"), BayX, 570.f,
                   BtnW(TEXT("TAKE THE FIELD"), 15.f, 44.f), 54.f);
    }

    BackBtn(TEXT("nav_matchup"));
}

// ---------------------------------------------------------------- SCREEN 12
void AC26HUD::Help()
{
    PageHead(TEXT("TACTICS"), TEXT("HOW TO PLAY"), TEXT("Three concepts to master the Super Over."));

    static const TCHAR* Idx[3] = {TEXT("01"), TEXT("02"), TEXT("03")};
    static const TCHAR* TT[3] = {TEXT("BATTING"), TEXT("BOWLING"), TEXT("THE SUPER OVER")};
    static const TCHAR* HD[3] = {
        TEXT("Left stick sets footwork. Swipe the right side to aim direction and power, then time the contact as the ball pitches."),
        TEXT("Choose delivery type and length, drag the pitch marker onto your target, then release inside the sweet spot."),
        TEXT("Six balls. Two wickets. Every dot ball is worth gold and every boundary swings the whole match.")
    };

    HairRule(BayX, 344.f, BayW);
    for (int I = 0; I < 3; ++I)
    {
        const float RY = 344.f + I * 118.f;
        Text(Idx[I], BayX, RY + 30.f, 15.f, T7, false, 2);
        TextT(TT[I], BayX + 64.f, RY + 24.f, 24.f, T2, .10f, 0);
        TextWrap(HD[I], BayX + 64.f, RY + 60.f, 15.f, T5, BayW - 64.f, false, 1);
        HairRule(BayX, RY + 118.f, BayW);
    }

    PrimaryBtn(TEXT("quickplay"), TEXT("START PLAYING"), BayX, 726.f,
               BtnW(TEXT("START PLAYING"), 15.f, 44.f), 54.f);
    BackBtn(TEXT("nav_home"));
}

// ---------------------------------------------------------------- SCREENS 9-10
void AC26HUD::Future(int Kind)
{
    static const TCHAR* Titles[5] = {TEXT("CAREER DYNASTY"), TEXT("GLOBAL TOURNAMENTS"),
                                     TEXT("ONLINE MULTIPLAYER"), TEXT("TRAINING NETS"), TEXT("WORLD ROSTER")};
    static const TCHAR* Subs[5] = {
        TEXT("Lead your franchise to the international podium over multi-season contracts."),
        TEXT("Tiered bracket tournaments across authentic global stadiums."),
        TEXT("Ranked one-versus-one live Super Overs with global leaderboards."),
        TEXT("Drill batting timing and bowling lines with no match pressure."),
        TEXT("Authentic world teams with custom squads and licensed kits.")
    };
    const int K = FMath::Clamp(Kind, 0, 4);
    PageHead(TEXT("IN DEVELOPMENT"), Titles[K], Subs[K]);

    HairRule(BayX, 380.f, BayW);
    TextT(TEXT("COMING IN A FUTURE UPDATE"), BayX, 412.f, 11.f, Mint, .28f, 0);
    HairRule(BayX, 452.f, BayW);

    PrimaryBtn(TEXT("mode_super"), TEXT("PLAY SUPER OVER"), BayX, 500.f,
               BtnW(TEXT("PLAY SUPER OVER"), 15.f, 44.f), 54.f);
    BackBtn(TEXT("nav_home"));
}

void AC26HUD::PlayerCard(float X, float Y, float W, float H, const FString& Name, const FString& Role, int Bat, int Bowl, int Field, int Team, bool Selected)
{
    Panel(X, Y, W, H, Selected ? TeamColor(Team) : FLinearColor::Transparent);

    float CY = Y + GapBlock;
    TextFit(Name, X + GapBlock, CY, 20, WhiteAthletic, W - 2.f * GapBlock, false, 0);
    CY += LineH(20) + GapLine;
    TextFit(Role, X + GapBlock, CY, 13, TeamColor(Team), W - 2.f * GapBlock, false, 0);
    CY += LineH(13) + GapComp;
    Rule(X + GapBlock, CY, W - 2.f * GapBlock);

    // Three stat columns share the width evenly, each centred inside its column
    const float ColW = (W - 2.f * GapBlock) / 3.f;
    CY += GapBlock;
    auto StatCol = [&](int I, const TCHAR* Label, int Val)
    {
        const float ColCenter = X + GapBlock + (I + .5f) * ColW;
        Text(FString::FromInt(Val), ColCenter, CY, 20, WhiteAthletic, true, 2);
        Text(Label, ColCenter, CY + LineH(20) + GapLine, 12, SlateMuted, true, 0);
    };
    StatCol(0, TEXT("BAT"), Bat);
    StatCol(1, TEXT("BWL"), Bowl);
    StatCol(2, TEXT("FLD"), Field);
}


// ============================================================================
// GAMEPLAY HUD: 2-ROW BROADCAST SCORE HUD & OVER STRIP
// Split into 2 clean logical rows with zero cramming or collision:
// ROW 1: TEAM | SCORE | OVERS | TARGET / RUN RATE
// ROW 2: BATTER | BOWLER | REQUIREMENT
// ============================================================================
void AC26HUD::Score()
{
    // Do not draw live score bug, player strip, or over ticker during instant replay!
    if (Match->Phase == EC26Phase::Replay) return;

    const auto& S = Match->Rules.Now();
    const int Bat = Match->BattingTeam();
    const auto TC = TeamColor(Bat);

    // ------------------------------------------------------------------------
    // ROW 1: MAIN BROADCAST SCORE BAR (Top-Left, W = 520, H = 52)
    // ------------------------------------------------------------------------
    const float BX = 56.f, BY = 36.f, BW = 520.f, BH1 = 52.f;
    Rect(BX, BY, BW, BH1, SurfaceCard);
    Line(BX, BY, BX + BW, BY, HairlineSoft, 1.f);
    Line(BX, BY + BH1, BX + BW, BY + BH1, HairlineSoft, 1.f);
    Line(BX, BY, BX, BY + BH1, HairlineSoft, 1.f);
    Line(BX + BW, BY, BX + BW, BY + BH1, HairlineSoft, 1.f);
    Line(BX + 1, BY + 1, BX + BW - 1, BY + 1, HairlineGleam, 1.f);

    // Left Team Flag & Monogram (Solid Club Color Badge with 12px padding)
    const float FlagW = 84.f;
    Rect(BX, BY, FlagW, BH1, TC);
    const FString TeamShort = Match->TeamShort(Bat);
    TextMid(TeamShort, BX + FlagW * .5f, BY, BH1, 26, WhiteAthletic, true, 2);

    // Primary Score Display: DIN Condensed Bold (e.g. "299 / 9" or "24 / 0")
    const FString ScoreStr = FString::Printf(TEXT("%d / %d"), S.Runs, S.Wickets);
    TextMid(ScoreStr, BX + FlagW + 16.f, BY, BH1, 38, WhiteAthletic, false, 2);
    const float ScoreW = Width(ScoreStr, 38, 2);

    // Vertical Divider Hairline
    const float DivX = BX + FlagW + 16.f + ScoreW + 16.f;
    Line(DivX, BY + 10.f, DivX, BY + BH1 - 10.f, HairlineSoft, 1.f);

    // Overs / Balls Counter: e.g. "0.4 ov"
    const int Overs = S.LegalBalls / 6;
    const int Balls = S.LegalBalls % 6;
    const FString BallsStr = FString::Printf(TEXT("%d.%d ov"), Overs, Balls);
    TextMid(BallsStr, DivX + 16.f, BY, BH1, 22, SilverCool, false, 2);

    // Right Context Pill: Chase Target or Run Rate (Right-aligned inside Row 1)
    if (Match->Rules.Current == 1)
    {
        const FString ReqStr = FString::Printf(TEXT("NEED %d (%db)"), Match->Rules.RunsRequired(), Match->Rules.BallsRemaining());
        const float RW = Width(ReqStr, 18, 2);
        const float PillW = RW + 20.f;
        const float PillX = BX + BW - PillW - 14.f;
        Rect(PillX, BY + 9.f, PillW, 34.f, FLinearColor(Gold.R, Gold.G, Gold.B, .18f));
        Line(PillX, BY + 9.f, PillX + PillW, BY + 9.f, Gold, 1.f);
        Line(PillX, BY + 9.f + 34.f, PillX + PillW, BY + 9.f + 34.f, Gold, 1.f);
        Line(PillX, BY + 9.f, PillX, BY + 9.f + 34.f, Gold, 1.f);
        Line(PillX + PillW, BY + 9.f, PillX + PillW, BY + 9.f + 34.f, Gold, 1.f);
        TextMid(ReqStr, PillX + PillW * .5f, BY + 9.f, 34.f, 18, Gold, true, 2);
    }
    else
    {
        const float CRR = S.LegalBalls > 0 ? (float)S.Runs / (float)S.LegalBalls * 6.f : 0.f;
        const FString InfoStr = CRR > 0.1f ? FString::Printf(TEXT("CRR %.1f"), CRR) : TEXT("1st INN");
        const float IW = Width(InfoStr, 16, 0);
        TextMid(InfoStr, BX + BW - 18.f - IW * .5f, BY, BH1, 16, SlateMuted, true, 0);
    }

    // ------------------------------------------------------------------------
    // ROW 2: BATTER & BOWLER ATHLETES BAR (Docked 8px beneath Row 1, H = 36)
    // ------------------------------------------------------------------------
    const float BBW = BW, BBH = 36.f, BBY = BY + BH1 + 8.f;
    Rect(BX, BBY, BBW, BBH, SurfaceWell);
    Line(BX, BBY, BX + BBW, BBY, HairlineSoft, 1.f);
    Line(BX, BBY + BBH, BX + BBW, BBY + BBH, HairlineSoft, 1.f);
    Line(BX, BBY, BX, BBY + BBH, HairlineSoft, 1.f);
    Line(BX + BBW, BBY, BX + BBW, BBY + BBH, HairlineSoft, 1.f);
    Rect(BX, BBY, 3.5f, BBH, TC);

    // Striker Left: e.g. "★ A. RAO *" with auto-fitting up to 230px
    const float RowTextX = BX + 4.f + PadEdge;
    const FString StrikerStr = TEXT("★ ") + Match->BatterName() + TEXT(" *");
    TextFit(StrikerStr, RowTextX, BBY + (BBH - 15.f) * .5f, 15, WhiteAthletic, 230.f, false, 0);

    // Center divider in Row 2
    Line(BX + BBW * .5f, BBY + 6.f, BX + BBW * .5f, BBY + BBH - 6.f, HairlineSoft, 1.f);

    // Bowler Right: e.g. "⚡ N. ARCHER" with auto-fitting up to 230px
    const FString BowlerStr = TEXT("⚡ ") + Match->BowlerName();
    const float BwlW = FMath::Min(230.f, Width(BowlerStr, 15, 0));
    TextFit(BowlerStr, BX + BBW - PadEdge - BwlW, BBY + (BBH - 15.f) * .5f, 15, SilverCool, 230.f, false, 0);

    // Free Hit Badge (Cleanly isolated to the right of Row 1)
    if (S.FreeHit)
    {
        Tag(TEXT("● FREE HIT"), BX + BW + 16.f, BY + 12.f, true);
    }

    // Top-Right Pause Button
    Btn(TEXT("pause"), TEXT("II"), 1472.f, 36.f, 56.f, 48.f, 0);

    // ------------------------------------------------------------------------
    // 3. BROADCAST BALL-BY-BALL OVER STRIP (Docked at Top-Right)
    // ------------------------------------------------------------------------
    const float OverX = 1040.f, OverY = 36.f, OverW = 416.f, OverH = 48.f;
    Rect(OverX, OverY, OverW, OverH, SurfaceWell);
    Line(OverX, OverY, OverX + OverW, OverY, HairlineSoft, 1.f);
    Line(OverX, OverY + OverH, OverX + OverW, OverY + OverH, HairlineSoft, 1.f);
    Line(OverX, OverY, OverX, OverY + OverH, HairlineSoft, 1.f);
    Line(OverX + OverW, OverY, OverX + OverW, OverY + OverH, HairlineSoft, 1.f);

    TextMid(TEXT("THIS OVER"), OverX + 16.f, OverY, OverH, 13, SlateMuted, false, 0);

    // 6 Ball Capsules with 12px gap
    float BallX = OverX + 104.f;
    int Start = FMath::Max(0, int(S.Ledger.size()) - 6);
    for (int I = Start; I < int(S.Ledger.size()); ++I)
    {
        const auto& O = S.Ledger[I];
        const bool W = O.Wicket != C26::Dismissal::None;
        const bool BoundarySix = O.BatRuns >= 6;
        const bool BoundaryFour = O.BatRuns >= 4 && !BoundarySix;
        const bool Dot = O.BatRuns == 0 && O.WideRuns == 0 && !O.NoBall && !W;

        FString V = W ? TEXT("W") : (O.WideRuns ? TEXT("Wd") : (O.NoBall ? TEXT("Nb") : (Dot ? TEXT("•") : FString::FromInt(O.BatRuns + O.Byes + O.LegByes))));

        FLinearColor BallBg = W ? Crimson : (BoundarySix ? Gold : (BoundaryFour ? TurfGreen : (Dot ? SurfacePill : WhiteAthletic)));
        FLinearColor BallText = (W || BoundaryFour) ? WhiteAthletic : ((BoundarySix || !Dot) ? DarkLabel : SlateMuted);

        Rect(BallX, OverY + 8.f, 36.f, 32.f, BallBg);
        Line(BallX, OverY + 8.f, BallX + 36.f, OverY + 8.f, HairlineSoft, 1.f);
        TextMid(V, BallX + 18.f, OverY + 8.f, 32.f, 16, BallText, true, 2);

        BallX += 48.f;
    }
}

// ============================================================================
// GAMEPLAY CONTROLS & ERGONOMIC TOUCH ZONES
// Pro broadcast touch interface with zero obstruction of athlete and pitch.
// ============================================================================
void AC26HUD::Controls()
{
    const auto Phase = Match->Phase;

    if (Phase == EC26Phase::Ready || Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery)
    {
        if (Match->PlayerBatting())
        {
            // Left Stick Footwork (Ergonomic Translucent Dial)
            const float StickX = 140.f, StickY = 740.f;
            Circle(StickX, StickY, 52, FLinearColor(.12f, .18f, .26f, .35f), 1.5f);
            Line(StickX - 52, StickY, StickX + 52, StickY, FLinearColor(1.f, 1.f, 1.f, .10f), 1.f);
            Line(StickX, StickY - 52, StickX, StickY + 52, FLinearColor(1.f, 1.f, 1.f, .10f), 1.f);
            Circle(StickX + Match->Footwork * 32.f, StickY, 18, Gold, 2.5f);
            Text(TEXT("FOOTWORK"), StickX, StickY + 64.f, 14, SlateMuted, true, 0);

            // Right Touch Shot Area: intentionally undrawn. The batting guide
            // ring and its "hold anywhere right" caption were removed; the shot
            // zone is the whole right half and needs no on-screen furniture.

            // 3-Segment Shot Intent Switcher: [ LOFT | GROUND | DEFEND ]
            const float SwitchX = 1240.f, SwitchY = 620.f, SwitchW = 280.f, SwitchH = 44.f;
            const float SegW = SwitchW / 3.f;
            Rect(SwitchX, SwitchY, SwitchW, SwitchH, SurfaceWell);
            Line(SwitchX, SwitchY, SwitchX + SwitchW, SwitchY, HairlineSoft, 1.f);
            Line(SwitchX, SwitchY + SwitchH, SwitchX + SwitchW, SwitchY + SwitchH, HairlineSoft, 1.f);
            Line(SwitchX, SwitchY, SwitchX, SwitchY + SwitchH, HairlineSoft, 1.f);
            Line(SwitchX + SwitchW, SwitchY, SwitchX + SwitchW, SwitchY + SwitchH, HairlineSoft, 1.f);

            // Loft Button (Segment 0)
            const bool IsLoft = Match->Intent.Loft;
            if (IsLoft) Rect(SwitchX, SwitchY, SegW, SwitchH, FLinearColor(Gold.R, Gold.G, Gold.B, .25f));
            Btn(TEXT("loft"), TEXT("LOFT"), SwitchX, SwitchY, SegW, SwitchH, IsLoft ? 3 : 0, IsLoft);

            // Ground Shot (Segment 1, Default)
            const bool IsGround = !Match->Intent.Loft && !Match->Intent.Defend;
            if (IsGround) Rect(SwitchX + SegW, SwitchY, SegW, SwitchH, FLinearColor(TurfGreen.R, TurfGreen.G, TurfGreen.B, .20f));
            TextMid(TEXT("GROUND"), SwitchX + SegW + SegW * .5f, SwitchY, SwitchH, 15, IsGround ? TurfGreen : SlateMuted, true, 0);

            // Defend Button (Segment 2)
            const bool IsDefend = Match->Intent.Defend;
            if (IsDefend) Rect(SwitchX + 2 * SegW, SwitchY, SegW, SwitchH, FLinearColor(.04f, .47f, .95f, .25f));
            Btn(TEXT("defend"), TEXT("DEFEND"), SwitchX + 2 * SegW, SwitchY, SegW, SwitchH, IsDefend ? 3 : 0, IsDefend);

            if (Phase == EC26Phase::Ready)
            {
                Btn(TEXT("ready"), TEXT("READY TO FACE  >"), 1240, 520, 280, 56, 1);
            }
        }
        else
        {
            // ================================================================
            // BOWLING CONTROLS: Complete Tactical Planning & Execution Panel
            // ================================================================
            const auto& Plan = Phase == EC26Phase::Ready ? Match->Bowling : Match->LockedBowling;
            if (Phase == EC26Phase::Ready)
            {
                // ---- A. DELIVERY TYPE CAROUSEL ----
                const float CarX = 56.f, CarY = 400.f, CarW = 310.f, CarH = 52.f;
                Panel(CarX, CarY, CarW, CarH, Gold);
                Btn(TEXT("delivery_prev"), TEXT("<"), CarX + 8.f, CarY + 6.f, 40.f, 40.f, 0);
                {
                    const FString DN = FString(C26Delivery::Name(Match->BowlingPlan.Type));
                    TextMid(DN, CarX + CarW * 0.5f, CarY, CarH, 22, Gold, true, 0);
                }
                Btn(TEXT("delivery_next"), TEXT(">"), CarX + CarW - 48.f, CarY + 6.f, 40.f, 40.f, 0);

                // Movement indicator beside delivery name
                {
                    const EC26Movement Mov = C26Delivery::MovementOf(Match->BowlingPlan.Type);
                    if (Mov != EC26Movement::None)
                    {
                        const float Dir = C26Delivery::DirectionIsFree(Match->BowlingPlan.Type)
                            ? Match->BowlingPlan.MovementDirection : C26Delivery::NaturalDirection(Match->BowlingPlan.Type);
                        const float ArrowX = CarX + CarW + 12.f;
                        const float ArrowY = CarY + CarH * 0.5f;
                        const float ArrowLen = 22.f;
                        const FLinearColor ArrowCol = Mov == EC26Movement::ReverseSwing
                            ? FLinearColor(.90f, .55f, .15f, .85f) : FLinearColor(Gold.R, Gold.G, Gold.B, .70f);
                        Line(ArrowX, ArrowY, ArrowX + Dir * ArrowLen, ArrowY - 8.f, ArrowCol, 2.f);
                        Line(ArrowX + Dir * ArrowLen, ArrowY - 8.f, ArrowX + Dir * ArrowLen - Dir * 6.f, ArrowY - 14.f, ArrowCol, 2.f);
                        Line(ArrowX + Dir * ArrowLen, ArrowY - 8.f, ArrowX + Dir * ArrowLen - Dir * 6.f, ArrowY - 2.f, ArrowCol, 2.f);
                    }
                }

                // ---- B. MOVEMENT DIAL (Visual) ----
                // The delivery-plan readout that used to sit here was removed:
                // the planning screen now offers the delivery TYPE as the only
                // selectable option, and everything else is a live control.
                {
                    const float DX = Match->DialCentreX;
                    const float DY = Match->DialCentreY;
                    const float DR = 78.f;

                    Circle(DX, DY, DR, FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .25f), 1.5f);
                    Circle(DX, DY, DR + 1.f, FLinearColor(0.f, 0.f, 0.f, .30f), 2.5f);

                    const int ArcSegs = 32;
                    const int ArcFill = FMath::CeilToInt(Match->BowlingPlan.MovementMagnitude * ArcSegs);
                    for (int I = 0; I < ArcSegs; ++I)
                    {
                        const float A1 = -PI * 0.5f + I * 2.f * PI / ArcSegs;
                        const float A2 = -PI * 0.5f + (I + 1) * 2.f * PI / ArcSegs;
                        const bool On = I < ArcFill;
                        Line(DX + (DR - 8.f) * FMath::Cos(A1), DY + (DR - 8.f) * FMath::Sin(A1),
                             DX + (DR - 8.f) * FMath::Cos(A2), DY + (DR - 8.f) * FMath::Sin(A2),
                             On ? FLinearColor(Gold.R, Gold.G, Gold.B, .75f) : FLinearColor(1.f, 1.f, 1.f, .08f),
                             On ? 3.f : 1.5f);
                    }

                    const float DirVal = C26Delivery::DirectionIsFree(Match->BowlingPlan.Type)
                        ? Match->BowlingPlan.MovementDirection : C26Delivery::NaturalDirection(Match->BowlingPlan.Type);
                    const float ArrowMag = Match->BowlingPlan.MovementMagnitude;
                    if (FMath::Abs(DirVal) > 0.05f || ArrowMag > 0.05f)
                    {
                        const float ArrowLen = FMath::Clamp(ArrowMag, 0.15f, 1.f) * (DR - 14.f);
                        const FVector2D ArrowDir(DirVal, -0.3f);
                        const FVector2D ArrowN = ArrowDir.IsNearlyZero() ? FVector2D(1.f, 0.f) : ArrowDir.GetSafeNormal();
                        const FVector2D Tip(DX + ArrowN.X * ArrowLen, DY + ArrowN.Y * ArrowLen);
                        const bool bFixed = !C26Delivery::DirectionIsFree(Match->BowlingPlan.Type);
                        const FLinearColor ACol = bFixed
                            ? FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .65f)
                            : FLinearColor(Gold.R, Gold.G, Gold.B, .90f);
                        Line(DX, DY, Tip.X, Tip.Y, FLinearColor(0.f, 0.f, 0.f, .50f), 5.f);
                        Line(DX, DY, Tip.X, Tip.Y, ACol, 2.5f);
                        const FVector2D Perp(-ArrowN.Y, ArrowN.X);
                        Line(Tip.X, Tip.Y, Tip.X - ArrowN.X * 10.f + Perp.X * 6.f, Tip.Y - ArrowN.Y * 10.f + Perp.Y * 6.f, ACol, 2.f);
                        Line(Tip.X, Tip.Y, Tip.X - ArrowN.X * 10.f - Perp.X * 6.f, Tip.Y - ArrowN.Y * 10.f - Perp.Y * 6.f, ACol, 2.f);
                    }
                    else
                    {
                        Circle(DX, DY, 6.f, FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .40f), 1.5f);
                    }

                    const FString MovLabel = Match->GetMovementText();
                    TextFit(MovLabel, DX - DR, DY + DR + 10.f, 12, SlateMuted, DR * 2.f, true, 0);
                    if (!C26Delivery::DirectionIsFree(Match->BowlingPlan.Type))
                        TextFit(TEXT("DIRECTION LOCKED"), DX - DR, DY + DR + 26.f, 10, FLinearColor(SlateMuted.R, SlateMuted.G, SlateMuted.B, .55f), DR * 2.f, true, 0);
                }

                // ---- D. PACE SLIDER (Visual) ----
                {
                    const float TX = Match->PaceTrackX;
                    const float TW = Match->PaceTrackW;
                    const float TY = Match->PaceTrackY;
                    const float TH_S = 12.f;

                    float MinKph, MaxKph;
                    Match->PaceRangeKph(MinKph, MaxKph);
                    const float PaceN = Match->BowlingPlan.PaceNormalized;
                    const float CurKph = Match->PlannedKph();
                    const float Strain = C26Delivery::EffortStrain(PaceN);

                    Rect(TX, TY, TW, TH_S, SurfaceWell);
                    Line(TX, TY, TX + TW, TY, HairlineSoft, 1.f);
                    Line(TX, TY + TH_S, TX + TW, TY + TH_S, HairlineSoft, 1.f);

                    const FLinearColor FillCol = Strain > 0.5f ? Crimson
                        : (Strain > 0.f ? FLinearColor(Gold.R, Gold.G, Gold.B, .70f) : FLinearColor(TurfGreen.R, TurfGreen.G, TurfGreen.B, .55f));
                    Rect(TX, TY, TW * PaceN, TH_S, FLinearColor(FillCol.R, FillCol.G, FillCol.B, .40f));

                    const float ThumbX = TX + TW * PaceN;
                    Rect(ThumbX - 4.f, TY - 6.f, 8.f, TH_S + 12.f, FillCol);
                    Rect(ThumbX - 2.f, TY - 4.f, 4.f, TH_S + 8.f, WhiteAthletic);

                    Text(FString::Printf(TEXT("%.0f"), MinKph), TX - 8.f, TY - 22.f, 11, SlateMuted, false, 0);
                    Text(FString::Printf(TEXT("%.0f"), MaxKph), TX + TW - 28.f, TY - 22.f, 11, SlateMuted, false, 0);
                    Text(FString::Printf(TEXT("%.0f KM/H"), CurKph), ThumbX, TY - 26.f, 14, FillCol, true, 0);
                    Text(TEXT("PACE"), TX + TW * 0.5f, TY + TH_S + 8.f, 12, SlateMuted, true, 0);
                    if (Strain > 0.f)
                        Text(FString::Printf(TEXT("HIGH EFFORT  \u2022  -%d%% ACCURACY"), int(Strain * 100.f)),
                             TX + TW * 0.5f, TY + TH_S + 24.f, 10, FLinearColor(Crimson.R, Crimson.G, Crimson.B, .70f), true, 0);
                }

                // ---- E. TRAJECTORY PREVIEW (project 3D spline to screen) ----
                {
                    APlayerController* PC = GetOwningPlayerController();
                    if (PC && Match->TrajectoryPreview.Num() > 1)
                    {
                        const EC26Movement Mov = C26Delivery::MovementOf(Match->BowlingPlan.Type);
                        FLinearColor PreCol;
                        switch (Mov)
                        {
                        case EC26Movement::Swing:        PreCol = FLinearColor(.40f, .75f, 1.f, .35f); break;
                        case EC26Movement::ReverseSwing: PreCol = FLinearColor(.90f, .55f, .20f, .35f); break;
                        case EC26Movement::Seam:         PreCol = FLinearColor(.80f, .90f, .30f, .35f); break;
                        case EC26Movement::Spin:         PreCol = FLinearColor(.70f, .40f, .90f, .35f); break;
                        default:                         PreCol = FLinearColor(.70f, .70f, .70f, .25f); break;
                        }
                        FVector2D PrevD;
                        bool bPrevValid = false;
                        for (int I = 0; I < Match->TrajectoryPreview.Num(); ++I)
                        {
                            FVector2D Px;
                            if (!PC->ProjectWorldLocationToScreen(Match->TrajectoryPreview[I], Px)) { bPrevValid = false; continue; }
                            const FVector2D D = ToDesign(Px);
                            if (bPrevValid)
                            {
                                const bool PostBounce = Match->TrajectoryPreviewBounce >= 0 && I > Match->TrajectoryPreviewBounce;
                                const float Thick = PostBounce ? 1.5f : 2.5f;
                                FLinearColor SegCol = PreCol;
                                if (PostBounce && (Mov == EC26Movement::Seam || Mov == EC26Movement::Spin))
                                    SegCol = FLinearColor(.90f, .80f, .20f, .40f);
                                if (!PostBounce || (I % 2 == 0))
                                    Line(PrevD.X, PrevD.Y, D.X, D.Y, SegCol, Thick);
                            }
                            if (Match->TrajectoryPreviewBounce >= 0 && I == Match->TrajectoryPreviewBounce)
                            {
                                Circle(D.X, D.Y, 5.f, FLinearColor(Gold.R, Gold.G, Gold.B, .55f), 2.f);
                            }
                            PrevD = D;
                            bPrevValid = true;
                        }
                    }
                }

                // ---- F. AROUND THE WICKET TOGGLE ----
                // The quick presets (YORKER / 4TH OFF / BOUNCER / WIDE Y / SL CUT /
                // IN YORK) were removed: the delivery TYPE carousel above is the
                // only selectable option on this screen. The toggle now sits
                // directly under it so the column reads as one control.
                {
                    const FString WicketStr = Match->BowlingPlan.bAroundWicket ? TEXT("AROUND WICKET") : TEXT("OVER WICKET");
                    Btn(TEXT("around"), WicketStr, 56.f, 466.f, 156.f, 34.f, 0, Match->BowlingPlan.bAroundWicket);
                }

                // ---- H. LAST BALL GHOST ----
                if (Match->bHasLastPitch)
                {
                    APlayerController* PC = GetOwningPlayerController();
                    if (PC)
                    {
                        FVector2D GhostPx;
                        if (PC->ProjectWorldLocationToScreen(Match->LastActualPitch, GhostPx))
                        {
                            const FVector2D G = ToDesign(GhostPx);
                            if (G.X > -60.f && G.X < 1660.f && G.Y > -60.f && G.Y < 960.f)
                            {
                                Circle(G.X, G.Y, 8.f, FLinearColor(Crimson.R, Crimson.G, Crimson.B, .28f), 1.5f);
                                Circle(G.X, G.Y, 3.f, FLinearColor(Crimson.R, Crimson.G, Crimson.B, .18f), 1.f);
                                Text(TEXT("LAST"), G.X, G.Y - 18.f, 9, FLinearColor(Crimson.R, Crimson.G, Crimson.B, .35f), true, 0);
                            }
                        }
                    }
                }

                // ---- I. START RUN-UP BUTTON ----
                Btn(TEXT("ready"), TEXT("START RUN-UP  >"), 1240, 810, 280, 60, 1);

                // ---- J. INSTRUCTION HINT ----
                // Docked to the bottom safe line rather than the raw canvas edge.
                TextFit(TEXT("DRAG PITCH TO AIM  \u2022  DIAL = MOVEMENT  \u2022  SLIDER = PACE"), 800.f,
                        SafeBot - LineH(11), 11, FLinearColor(SlateMuted.R, SlateMuted.G, SlateMuted.B, .60f),
                        ContentW, true, 0);
            }
            else // RunUp or Delivery
            {
                // ================================================================
                // RELEASE BAR: Properly zoned with labeled Perfect|NoBall boundary
                // ================================================================
                const float MeterX = 420.f, MeterY = 808.f, MeterW = 660.f, MeterH = 32.f;
                const auto& Bar = Match->ActiveBar;
                const float Meter01 = Match->BowlingMeter();

                // TooEarly zone
                Rect(MeterX, MeterY, MeterW * Bar.EarlyStart, MeterH, FLinearColor(.06f, .08f, .12f, .70f));
                // Early zone
                const float EarlyW = Bar.GoodStart - Bar.EarlyStart;
                Rect(MeterX + MeterW * Bar.EarlyStart, MeterY, MeterW * EarlyW, MeterH,
                     FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .12f));
                // Good zone
                const float GoodW = Bar.PerfectStart - Bar.GoodStart;
                Rect(MeterX + MeterW * Bar.GoodStart, MeterY, MeterW * GoodW, MeterH,
                     FLinearColor(Gold.R, Gold.G, Gold.B, .18f));
                // Perfect zone
                const float PerfW = Bar.NoBallStart - Bar.PerfectStart;
                Rect(MeterX + MeterW * Bar.PerfectStart, MeterY, MeterW * PerfW, MeterH,
                     FLinearColor(TurfGreen.R, TurfGreen.G, TurfGreen.B, .30f));
                // NoBall zone
                const float NBW = 1.f - Bar.NoBallStart;
                Rect(MeterX + MeterW * Bar.NoBallStart, MeterY, MeterW * NBW, MeterH,
                     FLinearColor(Crimson.R, Crimson.G, Crimson.B, .30f));

                // Zone boundary lines
                Line(MeterX + MeterW * Bar.EarlyStart, MeterY, MeterX + MeterW * Bar.EarlyStart, MeterY + MeterH, HairlineSoft, 1.f);
                Line(MeterX + MeterW * Bar.GoodStart, MeterY, MeterX + MeterW * Bar.GoodStart, MeterY + MeterH, FLinearColor(Gold.R, Gold.G, Gold.B, .30f), 1.f);
                Line(MeterX + MeterW * Bar.PerfectStart, MeterY, MeterX + MeterW * Bar.PerfectStart, MeterY + MeterH, FLinearColor(TurfGreen.R, TurfGreen.G, TurfGreen.B, .50f), 1.f);
                // THE critical line: Perfect|NoBall boundary
                Line(MeterX + MeterW * Bar.NoBallStart, MeterY - 4.f, MeterX + MeterW * Bar.NoBallStart, MeterY + MeterH + 4.f,
                     FLinearColor(Crimson.R, Crimson.G, Crimson.B, .85f), 2.5f);

                // Outer border
                Line(MeterX, MeterY, MeterX + MeterW, MeterY, HairlineSoft, 1.f);
                Line(MeterX, MeterY + MeterH, MeterX + MeterW, MeterY + MeterH, HairlineSoft, 1.f);
                Line(MeterX, MeterY, MeterX, MeterY + MeterH, HairlineSoft, 1.f);
                Line(MeterX + MeterW, MeterY, MeterX + MeterW, MeterY + MeterH, HairlineSoft, 1.f);

                // Zone labels above bar
                TextMid(TEXT("EARLY"), MeterX + MeterW * (Bar.EarlyStart + EarlyW * 0.5f), MeterY - 18.f, 16.f, 10, SlateMuted, true, 0);
                TextMid(TEXT("GOOD"), MeterX + MeterW * (Bar.GoodStart + GoodW * 0.5f), MeterY - 18.f, 16.f, 11, FLinearColor(Gold.R, Gold.G, Gold.B, .70f), true, 0);
                TextMid(TEXT("PERFECT"), MeterX + MeterW * (Bar.PerfectStart + PerfW * 0.5f), MeterY - 18.f, 16.f, 12, TurfGreen, true, 0);
                TextMid(TEXT("NO BALL"), MeterX + MeterW * (Bar.NoBallStart + NBW * 0.5f), MeterY - 18.f, 16.f, 10, Crimson, true, 0);

                // Moving needle or locked release
                if (!Match->ReleaseLocked)
                {
                    const float NeedleX = MeterX + MeterW * Meter01;
                    Rect(NeedleX - 3.f, MeterY - 6.f, 6.f, MeterH + 12.f, FLinearColor(0.f, 0.f, 0.f, .60f));
                    Rect(NeedleX - 2.f, MeterY - 5.f, 4.f, MeterH + 10.f, WhiteAthletic);
                    Rect(NeedleX - 1.f, MeterY - 4.f, 2.f, MeterH + 8.f, Gold);
                }
                else
                {
                    const float LockX = MeterX + MeterW * Match->ReleaseMeterValue;
                    const FLinearColor LockCol = Match->bBowlingNoBall ? Crimson
                        : (Match->ReleaseBandQuality > 0.8f ? TurfGreen
                        : (Match->ReleaseBandQuality > 0.55f ? Gold : SilverCool));
                    Rect(LockX - 3.f, MeterY - 6.f, 6.f, MeterH + 12.f, FLinearColor(0.f, 0.f, 0.f, .60f));
                    Rect(LockX - 2.f, MeterY - 5.f, 4.f, MeterH + 10.f, LockCol);

                    // Release band feedback badge, lifted clear of the zone labels
                    const FString BandStr = Match->GetReleaseBandName();
                    const float FBW = Width(BandStr, 24, 0) + 2.f * Sp24;
                    Rect(800.f - FBW * 0.5f, MeterY - 66.f, FBW, 38.f, SurfaceWell);
                    Line(800.f - FBW * 0.5f, MeterY - 66.f, 800.f + FBW * 0.5f, MeterY - 66.f, LockCol, 1.5f);
                    TextMidFit(BandStr, 800.f, MeterY - 66.f, 38.f, 24, LockCol, FBW - 2.f * PadEdge, true, 0);

                    if (Match->LastActualKph > 10.f)
                    {
                        Text(FString::Printf(TEXT("%.1f KM/H"), Match->LastActualKph),
                             800.f, MeterY + MeterH + GapItem, 16, WhiteAthletic, true, 0);
                    }
                }

                // Delivery info compact readout during run-up, stacked above the
                // bar on its own leading so the two lines cannot touch.
                {
                    const FString DelName = FString(C26Delivery::Name(Match->LockedBowling.Type));
                    const float InfoX = 56.f;
                    const float Info2Y = MeterY - GapLine - LineH(12);
                    const float Info1Y = Info2Y - GapLine - LineH(14);
                    TextFit(DelName, InfoX, Info1Y, 14, SilverCool, 360.f, false, 0);
                    TextFit(Match->GetDeliveryLengthName() + TEXT("  \u2022  ") + Match->GetDeliveryLineName(),
                            InfoX, Info2Y, 12, SlateMuted, 360.f, false, 0);
                }

                // Touch hint
                if (!Match->ReleaseLocked && Phase == EC26Phase::RunUp)
                    TextFit(TEXT("TAP ANYWHERE TO RELEASE"), 800.f, MeterY + MeterH + GapComp, 12,
                            FLinearColor(SlateMuted.R, SlateMuted.G, SlateMuted.B, .60f), ContentW, true, 0);
            }
        }
    }

    if (Phase == EC26Phase::InPlay)
    {
        // Broadcast Shot Timing Feedback Badge (EC26Timing order: Perfect, Good, Early, Late, Edge, Miss)
        if (Match->PhaseTime < .95f)
        {
            // Release timing is classified from LastContact.TimingDeltaMs - literally
            // the millisecond delta the gesture measured and handed to the bat. The
            // meter and the gameplay result are the same number, never two.
            const int Diff = Match->Preferences ? Match->Preferences->Difficulty : 1;
            const float WScale = C26Controls::TimingWindowScale(Diff);
            const auto& GT = Match->GestureTuning;
            const EC26ReleaseTiming RT = C26Controls::ReleaseTimingFromDelta(Match->LastContact.TimingDeltaMs,
                GT.PerfectWindowMs * WScale, GT.GoodWindowMs * WScale, GT.VeryEarlyLateMs * WScale,
                Match->Tuning.ContactWindow * 1000.f * WScale);
            FString TStr = C26Controls::ReleaseTimingName(RT);
            FLinearColor TimingCol = RT == EC26ReleaseTiming::Perfect ? Gold
                : (RT == EC26ReleaseTiming::Good ? TurfGreen : (RT == EC26ReleaseTiming::NoShot ? SlateMuted : SilverCool));
            if (Match->LastContact.Timing == EC26Timing::Miss) { TStr = TEXT("BEATEN"); TimingCol = SlateMuted; }
            else if (Match->LastContact.Timing == EC26Timing::Edge) { TStr += TEXT("  •  EDGE"); TimingCol = Crimson; }
            if (!TStr.IsEmpty())
            {
                const float TW = Width(TStr, 28, 0) + 48.f;
                Rect(800.f - TW * .5f, 190.f, TW, 44.f, SurfaceWell);
                Line(800.f - TW * .5f, 190.f, 800.f + TW * .5f, 190.f, TimingCol, 1.5f);
                TextMid(TStr, 800.f, 190.f, 44.f, 28, TimingCol, true, 0);
            }
        }

        if (Match->Running)
        {
            Text(FString::Printf(TEXT("%d COMPLETED"), Match->CompletedRuns), 800, 680, 26, Gold, true, 2);
        }
    }

    if (Phase == EC26Phase::Reaction)
    {
        // High-impact Broadcast Event Banner across center. Sized off its own
        // two lines so the stinger keeps equal padding above and below them.
        const auto Edge = Match->Callout == TEXT("WICKET") ? Crimson : (Match->Callout == TEXT("SIX") ? Gold : TurfGreen);
        const float StingerW = 760.f, StingerH = 168.f, StingerY = 366.f;
        Panel(800.f - StingerW * .5f, StingerY, StingerW, StingerH, Edge);

        TextFit(Match->Callout, 800, StingerY + GapBlock, 56, Edge, StingerW - 2.f * PadCard, true, 3);
        TextFit(Match->Detail, 800, StingerY + GapBlock + LineH(56) + GapLine, 20, WhiteAthletic,
                StingerW - 2.f * PadCard, true, 0);
    }

    if (Phase == EC26Phase::Replay)
    {
        const float Speed = Match->Director ? Match->Director->ReplaySpeed() : 0.5f;
        const bool Outro = Match->Director ? Match->Director->IsReplayOutro : false;
        const float Alpha = Outro ? FMath::Clamp(1.f - Match->Director->ReplayOutroAlpha, 0.f, 1.f) : 1.f;

        // Dedicated Broadcast Replay Pill Badge at Top-Left (zero collision with any score bug).
        // The pill is sized from its own two labels, so the speed readout can never
        // run into the REPLAY legend and the trailing inset always clears the rim.
        FLinearColor PillCoral = Crimson;
        PillCoral.A *= Alpha;
        FLinearColor SubCol = SilverCool;
        SubCol.A *= Alpha;

        const FString RepLabel = TEXT("●  REPLAY");
        const FString SlowLabel = FString::Printf(TEXT("%.1fx SLOW MOTION"), Speed);
        const float RepW = Width(RepLabel, 19, 0);
        const float PillX = 56.f, PillY = 36.f, PillH = 44.f;
        const float PillW = PadEdge + RepW + GapItem + Width(SlowLabel, 13, 0) + PadEdge;

        Rect(PillX, PillY, PillW, PillH, FLinearColor(Void.R, Void.G, Void.B, .90f * Alpha));
        Line(PillX, PillY, PillX + PillW, PillY, PillCoral, 1.5f);
        Line(PillX, PillY, PillX, PillY + PillH, PillCoral, 2.5f);

        const float RepX = PillX + PadEdge;
        TextMid(RepLabel, RepX, PillY, PillH, 19, PillCoral, false, 0);
        const float SlowX = RepX + RepW + GapItem;
        TextMidFit(SlowLabel, SlowX, PillY, PillH, 13, SubCol,
                   PillX + PillW - PadEdge - SlowX, false, 0);

        // Broadcast Viewfinder Corner Brackets
        const float BLen = 32.f;
        const FLinearColor BCol(1.f, 1.f, 1.f, 0.25f * Alpha);
        Rect(44, 44, BLen, 2, BCol);
        Rect(44, 44, 2, BLen, BCol);
        Rect(1556 - BLen, 44, BLen, 2, BCol);
        Rect(1556 - 2, 44, 2, BLen, BCol);
        Rect(44, 856 - 2, BLen, 2, BCol);
        Rect(44, 856 - BLen, 2, BLen, BCol);
        Rect(1556 - BLen, 856 - 2, BLen, 2, BCol);
        Rect(1556 - 2, 856 - BLen, 2, BLen, BCol);

        // Dedicated Replay Skip Button positioned cleanly at bottom right with generous margins
        Btn(TEXT("skip"), TEXT("SKIP REPLAY  >"), 1340, 800, 200, 52, 0);

        // Smooth dissolve veil during replay outro
        if (Outro)
        {
            const float Fade = Match->Director->ReplayOutroAlpha;
            Rect(0, 0, 1600, 900, FLinearColor(0.f, 0.f, 0.f, Fade * 0.40f));
        }
    }
}

// ============================================================================
// SCORECARD TABLE: Strict column alignment
// Batting: BATTER | DISMISSAL | R | B | 4s | 6s | SR
// Bowling: BOWLER | O | M | R | W | ECON
// ============================================================================
void AC26HUD::ScorecardTable(float X, float Y, float W, float H)
{
    const auto& S0 = Match->Rules.Scores[0];
    const auto& S1 = Match->Rules.Scores[1];

    // Table Header Bar (BATTER | HOW OUT | R | B | 4s | 6s | SR)
    const float HdrH = 30.f;
    Rect(X, Y, W, HdrH, SurfaceWell);
    Line(X, Y, X + W, Y, HairlineSoft, 1.f);
    Line(X, Y + HdrH, X + W, Y + HdrH, HairlineSoft, 1.f);

    const float ColName = X + GapBlock;
    const float ColOut  = X + 232.f;
    // The five numeric columns share the remaining width on an even pitch, so
    // the table fills its card instead of bunching against the left edge.
    const float NumL = X + 460.f, NumR = X + W - GapBlock;
    const float NumPitch = (NumR - NumL) / 4.f;
    const float ColR  = NumL;
    const float ColB  = NumL + NumPitch;
    const float Col4s = NumL + 2.f * NumPitch;
    const float Col6s = NumL + 3.f * NumPitch;
    const float ColSR = NumR;

    TextMid(TEXT("BATTER"), ColName, Y, HdrH, 13, SlateMuted, false, 0);
    TextMid(TEXT("HOW OUT"), ColOut, Y, HdrH, 13, SlateMuted, false, 0);
    TextMid(TEXT("R"), ColR, Y, HdrH, 13, SlateMuted, true, 0);
    TextMid(TEXT("B"), ColB, Y, HdrH, 13, SlateMuted, true, 0);
    TextMid(TEXT("4s"), Col4s, Y, HdrH, 13, SlateMuted, true, 0);
    TextMid(TEXT("6s"), Col6s, Y, HdrH, 13, SlateMuted, true, 0);
    TextMid(TEXT("SR"), ColSR, Y, HdrH, 13, SlateMuted, true, 0);

    float RowY = Y + HdrH;
    const float RowH = 36.f;

    // Display each innings summary cleanly
    for (int Inn = 0; Inn < 2; ++Inn)
    {
        const auto& InnScore = Match->Rules.Scores[Inn];
        const int Team = Inn == 0 ? Match->FirstBattingTeam : (1 - Match->FirstBattingTeam);
        const FLinearColor TC = TeamColor(Team);

        // Innings Header Separator
        Rect(X, RowY, W, 26.f, FLinearColor(TC.R, TC.G, TC.B, .12f));
        Rect(X, RowY, 3.5f, 26.f, TC);
        const FString InnTitle = FString::Printf(TEXT("%s INNINGS  •  %d / %d  (6b)"), *Match->TeamName(Team), InnScore.Runs, InnScore.Wickets);
        TextFit(InnTitle, X + GapItem + 4.f, RowY + (26.f - 13.f) * .5f - 1.f, 13, TC, W - GapItem - 4.f - PadEdge, false, 0);
        RowY += 26.f;

        // Top 2 Batters for this Innings
        for (int B = 0; B < 2; ++B)
        {
            const bool Even = (B % 2) == 0;
            if (Even) Rect(X, RowY, W, RowH, FLinearColor(1.f, 1.f, 1.f, .015f));
            Line(X, RowY + RowH, X + W, RowY + RowH, HairlineSoft, 0.5f);

            FString BName = (B == 0) ? (Inn == 0 ? TEXT("A. RAO *") : TEXT("J. HART *")) : (Inn == 0 ? TEXT("K. DESAI") : TEXT("L. REED"));
            const int Runs = InnScore.BatterRuns[B];
            const int Balls = InnScore.BatterBalls[B];
            const FString Dismissal = (B < InnScore.Wickets) ? TEXT("b Archer") : TEXT("not out");
            const float SR = Balls > 0 ? ((float)Runs / (float)Balls * 100.f) : 0.f;
            const FString SRStr = Balls > 0 ? FString::Printf(TEXT("%.1f"), SR) : TEXT("-");

            TextFit(BName, ColName, RowY + (RowH - 16.f) * .5f, 16, WhiteAthletic, ColOut - ColName - GapItem, false, 0);
            TextFit(Dismissal, ColOut, RowY + (RowH - 14.f) * .5f, 14, SilverCool, NumL - ColOut - GapItem, false, 0);
            TextMid(FString::FromInt(Runs), ColR, RowY, RowH, 17, WhiteAthletic, true, 2);
            TextMid(FString::FromInt(Balls), ColB, RowY, RowH, 16, SilverCool, true, 2);
            TextMid(FString::FromInt(Runs >= 4 ? 1 : 0), Col4s, RowY, RowH, 16, TurfGreen, true, 2);
            TextMid(FString::FromInt(Runs >= 6 ? 1 : 0), Col6s, RowY, RowH, 16, Gold, true, 2);
            TextMid(SRStr, ColSR, RowY, RowH, 16, SilverCool, true, 2);

            RowY += RowH;
        }
    }
}

// ============================================================================
// SCREEN: MATCH RESULT & FULL BROADCAST SCORECARD
// ============================================================================
void AC26HUD::Result()
{
    Vignette();
    ScrimBottom(.92f);

    const bool Won = Match->Callout == TEXT("VICTORY");
    const bool Tie = Match->Callout == TEXT("MATCH TIED");
    const auto Edge = Tie ? Gold : (Won ? Gold : Crimson);

    // Broadcast Result & Scorecard Card. Height is driven by its own content:
    // table -> commentary line -> action row -> one PadCard of bottom padding.
    const float CardX = 320.f, CardY = 60.f, CardW = 960.f;
    const float TableH = 226.f;                       // header + 2 innings blocks
    const float SubtitleH = 38.f, BtnH = 56.f;
    const float ContentH = PadCard + LineH(56) + GapLine + LineH(22) + GapComp + 1.f
                         + GapComp + SubtitleH + GapBlock + TableH
                         + GapSect + SubtitleH + GapSect + BtnH + PadCard;
    const float CardH = ContentH;
    Panel(CardX, CardY, CardW, CardH, Edge);

    float Y = CardY + PadCard;
    TextFit(Match->Callout, 800, Y, 56, Edge, CardW - 2.f * PadCard, true, 3);
    Y += LineH(56) + GapLine;

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
        TextFit(Margin, 800, Y, 22, Gold, CardW - 2.f * PadCard, true, 0);
        Y += LineH(22) + GapComp;
    }

    Line(CardX + PadCard, Y, CardX + CardW - PadCard, Y, HairlineSoft, 1.f);
    Y += GapComp;

    // Player of the Match Spotlight Chip with a guaranteed text inset
    Rect(CardX + PadCard, Y, CardW - 2.f * PadCard, SubtitleH, SurfaceWell);
    Rect(CardX + PadCard, Y, 4, SubtitleH, Gold);
    TextMidFit(TEXT("PLAYER OF THE MATCH:  A. RAO  •  24* (6b)  [3x4  1x6]  •  SR 400.0"), 800,
               Y, SubtitleH, 16, WhiteAthletic, CardW - 2.f * PadCard - 2.f * PadEdge, true, 0);
    Y += SubtitleH + GapBlock;

    // Full Strict Column Aligned Scorecard Table
    ScorecardTable(CardX + PadCard, Y, CardW - 2.f * PadCard, TableH);

    // Centered Action Buttons, one GapBlock apart, one PadCard above the rim.
    // Subtitle() docks its commentary line into the GapSect directly above them.
    const float BtnY = CardY + CardH - PadCard - BtnH;
    SubtitleY = BtnY - GapSect - SubtitleH;
    const float BtnW = 260.f;
    const float BtnX0 = 800.f - (2.f * BtnW + GapBlock) * .5f;
    Btn(TEXT("again"), TEXT("PLAY AGAIN  >"), BtnX0, BtnY, BtnW, BtnH, 1);
    Btn(TEXT("menu"), TEXT("HOME"), BtnX0 + BtnW + GapBlock, BtnY, BtnW, BtnH, 0);
}

// ============================================================================
// PAUSE & IN-GAME SETTINGS
// ============================================================================
void AC26HUD::Preferences()
{
    Rect(0, 0, 1600, 900, FLinearColor(.004f, .008f, .014f, .88f));

    static const TCHAR* HelpA = TEXT("Watch the bowler. After release a marker shows the expected bounce. Pull anywhere right to shape the shot, release at contact.");
    static const TCHAR* HelpB = TEXT("Pull direction aims the shot, pull length sets power. Tiny pull defends, upward flick lofts. Wrong shots get punished.");
    static const TCHAR* HelpC = TEXT("Bowling: drag the pitch marker, pull the right deck for pace, start the run-up and release in the sweet spot.");

    if (Match->ControlsOpen)
    {
        // Card height is measured from the copy that actually wraps into it, so
        // the "HOW TO PLAY" panel closes one PadCard under its RESUME button.
        const float CardW = 700.f, WrapW = 620.f, BtnH = 48.f;
        const int BodyLines = WrapLines(HelpA, 20, WrapW, 1) + WrapLines(HelpB, 20, WrapW, 1) + WrapLines(HelpC, 20, WrapW, 1);
        const float BodyH = BodyLines * LineH(20);
        const float CardH = PadCard + LineH(36) + GapComp + 1.f + GapBlock + BodyH
                          + 2.f * GapComp + GapBlock + BtnH + PadCard;
        const float CardX = 800.f - CardW * .5f, CardY = 450.f - CardH * .5f;
        Panel(CardX, CardY, CardW, CardH, Gold);

        float TY = CardY + PadCard;
        TextFit(TEXT("HOW TO PLAY"), 800, TY, 36, WhiteAthletic, CardW - 2.f * PadCard, true, 0);
        TY += LineH(36) + GapComp;
        Rule(CardX + PadCard, TY, CardW - 2.f * PadCard);
        TY += GapBlock;
        TY += TextWrap(HelpA, 800, TY, 20, SilverCool, WrapW, true, 1) + GapComp;
        TY += TextWrap(HelpB, 800, TY, 20, SilverCool, WrapW, true, 1) + GapComp;
        TY += TextWrap(HelpC, 800, TY, 20, SilverCool, WrapW, true, 1) + GapBlock;

        const bool bPro = !Match->Preferences || Match->Preferences->ControlScheme == 0;
        Btn(TEXT("controls_scheme"), bPro ? TEXT("CONTROLS: GESTURE PRO") : TEXT("CONTROLS: CLASSIC"), CardX + PadCard, TY, 300, BtnH, 0);
        Btn(TEXT("lefthand"), Match->Preferences->LeftHandedUI ? TEXT("LEFT-HAND UI: ON") : TEXT("LEFT-HAND UI: OFF"), CardX + CardW - PadCard - 300, TY, 300, BtnH, 0);

        Btn(TEXT("close"), TEXT("RESUME MATCH"), 800 - 150, CardY + CardH - PadCard - 52.f, 300, 52, 1);
        return;
    }

    // Centered Modal Dialog Card, sized from its own button stack so the top and
    // bottom padding come out equal whatever the stack contains.
    const float CardW = 480.f, BtnH0 = 52.f, BtnHN = 48.f, StackCount = 5.f;
    const float StackH = BtnH0 + (StackCount - 1.f) * BtnHN + (StackCount - 1.f) * GapItem;
    const float CardH = PadCard + LineH(34) + GapComp + 1.f + GapBlock + StackH + PadCard;
    const float CardX = 800.f - CardW * .5f, CardY = 450.f - CardH * .5f;
    Panel(CardX, CardY, CardW, CardH, Gold);

    TextFit(TEXT("MATCH PAUSED"), 800, CardY + PadCard, 34, WhiteAthletic, CardW - 2.f * PadCard, true, 0);
    Rule(CardX + PadCard, CardY + PadCard + LineH(34) + GapComp, CardW - 2.f * PadCard);

    const float CX = CardX + PadCard, CW = CardW - 2.f * PadCard;
    float Y = CardY + PadCard + LineH(34) + GapComp + 1.f + GapBlock;

    Btn(TEXT("pause"), TEXT("RESUME MATCH"), CX, Y, CW, BtnH0, 1);
    Y += BtnH0 + GapItem;
    Btn(TEXT("master"), Match->Preferences->SoundVolume > .1f ? TEXT("AUDIO: ON") : TEXT("AUDIO: OFF"), CX, Y, CW, BtnHN, 0);
    Y += BtnHN + GapItem;
    Btn(TEXT("help"), TEXT("HOW TO PLAY"), CX, Y, CW, BtnHN, 0);
    Y += BtnHN + GapItem;
    Btn(TEXT("confirm_restart"), TEXT("RESTART OVER"), CX, Y, CW, BtnHN, 2);
    Y += BtnHN + GapItem;
    Btn(TEXT("confirm_exit"), TEXT("EXIT TO HOME"), CX, Y, CW, BtnHN, 0);
}

void AC26HUD::Subtitle()
{
    if (!Match || !Match->Audio || !Match->Preferences || !Match->Preferences->Subtitles || Match->Audio->ActiveSubtitle.IsEmpty()) return;

    const FString& S = Match->Audio->ActiveSubtitle;
    const float H = 38.f;
    // Cap the plate so a long commentary line can never run off the canvas, then
    // fit the text inside it with the standard inset.
    const float MaxPlateW = 1200.f;
    const float W = FMath::Min(MaxPlateW, Width(S, 20, 1) + 2.f * Sp24);
    const float X = 800.f - W * .5f, Y = SubtitleY;

    Rect(X, Y, W, H, Void);
    Line(X, Y, X + W, Y, HairlineSoft, 1.f);
    Line(X, Y + H, X + W, Y + H, HairlineSoft, 1.f);
    Line(X, Y, X, Y + H, HairlineSoft, 1.f);
    Line(X + W, Y, X + W, Y + H, HairlineSoft, 1.f);
    TextMidFit(S, 800, Y, H, 20, WhiteAthletic, W - 2.f * PadEdge, true, 1);
}

// Development-only UI Debug Bounds Visualization (Pressable or CVAR/Command-Line)
void AC26HUD::DrawUIDebug()
{
    // 1. 1600 x 900 Design Canvas Bounds (Cyan)
    Line(0, 0, 1600, 0, FLinearColor(0.f, 1.f, 1.f, .85f), 1.5f);
    Line(1600, 0, 1600, 900, FLinearColor(0.f, 1.f, 1.f, .85f), 1.5f);
    Line(1600, 900, 0, 900, FLinearColor(0.f, 1.f, 1.f, .85f), 1.5f);
    Line(0, 900, 0, 0, FLinearColor(0.f, 1.f, 1.f, .85f), 1.5f);

    // 2. Safe-Zone Inset Bounds (Green)
    Line(ContentX, TopH, ContentR, TopH, FLinearColor(0.1f, 0.9f, 0.3f, .65f), 1.f);
    Line(ContentR, TopH, ContentR, 858.f, FLinearColor(0.1f, 0.9f, 0.3f, .65f), 1.f);
    Line(ContentR, 858.f, ContentX, 858.f, FLinearColor(0.1f, 0.9f, 0.3f, .65f), 1.f);
    Line(ContentX, 858.f, ContentX, TopH, FLinearColor(0.1f, 0.9f, 0.3f, .65f), 1.f);

    // 3. Interactive Hit Zones (Magenta)
    for (const auto& Z : Zones)
    {
        const float X1 = Z.Rect.Min.X, Y1 = Z.Rect.Min.Y;
        const float X2 = Z.Rect.Max.X, Y2 = Z.Rect.Max.Y;
        Line(X1, Y1, X2, Y1, FLinearColor(1.f, 0.1f, 0.8f, .55f), 1.f);
        Line(X2, Y1, X2, Y2, FLinearColor(1.f, 0.1f, 0.8f, .55f), 1.f);
        Line(X2, Y2, X1, Y2, FLinearColor(1.f, 0.1f, 0.8f, .55f), 1.f);
        Line(X1, Y2, X1, Y1, FLinearColor(1.f, 0.1f, 0.8f, .55f), 1.f);
    }
}

void AC26HUD::DrawBounceIndicator()
{
    // Projected pitch marker. Live from the first frame of the run-up (showing the
    // bowler's intended length) all the way through to the bounce, which is the
    // only event allowed to end it. Screen-space projection of gameplay truth.
    if (!Match || !Match->PlayerBatting()) return;
    if (Match->Phase != EC26Phase::Delivery && Match->Phase != EC26Phase::RunUp) return;
    if (!Match->IsBounceIndicatorVisible()) return;
    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    const FVector World = Match->GetBounceIndicatorLocation();
    FVector2D Screen;
    if (!PC->ProjectWorldLocationToScreen(World, Screen)) return;
    const FVector2D C = ToDesign(Screen);
    if (C.X < -120.f || C.X > 1720.f || C.Y < -120.f || C.Y > 1020.f) return;

    const float Alpha = Match->GetBounceIndicatorAlpha();
    const float MScale = FMath::Clamp(Match->GestureTuning.MarkerScale, 0.4f, 2.5f);

    // Perspective-correct radius derived from a world-space reference, so the
    // marker sits ON the pitch instead of floating at a fixed pixel size.
    FVector2D EdgePx;
    float RX = 26.f;
    const float WorldR = 26.f + Match->BouncePrediction.UncertaintyRadius * 0.22f;
    if (PC->ProjectWorldLocationToScreen(World + FVector(WorldR, 0.f, 0.f), EdgePx))
        RX = FMath::Clamp((ToDesign(EdgePx) - C).Size(), 11.f, 74.f);
    // Length language: yorkers read tight, short balls read wide.
    float ShapeK = 1.f;
    if (Match->BouncePrediction.LengthCategory == EC26DeliveryLength::Yorker) ShapeK = 0.80f;
    else if (Match->BouncePrediction.LengthCategory == EC26DeliveryLength::Bouncer) ShapeK = 1.18f;
    else if (Match->BouncePrediction.LengthCategory == EC26DeliveryLength::Short) ShapeK = 1.09f;
    RX *= ShapeK * MScale;
    const float RY = RX * 0.50f;   // flattened onto the turf plane

    const bool bSpicy = Match->BouncePrediction.LengthCategory == EC26DeliveryLength::Yorker
        || Match->BouncePrediction.LengthCategory == EC26DeliveryLength::Bouncer;
    const FLinearColor Base = bSpicy ? FLinearColor(1.00f, .78f, .38f, 1.f) : FLinearColor(.72f, .96f, .78f, 1.f);

    // Dark under-shadow first: this is what keeps the marker readable against a
    // bleached day pitch as well as against night-match shadow.
    const int Segs = 44;
    auto Ellipse = [&](float ScaleK, FLinearColor Col, float Thick)
    {
        for (int I = 0; I < Segs; ++I)
        {
            const float A1 = I * 2.f * PI / Segs, A2 = (I + 1) * 2.f * PI / Segs;
            Line(C.X + RX * ScaleK * FMath::Cos(A1), C.Y + RY * ScaleK * FMath::Sin(A1),
                 C.X + RX * ScaleK * FMath::Cos(A2), C.Y + RY * ScaleK * FMath::Sin(A2), Col, Thick);
        }
    };
    Ellipse(1.06f, FLinearColor(0.f, 0.f, 0.f, .38f * Alpha), 3.2f);
    Ellipse(1.00f, FLinearColor(Base.R, Base.G, Base.B, .90f * Alpha), 2.2f);
    Ellipse(0.66f, FLinearColor(Base.R, Base.G, Base.B, .34f * Alpha), 1.2f);

    // Four short ticks on the axes: reads as a painted target, not a debug circle.
    const float Tick = RX * 0.26f;
    Line(C.X - RX - Tick, C.Y, C.X - RX + Tick * .3f, C.Y, FLinearColor(Base.R, Base.G, Base.B, .75f * Alpha), 1.8f);
    Line(C.X + RX - Tick * .3f, C.Y, C.X + RX + Tick, C.Y, FLinearColor(Base.R, Base.G, Base.B, .75f * Alpha), 1.8f);
    Line(C.X, C.Y - RY - Tick * .55f, C.X, C.Y - RY + Tick * .2f, FLinearColor(Base.R, Base.G, Base.B, .60f * Alpha), 1.6f);
    Line(C.X, C.Y + RY - Tick * .2f, C.X, C.Y + RY + Tick * .55f, FLinearColor(Base.R, Base.G, Base.B, .60f * Alpha), 1.6f);

    // Confidence halo while the marker is still showing intent rather than the
    // measured trajectory: it shrinks away as the correction completes.
    if (Match->BouncePrediction.bFromIntent && Match->BouncePrediction.UncertaintyRadius > 8.f)
    {
        const float HK = 1.55f;
        for (int I = 0; I < Segs; I += 2)
        {
            const float A1 = I * 2.f * PI / Segs, A2 = (I + 1) * 2.f * PI / Segs;
            Line(C.X + RX * HK * FMath::Cos(A1), C.Y + RY * HK * FMath::Sin(A1),
                 C.X + RX * HK * FMath::Cos(A2), C.Y + RY * HK * FMath::Sin(A2),
                 FLinearColor(Base.R, Base.G, Base.B, .22f * Alpha), 1.f);
        }
    }
}

void AC26HUD::DrawBowlingTarget()
{
    if (!Match || Match->PlayerBatting()) return;
    const bool bPro = !Match->Preferences || Match->Preferences->ControlScheme == 0;
    if (!bPro) return;
    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    if (Match->Phase == EC26Phase::Ready)
    {
        // Continuous pitch target: the player's ACTUAL chosen plan point.
        const FVector World(Match->Bowling.Line, Match->Bowling.Length, 8.f);
        FVector2D Screen;
        if (PC->ProjectWorldLocationToScreen(World, Screen))
        {
            const FVector2D C = ToDesign(Screen);
            if (C.X > -60.f && C.X < 1660.f && C.Y > -60.f && C.Y < 960.f)
            {
                FVector2D EdgePx;
                float R = 22.f;
                if (PC->ProjectWorldLocationToScreen(World + FVector(30.f, 0.f, 0.f), EdgePx))
                    R = FMath::Clamp((ToDesign(EdgePx) - C).Size(), 10.f, 60.f);
                Circle(C.X, C.Y, R, FLinearColor(Gold.R, Gold.G, Gold.B, .60f), 1.6f);
                Circle(C.X, C.Y, R * 0.45f, FLinearColor(Gold.R, Gold.G, Gold.B, .30f), 1.f);
                Line(C.X - R - 8.f, C.Y, C.X - R + 6.f, C.Y, Gold, 1.5f);
                Line(C.X + R - 6.f, C.Y, C.X + R + 8.f, C.Y, Gold, 1.5f);
                Text(Match->GetDeliveryLengthName(), C.X, C.Y - R - 26.f, 13, SilverCool, true, 0);
            }
        }
        // Live effort readout, straight off the plan the slider is writing into.
        if (Match->PacePointerId >= 0)
        {
            const float Effort01 = FMath::Clamp(Match->BowlingPlan.PaceNormalized, 0.f, 1.f);
            Text(FString::Printf(TEXT("EFFORT %d%%"), int(Effort01 * 100.f + 0.5f)),
                 Match->PaceTrackX + Match->PaceTrackW * 0.5f, Match->PaceTrackY + 22.f, 14, Gold, true, 0);
        }
    }
    else if (Match->Phase == EC26Phase::RunUp)
    {
        // The effort the ball was planned at stays readable while it is bowled.
        const float Effort01 = FMath::Clamp(Match->BowlingPlan.PaceNormalized, 0.f, 1.f);
        Text(FString::Printf(TEXT("EFFORT %d%%"), int(Effort01 * 100.f + 0.5f)), 1380.f, 668.f, 15, SilverCool, true, 0);
        if (Match->ReleaseLocked)
        {
            const FLinearColor Q = Match->BowlingExecutionQuality > 0.8f ? TurfGreen
                : (Match->BowlingExecutionQuality > 0.55f ? Gold : Crimson);
            Text(TEXT("RELEASE LOCKED"), 1380.f, 690.f, 13, Q, true, 0);
        }
    }
}

void AC26HUD::DrawBattingGestureCue()
{
    if (!Match || !Match->PlayerBatting()) return;
    const bool bPro = !Match->Preferences || Match->Preferences->ControlScheme == 0;
    if (!bPro) return;
    const auto Phase = Match->Phase;
    const bool bLive = (Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery);

    if (Match->bBattingGestureActive && bLive)
    {
        const FVector2D S = Match->BattingGestureStart;
        const FVector2D Raw = Match->BattingPullRaw;
        const float Dead = Match->GestureTuning.GestureDeadZone;
        const float MaxR = FMath::Max(20.f, Match->GestureTuning.GestureMaxRadius);
        const float Frac = Match->BattingPullFrac;
        const float Agg = Match->BattingAggression;
        const bool bArmed = Match->bGestureArmed;

        // --- origin furniture: dead zone + maximum radius, deliberately quiet ---
        Circle(S.X, S.Y, Dead, FLinearColor(1.f, 1.f, 1.f, .16f), 1.f);
        for (int I = 0; I < 48; I += 2)
        {
            const float A1 = I * 2.f * PI / 48.f, A2 = (I + 1) * 2.f * PI / 48.f;
            Line(S.X + MaxR * FMath::Cos(A1), S.Y + MaxR * FMath::Sin(A1),
                 S.X + MaxR * FMath::Cos(A2), S.Y + MaxR * FMath::Sin(A2),
                 FLinearColor(1.f, 1.f, 1.f, .09f), 1.f);
        }

        // --- the arrow: direction AND magnitude in one mark --------------------
        // Length is the clamped gameplay magnitude, not the raw finger travel, so
        // what the player sees is exactly what the shot will use.
        const FVector2D Dir = Raw.IsNearlyZero() ? FVector2D(0.f, -1.f) : Raw.GetSafeNormal();
        const float Len = FMath::Max(Dead, Frac * MaxR);
        const FVector2D Tip = S + Dir * Len;
        const FLinearColor Accent = !bArmed ? FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .70f)
            : (Agg < 0.58f ? TurfGreen : (Agg < 0.85f ? Gold : Crimson));

        if (bArmed)
        {
            // Shadowed shaft keeps the line readable over turf, crowd and sky.
            Line(S.X, S.Y, Tip.X, Tip.Y, FLinearColor(0.f, 0.f, 0.f, .45f), 6.f);
            Line(S.X, S.Y, Tip.X, Tip.Y, FLinearColor(Accent.R, Accent.G, Accent.B, .92f), 3.f);
            Line(S.X, S.Y, Tip.X, Tip.Y, FLinearColor(1.f, 1.f, 1.f, .35f), 1.f);

            // Arrow head, built from the pull direction so it always points true.
            const FVector2D Perp(-Dir.Y, Dir.X);
            const float Head = FMath::Clamp(14.f + Agg * 12.f, 14.f, 26.f);
            const FVector2D B1 = Tip - Dir * Head + Perp * Head * 0.52f;
            const FVector2D B2 = Tip - Dir * Head - Perp * Head * 0.52f;
            Line(B1.X, B1.Y, Tip.X, Tip.Y, FLinearColor(0.f, 0.f, 0.f, .45f), 5.f);
            Line(B2.X, B2.Y, Tip.X, Tip.Y, FLinearColor(0.f, 0.f, 0.f, .45f), 5.f);
            Line(B1.X, B1.Y, Tip.X, Tip.Y, Accent, 3.f);
            Line(B2.X, B2.Y, Tip.X, Tip.Y, Accent, 3.f);
            Line(B1.X, B1.Y, B2.X, B2.Y, FLinearColor(Accent.R, Accent.G, Accent.B, .55f), 2.f);
        }
        else
        {
            // Inside the dead zone: preparation only, no direction claimed.
            Circle(S.X, S.Y, Dead * 0.55f, FLinearColor(SilverCool.R, SilverCool.G, SilverCool.B, .55f), 2.f);
        }

        // --- power ring around the origin: a second read of the same number ----
        const int Segs = 40;
        const float PR = Dead + 13.f;
        const int Fill = FMath::CeilToInt(Agg * Segs);
        for (int I = 0; I < Segs; ++I)
        {
            const float A1 = -PI * 0.5f + I * 2.f * PI / Segs, A2 = -PI * 0.5f + (I + 1) * 2.f * PI / Segs;
            const bool On = I < Fill;
            Line(S.X + PR * FMath::Cos(A1), S.Y + PR * FMath::Sin(A1),
                 S.X + PR * FMath::Cos(A2), S.Y + PR * FMath::Sin(A2),
                 On ? Accent : FLinearColor(1.f, 1.f, 1.f, .10f), On ? 3.f : 1.5f);
        }

        // --- compact readout, clamped inside the safe area, never over the HUD --
        // Sized from its own three lines plus the suitability bar, so the bar
        // keeps the same PadEdge gutter as the copy above it.
        const float BoxW = 208.f;
        const float BoxH = PadEdge + LineH(21) + GapLine + LineH(14) + GapLine + LineH(13) + GapLine + 5.f + PadEdge;
        const float BX = FMath::Clamp(S.X - BoxW * .5f, 424.f, 1584.f - BoxW);
        const float BY = FMath::Clamp(S.Y - MaxR - BoxH - 20.f, 172.f, 596.f);
        Rect(BX, BY, BoxW, BoxH, FLinearColor(SurfaceWell.R, SurfaceWell.G, SurfaceWell.B, .82f));
        Line(BX, BY, BX + BoxW, BY, Accent, 1.5f);

        const float BoxTX = BX + PadEdge;
        const float BoxTW = BoxW - 2.f * PadEdge;
        const FString Zone = bArmed ? FString(C26Controls::DirectionZoneName(Match->BattingGestureAngle)) : TEXT("READY");
        const FString Band = bArmed ? FString(C26Controls::AggressionBandName(Agg)) : TEXT("PULL TO AIM");
        float BoxY = BY + PadEdge;
        TextFit(Zone, BoxTX, BoxY, 21, WhiteAthletic, BoxTW, false, 0);
        BoxY += LineH(21) + GapLine;
        TextFit(FString::Printf(TEXT("%s  •  POWER %d%%"), *Band, int(Agg * 100.f + .5f)), BoxTX, BoxY, 14, Accent, BoxTW, false, 0);
        BoxY += LineH(14) + GapLine;
        TextFit(bArmed && !Match->BattingShotCandidate.IsEmpty() ? Match->BattingShotCandidate : Match->GetBounceIndicatorText(),
                BoxTX, BoxY, 13, SlateMuted, BoxTW, false, 0);
        BoxY += LineH(13) + GapLine;
        // Shot suitability against THIS delivery: green = the right stroke.
        const FLinearColor SuitCol = Match->BattingSuitability > 0.75f ? TurfGreen
            : (Match->BattingSuitability > 0.5f ? Gold : Crimson);
        StatBar(BoxTX, BoxY, BoxTW, 5.f, Match->BattingSuitability, SuitCol);
    }
    // The idle batting affordance ring that used to pulse here has been removed;
    // the right half of the screen is already the live shot zone.

    // --- release feedback: the SAME delta the simulation was given -------------
    if (!Match->bBattingGestureActive && Match->Clock < Match->BattingFeedbackUntil
        && (Phase == EC26Phase::Delivery || Phase == EC26Phase::RunUp || Phase == EC26Phase::InPlay))
    {
        const FString Label = Match->GetReleaseTimingName();
        FLinearColor Col = SilverCool;
        switch (Match->BattingReleaseTiming)
        {
        case EC26ReleaseTiming::Perfect: Col = Gold; break;
        case EC26ReleaseTiming::Good:    Col = TurfGreen; break;
        case EC26ReleaseTiming::NoShot:  Col = SlateMuted; break;
        default:                         Col = Crimson; break;
        }
        const FString Sub = FString::Printf(TEXT("%+.0f ms  •  %s"), Match->BattingReleaseDeltaMs,
            Match->BattingShotCandidate.IsEmpty() ? TEXT("NO SHOT") : *Match->BattingShotCandidate);
        // Two stacked lines, each centred in its own leading-tall band, so the
        // big timing label can never be overrun by the sub-line beneath it.
        const float FBW = FMath::Max(Width(Label, 30, 0), Width(Sub, 14, 0)) + 2.f * PadEdge;
        const float FBH = PadEdge + LineH(30) + GapLine + LineH(14) + PadEdge;
        const float FBY = 246.f;
        Rect(800.f - FBW * .5f, FBY, FBW, FBH, SurfaceWell);
        Line(800.f - FBW * .5f, FBY, 800.f + FBW * .5f, FBY, Col, 1.5f);
        TextMid(Label, 800.f, FBY + PadEdge, LineH(30), 30, Col, true, 0);
        TextMid(Sub, 800.f, FBY + PadEdge + LineH(30) + GapLine, LineH(14), 14, SlateMuted, true, 0);
    }
}

void AC26HUD::DrawControlDebug()
{
#if !UE_BUILD_SHIPPING
    if (!Match || !Match->bDebugControls) return;
    // Development-only diagnostic overlay: every number the batting loop runs on,
    // so a bad shot can be read off the screen instead of guessed at.
    const float X = 44.f, W = 396.f;
    float Y = 150.f;
    Rect(X - Sp12, Y - Sp12, W, 720.f, FLinearColor(0.f, 0.f, 0.f, .58f));
    // Rows advance on the font's own leading, so a wrapped 13pt diagnostic can
    // never be overprinted by the row beneath it.
    auto Row = [&](const FString& S, FLinearColor C) { Text(S, X, Y, 13, C, false, 0); Y += LineH(13); };

    Row(TEXT("— BATTING INPUT DEBUG (C26Controls) —"), Gold);
    Row(FString::Printf(TEXT("STATE %s   ARMED %s   POINTER %d"), *Match->GetBattingStateName(),
        Match->bGestureArmed ? TEXT("Y") : TEXT("N"), Match->GesturePointerId), WhiteAthletic);
    Row(FString::Printf(TEXT("GESTURE START %.0f / %.0f    CURRENT %.0f / %.0f"),
        Match->BattingGestureStart.X, Match->BattingGestureStart.Y,
        Match->BattingGestureCurrent.X, Match->BattingGestureCurrent.Y), SilverCool);
    Row(FString::Printf(TEXT("PULL ANGLE %+.0f deg   AIM %+.1f deg   %s"),
        Match->BattingPullScreenAngle, Match->BattingGestureAngle,
        C26Controls::DirectionZoneName(Match->BattingGestureAngle)), WhiteAthletic);
    Row(FString::Printf(TEXT("RAW MAG %.0f du   NORM %.2f   AGGR %.2f (%s)"),
        Match->BattingPullRawMagnitude, Match->BattingPullFrac, Match->BattingAggression,
        C26Controls::AggressionBandName(Match->BattingAggression)), WhiteAthletic);
    Row(FString::Printf(TEXT("DEADZONE %.0f   MAX RADIUS %.0f   HOLD %.2fs"),
        Match->GestureTuning.GestureDeadZone, Match->GestureTuning.GestureMaxRadius,
        Match->BattingGestureHoldTime), SlateMuted);
    Row(FString::Printf(TEXT("CANDIDATE %s   SUIT %.2f   HAND %s"),
        Match->BattingShotCandidate.IsEmpty() ? TEXT("-") : *Match->BattingShotCandidate,
        Match->BattingSuitability, Match->bLeftHandedBatter ? TEXT("LEFT") : TEXT("RIGHT")), WhiteAthletic);

    Y += 4.f;
    Row(TEXT("— TIMING —"), Gold);
    Row(FString::Printf(TEXT("TO IDEAL RELEASE %+.0f ms"), Match->SecondsToIdealRelease() * 1000.f), SilverCool);
    const FLinearColor TCol = Match->BattingReleaseTiming == EC26ReleaseTiming::Perfect ? Gold
        : (Match->BattingReleaseTiming == EC26ReleaseTiming::Good ? TurfGreen : Crimson);
    Row(FString::Printf(TEXT("RELEASE DELTA %+.0f ms   %s"), Match->BattingReleaseDeltaMs,
        *Match->GetReleaseTimingName()), TCol);
    Row(FString::Printf(TEXT("SIM ERROR %+.0f ms   RESULT %s   Q %.2f"),
        Match->LastContact.TimingDeltaMs, *Match->LastContact.Shot, Match->LastContact.Quality), WhiteAthletic);
    Row(FString::Printf(TEXT("COMMITS THIS BALL %d   CANCEL '%s'"), Match->GestureCommitCount,
        Match->GestureCancelReason.IsEmpty() ? TEXT("-") : *Match->GestureCancelReason), SlateMuted);

    Y += 4.f;
    Row(TEXT("— PITCH MARKER —"), Gold);
    const auto& BP = Match->BouncePrediction;
    Row(FString::Printf(TEXT("SOURCE %s   VISIBLE %s   ALPHA %.2f"),
        BP.bFromIntent ? TEXT("IntendedTarget") : TEXT("PredictedTrajectory"),
        Match->IsBounceIndicatorVisible() ? TEXT("TRUE") : TEXT("FALSE"), BP.Alpha), WhiteAthletic);
    Row(FString::Printf(TEXT("INTENT %.0f,%.0f   SHOWN %.0f,%.0f   PRED %.0f,%.0f"),
        BP.IntendedLocation.X, BP.IntendedLocation.Y, BP.DisplayLocation.X, BP.DisplayLocation.Y,
        BP.TrueLocation.X, BP.TrueLocation.Y), SilverCool);
    Row(FString::Printf(TEXT("ACTUAL BOUNCE %.0f,%.0f   ERR %.1fcm   BOUNCED %s"),
        Match->Simulation.BouncePosition.X, Match->Simulation.BouncePosition.Y,
        FVector::Dist2D(BP.DisplayLocation, BP.TrueLocation), BP.bBounced ? TEXT("Y") : TEXT("N")), SlateMuted);
    Row(FString::Printf(TEXT("%s • %s   BOUNCE T %.2fs   BALL AGE %.2fs"),
        *Match->GetDeliveryLengthName(), *Match->GetDeliveryLineName(),
        BP.BounceTime, Match->Simulation.Ball.Age), WhiteAthletic);

    Y += 4.f;
    Row(TEXT("— BOWLING PLAN —"), Gold);
    {
        const FC26DeliveryPlan& Plan = Match->Phase == EC26Phase::Ready ? Match->Bowling : Match->LockedBowling;
        const bool bBowling = !Match->PlayerBatting();
        const EC26Movement Mov = C26Delivery::MovementOf(Match->BowlingPlan.Type);
        const float WantDir = C26Delivery::DirectionIsFree(Match->BowlingPlan.Type)
            ? Match->BowlingPlan.MovementDirection : C26Delivery::NaturalDirection(Match->BowlingPlan.Type);
        float Lo = 0.f, Hi = 0.f;
        Match->PaceRangeKph(Lo, Hi);
        const int Diff = Match->Preferences ? Match->Preferences->Difficulty : 1;

        Row(FString::Printf(TEXT("STATE %s   %s   DIFF %d"), *Match->GetBowlingStateName(),
            bBowling ? TEXT("PLAYER BOWLS") : TEXT("AI BOWLS"), Diff), WhiteAthletic);
        Row(FString::Printf(TEXT("TYPE %s   MOVEMENT %s"), *Match->GetDeliveryName(),
            Mov == EC26Movement::None ? TEXT("None") : Mov == EC26Movement::Swing ? TEXT("Swing")
            : Mov == EC26Movement::ReverseSwing ? TEXT("ReverseSwing")
            : Mov == EC26Movement::Seam ? TEXT("Seam") : TEXT("Spin")), WhiteAthletic);
        Row(FString::Printf(TEXT("DESIRED TARGET  X %+.0f  Y %.0f  (%s / %s)"),
            Match->BowlingIntendedPitch.X, Match->BowlingIntendedPitch.Y,
            *Match->GetDeliveryLineName(), *Match->GetDeliveryLengthName()), SilverCool);
        Row(FString::Printf(TEXT("ACTUAL PITCH    X %+.0f  Y %.0f   ERR %.1f cm"),
            Match->BowlingActualPitch.X, Match->BowlingActualPitch.Y,
            FVector::Dist2D(Match->BowlingIntendedPitch, Match->BowlingActualPitch)), SilverCool);
        Row(FString::Printf(TEXT("DESIRED PACE %.0f km/h   RANGE %.0f-%.0f   EFFORT %.2f"),
            Match->LastPlannedKph, Lo, Hi, Match->BowlingPlan.PaceNormalized), WhiteAthletic);
        Row(FString::Printf(TEXT("ACTUAL PACE  %.1f km/h   (%.0f cm/s)"),
            Match->LastActualKph, Plan.Speed), WhiteAthletic);
        Row(FString::Printf(TEXT("WANT DIR %+.2f (%s)   WANT AMOUNT %.2f"),
            WantDir, *Match->GetMovementText(), Match->BowlingPlan.MovementMagnitude), WhiteAthletic);
        Row(FString::Printf(TEXT("ACTUAL MOVEMENT  swing %+.1f  dev %+.1f  onset %.2f s"),
            Plan.Swing, Plan.Deviation, Plan.SwingOnset), WhiteAthletic);
        Row(FString::Printf(TEXT("RELEASE  meter %.3f  band %s  quality %.2f  NO BALL %s"),
            Match->ReleaseMeterValue < 0.f ? 0.f : Match->ReleaseMeterValue,
            *Match->GetReleaseBandName(), Match->BowlingExecutionQuality,
            Match->bBowlingNoBall ? TEXT("TRUE") : TEXT("false")), WhiteAthletic);
        Row(FString::Printf(TEXT("BAR  early %.2f  good %.2f  perfect %.2f  NOBALL %.2f"),
            Match->ActiveBar.EarlyStart, Match->ActiveBar.GoodStart,
            Match->ActiveBar.PerfectStart, Match->ActiveBar.NoBallStart), SlateMuted);
        Row(FString::Printf(TEXT("HANDS  bowler %s   batter %s   crease %s (%+.0f cm)"),
            Match->BowlerProfile.bLeftArm ? TEXT("LEFT-ARM") : TEXT("RIGHT-ARM"),
            Match->bLeftHandedBatter ? TEXT("LEFT") : TEXT("RIGHT"),
            Match->BowlingPlan.bAroundWicket ? TEXT("AROUND") : TEXT("OVER"),
            Match->CreaseOffsetCm()), SlateMuted);
        Row(FString::Printf(TEXT("DELIVERED  %s  line %.0f  length %.0f  bounce %.2f"),
            C26Delivery::Name(Plan.Type), Plan.Line, Plan.Length, Plan.Bounce), SlateMuted);
    }

    // --- world-space debug visualisation, development only -------------------
    if (APlayerController* PC = GetOwningPlayerController())
    {
        auto Mark = [&](const FVector& World, FLinearColor Col, const TCHAR* Tag)
        {
            FVector2D Px;
            if (!PC->ProjectWorldLocationToScreen(World, Px)) return;
            const FVector2D D = ToDesign(Px);
            Line(D.X - 9.f, D.Y, D.X + 9.f, D.Y, Col, 1.5f);
            Line(D.X, D.Y - 9.f, D.X, D.Y + 9.f, Col, 1.5f);
            Text(Tag, D.X + 12.f, D.Y - 8.f, 11, Col, false, 0);
        };
        if (Match->Phase == EC26Phase::RunUp || Match->Phase == EC26Phase::Delivery)
        {
            Mark(BP.IntendedLocation, FLinearColor(.45f, .75f, 1.f, .9f), TEXT("INTENT"));
            Mark(BP.TrueLocation, FLinearColor(1.f, .45f, .45f, .9f), TEXT("PREDICTED"));
            Mark(BP.DisplayLocation, FLinearColor(1.f, 1.f, 1.f, .9f), TEXT("SHOWN"));
            if (Match->Simulation.BounceEvent)
                Mark(Match->Simulation.BouncePosition, FLinearColor(.4f, 1.f, .5f, .9f), TEXT("ACTUAL"));
        }
    }
    // Raw (unclamped) gesture vector alongside the clamped gameplay vector.
    if (Match->bBattingGestureActive)
    {
        const FVector2D S = Match->BattingGestureStart, Cur = Match->BattingGestureCurrent;
        Line(S.X, S.Y, Cur.X, Cur.Y, FLinearColor(1.f, 0.f, 1.f, .45f), 1.f);
        Circle(Cur.X, Cur.Y, 5.f, FLinearColor(1.f, 0.f, 1.f, .55f), 1.f);
    }
    // Gesture zone bounds.
    const FVector2D Z0 = Match->GestureTuning.GestureZoneMin, Z1 = Match->GestureTuning.GestureZoneMax;
    const FLinearColor ZC(0.f, 1.f, 1.f, .18f);
    Line(Z0.X, Z0.Y, Z1.X, Z0.Y, ZC, 1.f); Line(Z1.X, Z0.Y, Z1.X, Z1.Y, ZC, 1.f);
    Line(Z1.X, Z1.Y, Z0.X, Z1.Y, ZC, 1.f); Line(Z0.X, Z1.Y, Z0.X, Z0.Y, ZC, 1.f);
#endif
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
    TextShadow = .80f;
    SubtitleY = 640.f;

    if (Match->Phase == EC26Phase::Menu)
    {
        Menu();
    }
    else if (Match->Phase == EC26Phase::Intro)
    {
        Vignette();
        float Y = 250.f;
        TextFit(TEXT("SUPER OVER  /  NIGHT SHOOTOUT"), 800, Y, 20, Gold, ContentW, true, 0);
        Y += LineH(20) + GapComp;
        TextFit(Match->TeamName(Match->FirstBattingTeam) + TEXT("  vs  ") + Match->TeamName(1 - Match->FirstBattingTeam),
                800, Y, 46, WhiteAthletic, ContentW, true, 0);
        Y += LineH(46) + GapBlock;
        TextFit(Match->TossText, 800, Y, 22, SlateMuted, ContentW, true, 1);

        Btn(TEXT("skip"), TEXT("SKIP INTRO  >"), 1320, SafeBot - 52.f, 200, 52, 0);
    }
    else if (Match->Phase == EC26Phase::Result)
    {
        Result();
    }
    else if (Match->Phase == EC26Phase::Interval)
    {
        Vignette();
        float Y = 240.f;
        TextFit(Match->Callout, 800, Y, 72, WhiteAthletic, ContentW, true, 3);
        Y += LineH(72) + GapComp;
        TextFit(Match->Detail, 800, Y, 24, Gold, ContentW, true, 0);
        Y += LineH(24) + GapBlock;

        TextFit(FString::Printf(TEXT("%s NEED %d RUNS FROM 6 BALLS"), *Match->TeamShort(1 - Match->FirstBattingTeam), Match->Rules.Target()),
                800, Y, 28, Gold, ContentW, true, 2);
        Y += LineH(28) + GapSect;

        Btn(TEXT("skip"), Match->PlayerBatting() ? TEXT("TAKE THE BALL  >") : TEXT("START THE CHASE  >"), 620, Y, 360, 60, 1);
    }
    else
    {
        Score();
        Controls();
        DrawBounceIndicator();
        DrawBowlingTarget();
        DrawBattingGestureCue();
        DrawControlDebug();
    }

    if (Match->Phase != EC26Phase::Menu)
    {
        Subtitle();
    }

    if (Match->Paused || Match->ControlsOpen)
    {
        Preferences();
    }

    // Development-only UI Debug Overlay (Z-Order 100)
    if (bUIDebug || FParse::Param(FCommandLine::Get(), TEXT("C26UIDebug")))
    {
        DrawUIDebug();
    }
}
