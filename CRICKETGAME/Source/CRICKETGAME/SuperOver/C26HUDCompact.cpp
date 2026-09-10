#include "C26HUD.h"
#include "C26MatchGameMode.h"
#include "C26Settings.h"
#include "C26Delivery.h"
#include "C26UIStyle.h"
#include "Engine/Canvas.h"
using namespace C26UIStyle;

bool AC26HUD::HandleLocalAction(FName Action)
{
    if (Action == TEXT("ui_menu")) MenuExpanded = !MenuExpanded;
    else if (Action == TEXT("ui_page")) CompactPage = 1 - CompactPage;
    else if (Action == TEXT("ui_setting_next")) { Match->SettingsTab = (Match->SettingsTab + 1) % 6; CompactPage = 0; }
    else if (Action == TEXT("ui_setting_prev")) { Match->SettingsTab = (Match->SettingsTab + 5) % 6; CompactPage = 0; }
    else return false;
    Zones.Reset();
    return true;
}

void AC26HUD::CompactHeader(const FString& Kicker, const FString& Title)
{
    Text(Kicker, 344, 40, 34, MintCyan);
    TextFit(Title, 344, 94, 76, Paper, 1200);
}

void AC26HUD::CompactFooter(FName Action, const FString& Label, FName Back)
{
    const auto& B = Layout.CompactBack;
    const auto& C = Layout.CompactContinue;
    Btn(Back, TEXT("< BACK"), B.X, B.Y, B.W, B.H);
    if (!Action.IsNone()) Btn(Action, Label, C.X, C.Y, C.W, C.H, 1);
}

void AC26HUD::CategoryMark(float X, float Y, int Kind)
{
    if (Kind == 0)
    {
        Circle(X, Y - 20, 19, MintCyan, 2);
        Line(X - 40, Y + 36, X - 32, Y + 12, PaperDim, 2);
        Line(X - 32, Y + 12, X + 32, Y + 12, PaperDim, 2);
        Line(X + 32, Y + 12, X + 40, Y + 36, PaperDim, 2);
        Line(X - 40, Y + 36, X + 40, Y + 36, PaperDim, 2);
    }
    else if (Kind == 1)
    {
        Line(X, Y - 54, X, Y - 24, MintCyan, 8);
        Rect(X - 14, Y - 24, 28, 70, PaperDim);
        Line(X - 10, Y + 46, X + 10, Y + 46, MintCyan, 3);
        Circle(X + 54, Y + 32, 14, Gold, 2);
    }
    else
    {
        Line(X - 32, Y - 36, X + 32, Y - 36, MintCyan, 2);
        Line(X - 32, Y - 36, X - 22, Y + 8, MintCyan, 2);
        Line(X + 32, Y - 36, X + 22, Y + 8, MintCyan, 2);
        Line(X - 22, Y + 8, X + 22, Y + 8, MintCyan, 2);
        Line(X, Y + 8, X, Y + 36, PaperDim, 2);
        Line(X - 28, Y + 36, X + 28, Y + 36, PaperDim, 3);
    }
}

