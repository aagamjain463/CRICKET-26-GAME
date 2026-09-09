#include "C26HUD.h"
#include "C26MatchGameMode.h"
#include "C26Settings.h"
#include "C26CameraDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"

namespace
{
const FLinearColor Ink(.014,.025,.045,.96f),Panel(.018,.034,.054,.93f),Paper(.88,.94,.95,1),Muted(.43,.58,.64,1),Teal(.12,.88,.79,1),Gold(1,.70,.27,1),Coral(1,.26,.17,1);
}
void AC26HUD::BeginPlay()
{
    Super::BeginPlay();
    DisplayFont=NewObject<UFont>(this);DisplayFont->FontCacheType=EFontCacheType::Runtime;DisplayFont->LegacyFontSize=32;
    DisplayFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(FTypefaceEntry(TEXT("Regular"),FPaths::ProjectContentDir()/TEXT("Cricket26/UI/Fonts/BarlowCondensed-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad));
}
void AC26HUD::Rect(float X,float Y,float W,float H,FLinearColor Color){DrawRect(Color,OffsetX+X*Scale,OffsetY+Y*Scale,W*Scale,H*Scale);}
void AC26HUD::Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Thickness){DrawLine(OffsetX+X*Scale,OffsetY+Y*Scale,OffsetX+X2*Scale,OffsetY+Y2*Scale,Color,Thickness*Scale);}
void AC26HUD::Text(const FString& S,float X,float Y,float Size,FLinearColor Color,bool Center)
{
    if(!DisplayFont)return;
    float W=0,H=0;Canvas->StrLen(DisplayFont,S,W,H);
    if(Center)X-=W*Size/32.f*.5f;
    FCanvasTextItem Item(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FText::FromString(S),DisplayFont,Color);
    Item.Scale=FVector2D(Scale*Size/32.f);Item.EnableShadow(FLinearColor(0,0,0,.55f),FVector2D(0,1));Canvas->DrawItem(Item);
}
float AC26HUD::Width(const FString& S,float Size)const
{if(!DisplayFont||!Canvas)return 0;float W=0,H=0;Canvas->StrLen(DisplayFont,S,W,H);return W*Size/32.f;}
void AC26HUD::Circle(float X,float Y,float Radius,FLinearColor Color,float Thickness)
{for(int I=0;I<64;++I){float A=I*2*PI/64,B=(I+1)*2*PI/64;Line(X+FMath::Cos(A)*Radius,Y+FMath::Sin(A)*Radius,X+FMath::Cos(B)*Radius,Y+FMath::Sin(B)*Radius,Color,Thickness);}}
void AC26HUD::Button(FName Action,const FString& Label,float X,float Y,float W,float H,bool Accent,bool Selected)
{
    Rect(X,Y,W,H,Accent?Teal:Selected?FLinearColor(.035,.19,.20,.96f):Panel);
    Line(X,Y,X+W,Y,Accent?Teal:Selected?Teal:FLinearColor(.14,.24,.29,1),1.4f);
    Text(Label,X+W*.5f,Y+(H-30)*.5f-2,30,Accent?Ink:Paper,true);
    Zones.Add({Action,FBox2D(FVector2D(X,Y),FVector2D(X+W,Y+H))});
}
FName AC26HUD::ActionAt(FVector2D Point)const
{const FVector2D P=ToDesign(Point);for(int I=Zones.Num()-1;I>=0;--I)if(Zones[I].Rect.IsInside(P))return Zones[I].Action;return NAME_None;}
FVector2D AC26HUD::ToDesign(FVector2D Point)const{return FVector2D((Point.X-OffsetX)/Scale,(Point.Y-OffsetY)/Scale);}
void AC26HUD::Logo(float X,float Y,float Size,int Team)
{
    const auto C=Team==0?Teal:Coral;
    if(Team==0)
    {
        Circle(X,Y,Size*.28f,C,3);Line(X-Size*.75f,Y-Size*.7f,X-Size*.2f,Y-Size*.1f,C,3);
        Line(X-Size*.4f,Y-Size*.85f,X+Size*.02f,Y-Size*.4f,C,2);Line(X-Size*.9f,Y-Size*.35f,X-Size*.45f,Y,C,2);
    }
    else{Line(X,Y-Size*.7f,X+Size*.42f,Y+Size*.2f,C,4);Line(X+Size*.42f,Y+Size*.2f,X,Y+Size*.55f,C,4);Line(X,Y+Size*.55f,X-Size*.42f,Y+Size*.2f,C,4);Line(X-Size*.42f,Y+Size*.2f,X,Y-Size*.7f,C,4);Line(X,Y-Size*.2f,X,Y+Size*.3f,Gold,4);}
}
void AC26HUD::Menu()
{
    // A controlled editorial scrim keeps the live stadium readable beside the title.
    for(int I=0;I<80;++I)Rect(I*14,0,15,900,FLinearColor(.008,.017,.032,.94f*(1-FMath::Pow(I/80.f,3))));
    Rect(74,64,7,22,Teal);Text(TEXT("ECLIPSE OVAL   /   NIGHT CRICKET"),98,59,24,Paper);
    // The wordmark is laid out from measured advance widths so no glyphs can collide.
    Text(TEXT("CRICKET"),74,168,104,Paper);
    const float Mark=74+Width(TEXT("CRICKET"),104)+22;
    Text(TEXT("26"),Mark,150,146,Teal);
    Text(TEXT("SUPER OVER"),78,286,62,Paper);
    const float Rule=FMath::Max(Mark+Width(TEXT("26"),146),78+Width(TEXT("SUPER OVER"),62));
    Line(80,372,Rule,372,Teal,2);
    Text(TEXT("SIX BALLS. TWO WICKETS."),80,398,34,Paper);
    Text(TEXT("ONE CHANCE TO OWN THE NIGHT."),80,439,28,Muted);
    Logo(115,553,35,Match->PlayerTeam);
    Button(TEXT("team"),Match->TeamName(Match->PlayerTeam)+TEXT("  >"),156,515,440,65);
    Text(TEXT("YOUR SIDE"),82,600,20,Muted);
    Button(TEXT("batfirst"),TEXT("BAT FIRST"),82,635,165,53,false,Match->PlayerBatsFirst&&!Match->UseToss);
    Button(TEXT("bowlfirst"),TEXT("BOWL FIRST"),257,635,175,53,false,!Match->PlayerBatsFirst&&!Match->UseToss);
    Button(TEXT("toss"),TEXT("TOSS"),442,635,154,53,false,Match->UseToss);
    Button(TEXT("play"),TEXT("PLAY SUPER OVER  >"),82,719,514,79,true);
    Button(TEXT("settings"),TEXT("SETTINGS"),625,735,165,61);Button(TEXT("help"),TEXT("HOW TO PLAY"),805,735,190,61);
    Rect(1170,630,345,118,Panel);Text(TEXT("12 BALLS"),1200,647,48,Paper);Text(TEXT("EVERY MOMENT MATTERS"),1200,705,22,Muted);
    Text(TEXT("SINGLE PLAYER  /  ORIGINAL TEAMS  /  INSTANT REMATCH"),82,843,21,Muted);
}
void AC26HUD::Score()
{
    const auto& S=Match->Rules.Now();
    Rect(54,42,652,107,Ink);Rect(54,42,6,107,Match->BattingTeam()==0?Teal:Coral);
    Logo(97,91,30,Match->BattingTeam());Text(Match->TeamShort(Match->BattingTeam()),131,57,32,Paper);
    Text(FString::Printf(TEXT("%d / %d"),S.Runs,S.Wickets),225,45,67,Paper);
    Rect(406,57,1,70,Muted);
    Text(FString::Printf(TEXT("%d"),S.LegalBalls),428,54,46,Paper);Text(TEXT("/ 6"),463,71,25,Muted);
    Text(Match->Rules.Current==0?TEXT("1ST INNINGS"):TEXT("THE CHASE"),545,71,21,Teal);
    Text(Match->BatterName()+TEXT("  *"),131,115,22,Paper);Text(Match->BowlerName(),435,115,22,Muted);
    if(Match->Rules.Current==1)
    {
        Rect(54,151,652,49,Panel);Text(FString::Printf(TEXT("NEED %d FROM %d"),Match->Rules.RunsRequired(),Match->Rules.BallsRemaining()),78,158,29,Gold);
        Text(FString::Printf(TEXT("TARGET  %d"),Match->Rules.Target()),550,160,24,Paper);
    }
    else{Rect(54,151,390,39,Panel);Text(TEXT("SUPER OVER   /   2 WICKETS"),76,155,23,Muted);}
    Rect(1240,44,294,48,Ink);Rect(1260,61,8,8,Coral);Text(TEXT("LIVE  /  ECLIPSE OVAL"),1285,52,25,Paper);
    Button(TEXT("pause"),TEXT("II"),1480,108,54,49);
    float X=565;
    Rect(530,804,540,56,Panel);
    int Start=FMath::Max(0,int(S.Ledger.size())-8);
    for(int I=Start;I<int(S.Ledger.size());++I)
    {
        const auto& O=S.Ledger[I];const bool W=O.Wicket!=C26::Dismissal::None;
        FString V=W?TEXT("W"):O.WideRuns?TEXT("Wd"):O.NoBall?TEXT("Nb"):FString::FromInt(O.BatRuns+O.Byes+O.LegByes);
        Circle(X,832,18,W?Coral:O.BatRuns>=4?Teal:Muted,1.5f);Text(V,X,815,25,W?Coral:Paper,true);X+=55;
    }
    if(S.Ledger.empty())Text(TEXT("THE OVER STARTS HERE"),800,817,25,Muted,true);
    if(S.FreeHit){Rect(725,44,165,46,Teal);Text(TEXT("FREE HIT"),807,49,29,Ink,true);}
}
void AC26HUD::Controls()
{
    const auto Phase=Match->Phase;
    if(Phase==EC26Phase::Ready||Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)
    {
        if(Match->PlayerBatting())
        {
            Circle(167,721,72,FLinearColor(.4,.65,.68,.5f),2);Circle(167+Match->Footwork*43,721,24,Teal,2);
            Text(TEXT("FOOTWORK"),167,812,24,Paper,true);
            Circle(1390,727,94,FLinearColor(.4,.65,.68,.45f),2);Line(1390,767,1390,682,Teal,3);Line(1375,699,1390,682,Teal,3);Line(1405,699,1390,682,Teal,3);
            Text(TEXT("SWIPE TO PLAY"),1390,831,25,Paper,true);
            Button(TEXT("loft"),Match->Intent.Loft?TEXT("LOFTED  ON"):TEXT("LOFTED"),1269,565,244,54,false,Match->Intent.Loft);
            Button(TEXT("defend"),TEXT("DEFEND"),78,573,178,53,false,Match->Intent.Defend);
            if(Phase==EC26Phase::Ready)Button(TEXT("ready"),TEXT("FACE DELIVERY  >"),592,676,416,73,true);
            else if(Phase==EC26Phase::Delivery)
            {
                const float T=Match->TimingCountdown();float P=FMath::Clamp(1-T/.45f,0.f,1.f);
                Rect(612,727,376,7,FLinearColor(.12,.21,.24,.9f));Rect(612,727,376*P,7,Teal);
                Text(Match->ShotQueued?TEXT("SHOT COMMITTED"):TEXT("WATCH THE BALL"),800,755,25,Paper,true);
            }
            if(Match->Preferences->Hints&&Match->Rules.Now().LegalBalls==0)
            {Rect(457,222,690,53,Panel);Text(TEXT("MOVE LEFT  /  SWIPE RIGHT TO AIM  /  TIME IT AS THE BALL ARRIVES"),802,234,23,Paper,true);}
        }
        else
        {
            static const TCHAR* Types[]={TEXT("STOCK PACE"),TEXT("YORKER"),TEXT("BOUNCER"),TEXT("SLOWER BALL"),TEXT("OUTSWINGER"),TEXT("INSWINGER")};
            Button(TEXT("delivery"),FString(Types[int(Match->Bowling.Type)])+TEXT("  >"),72,659,286,66);
            Text(TEXT("DRAG ON THE PITCH TO AIM"),78,744,25,Paper);
            const FVector P=Canvas->Project(FVector(Match->Bowling.Line,Match->Bowling.Length,8));
            if(P.Z>0){float X=(P.X-OffsetX)/Scale,Y=(P.Y-OffsetY)/Scale;Circle(X,Y,18,Teal,2);Line(X-28,Y,X+28,Y,Teal);Line(X,Y-28,X,Y+28,Teal);}
            if(Phase==EC26Phase::Ready)Button(TEXT("ready"),TEXT("START RUN-UP  >"),1160,716,350,80,true);
            if(Phase==EC26Phase::RunUp)
            {
                Rect(593,650,410,95,Panel);Text(TEXT("RELEASE IN THE GOLD ZONE"),798,662,25,Paper,true);
                Rect(623,707,350,8,Muted);Rect(900,701,41,20,Gold);Rect(623+350*Match->BowlingMeter(),695,5,32,Teal);
                Button(TEXT("release"),Match->ReleaseLocked?TEXT("RELEASE LOCKED"):TEXT("RELEASE"),1160,716,350,80,true);
            }
        }
    }
    if(Phase==EC26Phase::InPlay)
    {
        if(Match->PlayerBatting())
        {Button(TEXT("run"),Match->Running?TEXT("ANOTHER RUN"):TEXT("RUN"),1220,721,293,80,true);Button(TEXT("cancel"),TEXT("CANCEL"),79,742,195,60);}
        static const TCHAR* Timing[]={TEXT("PERFECT"),TEXT("GOOD"),TEXT("EARLY"),TEXT("LATE"),TEXT("EDGE"),TEXT("MISS")};
        Rect(658,217,284,62,Panel);Text(Timing[int(Match->LastContact.Timing)],800,222,39,Match->LastContact.Timing==EC26Timing::Perfect?Gold:Paper,true);
        if(Match->Running)Text(FString::Printf(TEXT("%d COMPLETED  /  RUNNING"),Match->CompletedRuns),800,759,28,Gold,true);
    }
    if(Phase==EC26Phase::Reaction)
    {
        const float P=FMath::Clamp(Match->PhaseTime/.18f,0.f,1.f);float Y=350+(1-P)*18;
        Rect(532,Y,536,174,Panel);Rect(532,Y,6,174,Match->Callout==TEXT("WICKET")?Coral:Teal);
        Text(Match->Callout,800,Y+9,89,Paper,true);Text(Match->Detail,800,Y+122,26,Muted,true);
    }
    if(Phase==EC26Phase::Replay)
    {
        Rect(676,215,248,53,Ink);Rect(676,215,5,53,Coral);
        Text(FString::Printf(TEXT("REPLAY  /  %.2fx"),Match->Director?Match->Director->ReplaySpeed():1.f),806,224,29,Paper,true);
        Button(TEXT("skip"),TEXT("SKIP REPLAY  >"),1240,774,275,60);
    }
}
void AC26HUD::Result()
{
    Rect(435,200,730,530,Ink);Rect(435,200,730,5,Match->Callout==TEXT("VICTORY")?Teal:Gold);
    Text(TEXT("SUPER OVER  /  FULL TIME"),800,238,25,Muted,true);Text(Match->Callout,800,282,100,Paper,true);
    Text(Match->Detail,800,409,29,Teal,true);
    for(int I=0;I<2;++I){const auto& S=Match->Rules.Scores[I];int Team=I==0?Match->FirstBattingTeam:1-Match->FirstBattingTeam;
        Text(Match->TeamName(Team),488,476+I*57,31,Paper);Text(FString::Printf(TEXT("%d / %d"),S.Runs,S.Wickets),1010,469+I*57,40,Paper);}
    Button(TEXT("again"),TEXT("PLAY AGAIN  >"),486,627,353,70,true);Button(TEXT("menu"),TEXT("MAIN MENU"),859,627,255,70);
}
void AC26HUD::Preferences()
{
    Rect(0,0,1600,900,FLinearColor(.006,.012,.022,.7f));Rect(420,148,760,610,Ink);
    Text(Match->ControlsOpen?TEXT("OWN THE MOMENT"):Match->Paused?TEXT("MATCH PAUSED"):TEXT("MATCH SETTINGS"),800,181,55,Paper,true);
    if(Match->ControlsOpen)
    {
        Text(TEXT("BATTING"),473,278,33,Teal);Text(TEXT("Drag left to move. Swipe right to direct your shot."),473,324,28,Paper);
        Text(TEXT("Time the swipe as the ball arrives. LOFTED adds aerial power."),473,366,28,Paper);
        Text(TEXT("Tap RUN to run between wickets. Tap again for another."),473,408,28,Paper);
        Text(TEXT("BOWLING"),473,474,33,Gold);Text(TEXT("Choose a delivery. Drag on the pitch. Start your run-up."),473,518,28,Paper);
        Text(TEXT("Tap RELEASE when the marker reaches the gold zone."),473,560,28,Paper);
        Text(TEXT("ON A COMPUTER: mouse drag/swipe. Space = action. R = run."),473,615,24,Muted);
    }
    else if(!Match->Paused)
    {
        const TCHAR* D[]={TEXT("EASY"),TEXT("NORMAL"),TEXT("HARD")};const TCHAR* Q[]={TEXT("LOW"),TEXT("MEDIUM"),TEXT("HIGH"),TEXT("ULTRA")};
        Button(TEXT("difficulty"),FString(TEXT("DIFFICULTY   /   "))+D[Match->Preferences->Difficulty],473,285,654,60);
        Button(TEXT("quality"),FString(TEXT("QUALITY   /   "))+Q[Match->Preferences->Quality],473,360,654,60);
        Button(TEXT("sound"),Match->Preferences->SoundVolume>.1f?TEXT("MATCH SOUND   /   ON"):TEXT("MATCH SOUND   /   OFF"),473,435,654,60);
        Button(TEXT("vibration"),Match->Preferences->Vibration?TEXT("VIBRATION   /   ON"):TEXT("VIBRATION   /   OFF"),473,510,654,60);
        Button(TEXT("sensitivity"),FString::Printf(TEXT("SWIPE SENSITIVITY   /   %.1fx"),Match->Preferences->Sensitivity),473,585,654,60);
    }
    else Text(TEXT("TAKE A BREATH. THE NEXT BALL IS YOURS."),800,390,36,Muted,true);
    Button(TEXT("close"),Match->Paused?TEXT("RESUME MATCH"):TEXT("DONE"),642,674,316,61,true);
}
void AC26HUD::DrawHUD()
{
    Super::DrawHUD();Match=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!Match||!Canvas||!Match->Preferences)return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);OffsetX=(Canvas->SizeX-1600*Scale)*.5f;OffsetY=(Canvas->SizeY-900*Scale)*.5f;Zones.Reset();
    if(Match->Phase==EC26Phase::Menu)Menu();
    else if(Match->Phase==EC26Phase::Intro)
    {
        Rect(0,0,1600,50,Ink);Rect(0,740,1600,160,Ink);
        Text(TEXT("CRICKET 26  /  SUPER OVER"),80,758,35,Teal);Text(Match->TeamName(Match->FirstBattingTeam)+TEXT("  v  ")+Match->TeamName(1-Match->FirstBattingTeam),80,804,48,Paper);
        Text(Match->TossText,80,859,23,Muted);Button(TEXT("skip"),TEXT("SKIP INTRO  >"),1290,659,245,61);
    }
    else if(Match->Phase==EC26Phase::Result)Result();
    else if(Match->Phase==EC26Phase::Interval)
    {
        Rect(468,255,664,381,Ink);Text(TEXT("INNINGS COMPLETE"),800,286,33,Muted,true);
        Text(Match->Callout,800,339,104,Paper,true);Text(Match->Detail,800,475,28,Teal,true);
        Button(TEXT("skip"),Match->PlayerBatting()?TEXT("TAKE THE BALL  >"):TEXT("START THE CHASE  >"),600,544,400,63,true);
    }
    else{Score();Controls();}
    if(Match->SettingsOpen||Match->ControlsOpen||Match->Paused)Preferences();
}