void AC26HUD::CompactMenu()
{
    Rect(0, 0, 304, 900, Void);
    Rect(320, 16, 1264, 868, FLinearColor(Void.R, Void.G, Void.B, .84f));
    Text(TEXT("CRICKET"), 40, 44, 52, Paper);
    Text(TEXT("26"), 40, 110, 72, MintCyan);
    Btn(TEXT("nav_home"), TEXT("PLAY"), 24, 264, 256, 112, 0, Match->MenuScreen <= 4);
    Btn(TEXT("nav_myteam"), TEXT("SQUAD"), 24, 392, 256, 112, 0, Match->MenuScreen == 5);
    Btn(TEXT("nav_settings"), TEXT("SETTINGS"), 24, 520, 256, 112, 0, Match->MenuScreen == 11);
    Btn(TEXT("ui_menu"), TEXT("ALL MODES"), 24, 648, 256, 112);

    const int Screen = Match->MenuScreen;
    const int Team = Match->PlayerTeam;
    const float X = 344.f, W = 1200.f, Center = 944.f;
    if (Screen == 0)
    {
        CompactHeader(TEXT("ECLIPSE OVAL / NIGHT MATCH"), TEXT("SUPER OVER"));
        Text(TEXT("SIX BALLS."), X, 236, 116, Paper);
        Text(TEXT("ALL HEART."), X, 364, 116, MintCyan);
        TextFit(Match->TeamName(Team) + TEXT(" / ") + Match->TeamName(1 - Team), X, 528, 36, Paper, W);
        Btn(TEXT("nav_teams"), TEXT("CHANGE CLUB"), X, 616, 576, 112);
        Btn(TEXT("nav_help"), TEXT("HOW TO PLAY"), X + 600, 616, 600, 112);
        CompactFooter(TEXT("quickplay"), TEXT("PLAY SUPER OVER"), TEXT("nav_play"));
    }
    else if (Screen == 1)
    {
        CompactHeader(TEXT("CHOOSE YOUR EXPERIENCE"), TEXT("GAME MODES"));
        Text(TEXT("SUPER OVER"), X, 244, 84, Paper);
        BoundedCopy(TEXT("Six balls. Two wickets. Maximum intensity under lights."), X, 362, 38, PaperDim, 800, 2);
        Text(TEXT("SINGLE PLAYER / APPROX. 5 MIN"), X, 502, 34, MintCyan);
        BoundedCopy(TEXT("Career, world tour and online modes are in development."), X, 592, 34, Muted, W, 2);
        CompactFooter(TEXT("mode_super"), TEXT("CHOOSE YOUR CLUB"));
    }
    else if (Screen == 2 || Screen == 3)
    {
        CompactHeader(TEXT("SUPER OVER / MATCH SETUP"), Screen == 2 ? TEXT("CHOOSE YOUR CLUB") : TEXT("TONIGHT'S MATCHUP"));
        for (int I = 0; I < 2; ++I)
        {
            const float CX = X + 284 + I * 624;
            Crest(CX, 304, 58, I);
            TextFit(I == 0 ? TEXT("MUMBAI") : TEXT("MELBOURNE"), CX, 400, 68, TeamColor(I), 560, true);
            Text(I == 0 ? TEXT("METEORS") : TEXT("EMBERS"), CX, 482, 42, Paper, true);
            if (Screen == 2) Btn(I == 0 ? TEXT("pick0") : TEXT("pick1"), Team == I ? TEXT("SELECTED") : TEXT("SELECT CLUB"), CX - 280, 588, 560, 112, 0, Team == I);
            else Text(TeamTagline(I), CX, 586, 34, TeamColor(I), true);
        }
        CompactFooter(Screen == 2 ? TEXT("nav_matchup") : TEXT("matchup_go"), Screen == 2 ? TEXT("CONTINUE TO MATCHUP") : TEXT("TO THE TOSS"), Screen == 2 ? TEXT("nav_play") : TEXT("nav_teams"));
    }
    else if (Screen == 4)
    {
        CompactHeader(TEXT("DECIDE WHO SETS THE PACE"), TEXT("THE TOSS"));
        const float K = Match->TossStage == 1 && !Match->Preferences->ReducedMotion ? FMath::Max(.12f, FMath::Abs(FMath::Sin(Match->TossClock * 9.f))) : 1.f;
        Circle(Center, 296, 64 * K, Gold, 3);
        TextMid(TEXT("26"), Center, 246, 100, 52, Gold, true);
        if (Match->TossStage == 0)
        {
            Text(TEXT("YOUR CALL. YOUR GAME."), Center, 404, 52, Paper, true);
            Btn(TEXT("tossquick"), TEXT("SKIP TOSS / BAT FIRST"), X + 220, 584, 760, 112);
            CompactFooter(TEXT("tossflip"), TEXT("FLIP THE COIN"), TEXT("nav_matchup"));
        }
        else if (Match->TossStage == 1)
        {
            Text(TEXT("COIN IN THE AIR…"), Center, 436, 52, MintCyan, true);
            CompactFooter(NAME_None, TEXT(""), TEXT("nav_matchup"));
        }
        else
        {
            const bool Won = Match->TossPlayerWon;
            TextFit(Won ? TEXT("YOU WON THE TOSS") : TEXT("THE OPPOSITION WON THE TOSS"), Center, 400, 52, Paper, W, true);
            if (Won)
            {
                Btn(TEXT("batfirst"), TEXT("BAT FIRST"), X, 560, 584, 112, 0, Match->TossPlayerChoseBat);
                Btn(TEXT("bowlfirst"), TEXT("BOWL FIRST"), X + 616, 560, 584, 112, 0, !Match->TossPlayerChoseBat);
            }
            else Text(Match->TossAIChoiceBat ? TEXT("THEY ELECT TO BAT FIRST") : TEXT("THEY ELECT TO BOWL FIRST"), Center, 558, 38, Gold, true);
            CompactFooter(TEXT("tosscontinue"), TEXT("START MATCH"), TEXT("nav_matchup"));
        }
    }
    else if (Screen == 5)
    {
        CompactHeader(TEXT("SIX-PLAYER PREVIEW / NOT EDITABLE"), Match->TeamName(Team));
        static const TCHAR* Names[2][6] = {{TEXT("A. RAO"), TEXT("K. DESAI"), TEXT("R. MEHRA"), TEXT("N. ARCHER"), TEXT("S. IYER"), TEXT("D. KOHLI")}, {TEXT("J. HART"), TEXT("L. REED"), TEXT("M. VALE"), TEXT("V. SEN"), TEXT("T. FOX"), TEXT("O. BLAKE")}};
        static const TCHAR* Roles[] = {TEXT("CAPTAIN / BATTER"), TEXT("BATTER"), TEXT("ALL-ROUNDER"), TEXT("BOWLER"), TEXT("KEEPER"), TEXT("BOWLER")};
        for (int I = 0; I < 6; ++I)
        {
            const float CX = X + (I % 2) * 616, CY = 236 + (I / 2) * 160;
            Panel(CX, CY, 584, 144, I == 0 ? TeamColor(Team) : FLinearColor::Transparent);
            Text(Names[Team][I], CX + 24, CY + 12, 46, Paper);
            Text(Roles[I], CX + 24, CY + 80, 34, TeamColor(Team));
        }
        CompactFooter(TEXT("nav_teams"), TEXT("CHANGE CLUB"));
    }
    else if (Screen == 11)
    {
        CompactHeader(TEXT("MAKE THE GAME YOURS"), TEXT("CONFIGURATION"));
        static const TCHAR* Tabs[] = {TEXT("GAME"), TEXT("AUDIO"), TEXT("GRAPHICS"), TEXT("CONTROLS"), TEXT("ACCESSIBILITY"), TEXT("ABOUT")};
        Btn(TEXT("ui_setting_prev"), TEXT("<"), X, 208, 128, 112);
        TextMid(Tabs[Match->SettingsTab], Center, 208, 112, 38, Paper, true);
        Btn(TEXT("ui_setting_next"), TEXT(">"), X + W - 128, 208, 128, 112);
        SettingRows(X, 344, W, true);
        CompactFooter(NAME_None, TEXT(""));
        if (Match->SettingsTab == 1) Btn(TEXT("ui_page"), CompactPage == 0 ? TEXT("MORE AUDIO >") : TEXT("< FIRST PAGE"), 964, 752, 580, 112);
    }
    else if (Screen == 12)
    {
        CompactHeader(TEXT("TIMING IS EVERYTHING"), TEXT("HOW TO PLAY"));
        static const TCHAR* Titles[] = {TEXT("BATTING"), TEXT("BOWLING"), TEXT("SUPER OVER")};
        static const TCHAR* Descs[] = {TEXT("Move with the left stick. Swipe to aim and time your shot. Toggle loft for aerial hits."), TEXT("Choose a delivery. Drag the pitch marker to aim. Release in the gold timing window."), TEXT("Six legal balls. Two wickets. Chase their score or defend yours. Tap RUN for singles.")};
        for (int I = 0; I < 3; ++I)
        {
            const float CY = 232 + I * 160;
            Text(Titles[I], X, CY, 42, MintCyan);
            BoundedCopy(Descs[I], X + 300, CY, 34, PaperDim, W - 300, 3);
        }
        CompactFooter(TEXT("nav_teams"), TEXT("START PLAYING"));
    }
    else
    {
        const FString Title = Screen == 6 ? TEXT("ROAD TO GLORY") : Screen == 7 ? TEXT("LEADERBOARDS") : Screen == 8 ? TEXT("MULTIPLAYER") : Screen == 9 ? TEXT("TRAINING NETS") : Screen == 10 ? TEXT("CRICKET WORLD") : TEXT("CRICKET STORE");
        CompactHeader(TEXT("THE NEXT CHAPTER / IN DEVELOPMENT"), Title);
        Text(TEXT("MORE CRICKET."), X, 262, 88, Paper);
        Text(TEXT("ON THE HORIZON."), X, 374, 88, MintCyan);
        BoundedCopy(Screen == 13 ? TEXT("Player packs, kits and cosmetics are previews only. No purchases are available.") : TEXT("This experience is not available in this build. Play Super Over while the next chapter takes shape."), X, 520, 38, PaperDim, W, 3);
        CompactFooter(TEXT("mode_super"), TEXT("PLAY SUPER OVER"));
    }

    if (MenuExpanded)
    {
        Zones.Reset();
        Rect(0, 0, 1600, 900, Void);
        Text(TEXT("YOUR CRICKET. YOUR WAY."), 56, 36, 58, Paper);
        static const TCHAR* Labels[] = {TEXT("PLAY NOW"), TEXT("SQUAD HUB"), TEXT("CAREER"), TEXT("MULTIPLAYER"), TEXT("LEADERBOARDS"), TEXT("STORE"), TEXT("TRAINING"), TEXT("CRICKET WORLD"), TEXT("SETTINGS"), TEXT("HOW TO PLAY")};
        static const FName Actions[] = {TEXT("nav_home"), TEXT("nav_myteam"), TEXT("nav_career"), TEXT("nav_online"), TEXT("nav_tour"), TEXT("nav_store"), TEXT("nav_train"), TEXT("nav_world"), TEXT("nav_settings"), TEXT("nav_help")};
        for (int I = 0; I < 10; ++I) Btn(Actions[I], Labels[I], 56 + (I % 2) * 752, 200 + (I / 2) * 132, 720, 112);
        Btn(TEXT("ui_menu"), TEXT("CLOSE"), 1280, 40, 264, 112);
    }
}

void AC26HUD::SettingRows(float X, float Y, float W, bool Compact)
{
    const float Pitch = Compact ? 120.f : 76.f, H = Compact ? 112.f : 56.f;
    const float Size = Compact ? 36.f : 22.f, ControlW = Compact ? 312.f : 220.f;
    auto Row = [&](FName A, const FString& Label, const FString& Value, int Index, bool On)
    {
        if (Compact && Index / 3 != CompactPage) return;
        const float RY = Y + (Compact ? Index % 3 : Index) * Pitch;
        Rule(X, RY, W);
        TextFit(Label, X, RY + (H - LineH(Size)) * .5f, Size, Paper, W - ControlW - 32);
        Btn(A, Value, X + W - ControlW, RY, ControlW, H, 0, On);
    };
    const auto* P = Match->Preferences;
    auto Toggle = [&](FName A, const TCHAR* L, bool On, int I) { Row(A, L, On ? TEXT("ON") : TEXT("OFF"), I, On); };
    static const TCHAR* D[] = {TEXT("EASY"), TEXT("NORMAL"), TEXT("HARD")};
    static const TCHAR* Q[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA")};
    switch (Match->SettingsTab)
    {
    case 1:
        Toggle(TEXT("master"), TEXT("MASTER AUDIO"), P->SoundVolume > .1f, 0);
        Toggle(TEXT("commentary"), TEXT("COMMENTARY"), P->CommentaryVolume > .1f, 1);
        Toggle(TEXT("crowd"), TEXT("STADIUM CROWD"), P->CrowdVolume > .1f, 2);
        Toggle(TEXT("sfx"), TEXT("SFX & IMPACTS"), P->SFXVolume > .1f, 3);
        break;
    case 2: Row(TEXT("quality"), TEXT("GRAPHICS QUALITY"), Q[FMath::Clamp(P->Quality, 0, 3)], 0, true); break;
    case 3:
        Row(TEXT("sensitivity"), TEXT("SWIPE SENSITIVITY"), FString::Printf(TEXT("%.1fx"), P->Sensitivity), 0, true);
        Toggle(TEXT("vibration"), TEXT("HAPTIC FEEDBACK"), P->Vibration, 1);
        Toggle(TEXT("hints"), TEXT("TACTICAL HINTS"), P->Hints, 2);
        break;
    case 4:
        Toggle(TEXT("subtitles"), TEXT("SUBTITLES"), P->Subtitles, 0);
        Toggle(TEXT("reducedmotion"), TEXT("REDUCED MOTION"), P->ReducedMotion, 1);
        break;
    case 5:
        Text(TEXT("CRICKET 26 / SUPER OVER"), X, Y, Compact ? 52 : 44, Paper);
        BoundedCopy(TEXT("Original clubs. Floodlit cricket. Precision ball physics. A single-player Super Over vertical slice built in Unreal Engine."), X, Y + 88, Size, PaperDim, W, 4);
        break;
    default:
        Row(TEXT("difficulty"), TEXT("AI DIFFICULTY"), D[FMath::Clamp(P->Difficulty, 0, 2)], 0, true);
        Toggle(TEXT("hints"), TEXT("TACTICAL HINTS"), P->Hints, 1);
        Toggle(TEXT("vibration"), TEXT("HAPTIC FEEDBACK"), P->Vibration, 2);
        break;
    }
}

void AC26HUD::CompactControls()
{
    const auto Phase = Match->Phase;
    const auto& Primary = Layout.Primary;
    auto Main = [&](FName A, const FString& Label) { Btn(A, Label, Primary.X, Primary.Y, Primary.W, Primary.H, 1); };
    if (Phase == EC26Phase::Ready || Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery)
    {
        if (Match->PlayerBatting())
        {
            const auto C = Layout.FootCenter;
            Circle(C.X, C.Y, Layout.FootRadius, PaperDim, 2);
            Circle(C.X + Match->Footwork * Layout.FootRadius, C.Y - Match->Intent.Stride * Layout.FootRadius, 24, MintCyan, 3);
            Text(TEXT("FOOTWORK"), C.X, 832, 34, PaperDim, true);
            Btn(TEXT("loft"), TEXT("LOFT"), 1176, 464, 360, 112, 0, Match->Intent.Loft);
            Btn(TEXT("defend"), TEXT("DEFEND"), 1176, 600, 360, 112, 0, Match->Intent.Defend);
            if (Phase == EC26Phase::Ready) Main(TEXT("ready"), TEXT("READY"));
            else Text(TEXT("SWIPE TO HIT"), 1356, 772, 34, Paper, true);
        }
        else
        {
            const auto& Plan = Phase == EC26Phase::Ready ? Match->Bowling : Match->LockedBowling;
            if (Phase == EC26Phase::Ready)
            {
                Btn(TEXT("delivery"), C26Delivery::Name(Plan.Type), 64, 464, 336, 112);
                Btn(TEXT("length"), C26Delivery::LengthName(Plan.Length), 64, 600, 336, 112);
                const TCHAR* L = Plan.Line > 35 ? TEXT("OUTSIDE OFF") : Plan.Line > 5 ? TEXT("OFF STUMP") : Plan.Line > -25 ? TEXT("MIDDLE") : TEXT("LEG SIDE");
                Btn(TEXT("line"), L, 64, 736, 336, 112);
                Main(TEXT("ready"), TEXT("START RUN-UP"));
            }
            const FVector P = Canvas->Project(FVector(Plan.Line, Plan.Length, 8));
            if (P.Z > 0) Circle((P.X - OffsetX) / Scale, (P.Y - OffsetY) / Scale, 18, MintCyan, 3);
            if (Phase == EC26Phase::RunUp)
            {
                Rect(560, 642, 480, 12, Muted);
                Rect(928, 634, 48, 28, Gold);
                Rect(560 + 480 * Match->BowlingMeter(), 626, 6, 44, MintCyan);
                Main(TEXT("release"), Match->ReleaseLocked ? TEXT("LOCKED") : TEXT("RELEASE"));
            }
        }
    }
    else if (Phase == EC26Phase::InPlay)
    {
        if (Match->PlayerBatting())
        {
            Main(TEXT("run"), Match->Running ? TEXT("RUN TWO?") : TEXT("RUN"));
            Btn(TEXT("cancel"), TEXT("BACK"), 64, 736, 336, 112);
        }
        static const TCHAR* Timing[] = {TEXT("PERFECT"), TEXT("GOOD"), TEXT("EARLY"), TEXT("LATE"), TEXT("EDGE"), TEXT("MISS")};
        if (Match->PhaseTime < .95f) Text(Timing[FMath::Clamp(int(Match->LastContact.Timing), 0, 5)], 800, 280, 52, Gold, true);
        if (Match->Running) Text(FString::Printf(TEXT("%d COMPLETED"), Match->CompletedRuns), 800, 626, 38, Gold, true);
    }
    else if (Phase == EC26Phase::Reaction)
    {
        TextFit(Match->Callout, 800, 380, 104, Match->Callout == TEXT("WICKET") ? Coral : MintCyan, 1200, true);
        BoundedCopy(Match->Detail, 800, 526, 38, Paper, 1120, 2, true);
    }
    else if (Phase == EC26Phase::Replay)
    {
        Text(TEXT("REPLAY"), 1260, 224, 38, Coral);
        Main(TEXT("skip"), TEXT("SKIP REPLAY"));
    }
}
