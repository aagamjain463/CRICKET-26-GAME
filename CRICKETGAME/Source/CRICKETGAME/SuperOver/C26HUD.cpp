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

// ============ C26 DESIGN SYSTEM v2 ============
// Broadcast-noir: near-black glass over live stadium, one accent at a time,
// display condensed type, tracked micro-labels, ghost numerals, cut-corner
// CTAs. Every screen stacks from a vertical cursor with measured heights —
// nothing is placed on top of anything else.
namespace
{
const FLinearColor Ink(.014,.025,.045,.96f),Panel(.018,.034,.054,.93f),Paper(.88,.94,.95,1),Muted(.43,.58,.64,1),Teal(.12,.88,.79,1),Gold(1,.70,.27,1),Coral(1,.26,.17,1);
const FLinearColor CardBg(.016,.030,.050,.78f),Hairline(.20,.34,.40,.55f),ScrimC(.005,.012,.024,1);
const FLinearColor Glass(.020,.040,.066,.72f),GlassLine(.38,.62,.68,.42f),GhostW(1,1,1,.05f);
}
void AC26HUD::BeginPlay()
{
    Super::BeginPlay();
    DisplayFont=NewObject<UFont>(this);DisplayFont->FontCacheType=EFontCacheType::Runtime;DisplayFont->LegacyFontSize=32;
    DisplayFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(FTypefaceEntry(TEXT("Regular"),FPaths::ProjectContentDir()/TEXT("Cricket26/UI/Fonts/BarlowCondensed-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad));
}
FLinearColor AC26HUD::WithA(FLinearColor C,float M) const{return FLinearColor(C.R,C.G,C.B,C.A*FA*M);}
float AC26HUD::TH(float Size) const{return Size+12.f;}
float AC26HUD::Enter(float Delay) const
{
    if(!Match||!Match->Preferences||Match->Preferences->ReducedMotion)return 1.f;
    const float T=(Match->Clock-Match->ScreenEnteredAt-Delay)/.38f;
    if(T<=0)return 0.f;if(T>=1)return 1.f;
    return 1.f-FMath::Pow(1.f-T,3.f);
}
bool AC26HUD::Pressed(FName Action) const
{return Match&&Match->LastAction==Action&&(Match->Clock-Match->LastActionAt)<.16f;}
FLinearColor AC26HUD::TeamColor(int Team) const{return Team==0?Teal:Coral;}
FString AC26HUD::TeamTagline(int Team) const{return Team==0?TEXT("RIDE THE STORM"):TEXT("BURN BRIGHT");}
FString AC26HUD::Track(const FString& S) const
{FString T;T.Reserve(S.Len()*2);for(int I=0;I<S.Len();++I){T.AppendChar(S[I]);if(S[I]!=' ')T.AppendChar(' ');}return T;}
void AC26HUD::Rect(float X,float Y,float W,float H,FLinearColor Color){DrawRect(WithA(Color),OffsetX+X*Scale,OffsetY+Y*Scale,W*Scale,H*Scale);}
void AC26HUD::Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Thickness){DrawLine(OffsetX+X*Scale,OffsetY+Y*Scale,OffsetX+X2*Scale,OffsetY+Y2*Scale,WithA(Color),Thickness*Scale);}
void AC26HUD::Text(const FString& S,float X,float Y,float Size,FLinearColor Color,bool Center)
{
    if(!DisplayFont)return;
    float W=0,H=0;Canvas->StrLen(DisplayFont,S,W,H);
    if(Center)X-=W*Size/32.f*.5f;
    FCanvasTextItem Item(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FText::FromString(S),DisplayFont,WithA(Color));
    Item.Scale=FVector2D(Scale*Size/32.f);Item.EnableShadow(FLinearColor(0,0,0,.55f),FVector2D(0,1));Canvas->DrawItem(Item);
}
float AC26HUD::Width(const FString& S,float Size)const
{if(!DisplayFont||!Canvas)return 0;float W=0,H=0;Canvas->StrLen(DisplayFont,S,W,H);return W*Size/32.f;}
void AC26HUD::Circle(float X,float Y,float Radius,FLinearColor Color,float Thickness)
{for(int I=0;I<48;++I){float A=I*2*PI/48,B=(I+1)*2*PI/48;Line(X+FMath::Cos(A)*Radius,Y+FMath::Sin(A)*Radius,X+FMath::Cos(B)*Radius,Y+FMath::Sin(B)*Radius,Color,Thickness);}}
FName AC26HUD::ActionAt(FVector2D Point)const
{const FVector2D P=ToDesign(Point);for(int I=Zones.Num()-1;I>=0;--I)if(Zones[I].Rect.IsInside(P))return Zones[I].Action;return NAME_None;}
FVector2D AC26HUD::ToDesign(FVector2D Point)const{return FVector2D((Point.X-OffsetX)/Scale,(Point.Y-OffsetY)/Scale);}
// ---- cinematic grade: dark crown + floor, scene breathes mid-frame ----
void AC26HUD::Vignette()
{
    for(int I=0;I<26;++I)Rect(0,I*10,1600,11,FLinearColor(ScrimC.R,ScrimC.G,ScrimC.B,.62f*(1.f-I/26.f)));
    for(int I=0;I<30;++I)Rect(0,900-10*(I+1),1600,11,FLinearColor(ScrimC.R,ScrimC.G,ScrimC.B,.66f*(1.f-I/30.f)));
    for(int I=0;I<40;++I)Rect(I*8,0,9,900,FLinearColor(ScrimC.R,ScrimC.G,ScrimC.B,.34f*(1.f-I/40.f)));
}
// ---- glass card: translucent fill, hairline border, accent edge, cut tick ----
void AC26HUD::Panel(float X,float Y,float W,float H,FLinearColor Edge)
{
    Rect(X,Y,W,H,Glass);
    Line(X,Y,X+W,Y,GlassLine,1.5f);Line(X,Y+H,X+W,Y+H,GlassLine,1.5f);
    Line(X,Y,X,Y+H,GlassLine,1.5f);Line(X+W,Y,X+W,Y+H,GlassLine,1.5f);
    Rect(X,Y,4,H,Edge);
    Line(X+W-22,Y+H,X+W,Y+H-22,Edge,2.5f);
}
void AC26HUD::Rule(float X,float Y,float W){Line(X,Y,X+W,Y,Hairline,1.f);}
void AC26HUD::Ghost(const FString& S,float X,float Y,float Size){Text(S,X,Y,Size,GhostW);}
void AC26HUD::Crest(float X,float Y,float R,int Team)
{
    const auto C=TeamColor(Team);
    Circle(X,Y,R,FLinearColor(C.R,C.G,C.B,.28f),R*.16f);
    Circle(X,Y,R*.78f,C,2.4f);
    Text(Match?Match->TeamShort(Team):TEXT("C26"),X,Y-R*.30f,R*.52f,Paper,true);
    Rect(X-R*.30f,Y+R*.42f,R*.60f,3.f,C);
}
void AC26HUD::PlayGlyph(float X,float Y,float S,FLinearColor C)
{Line(X,Y-S*.6f,X,Y+S*.6f,C,3.f);Line(X,Y-S*.6f,X+S*.9f,Y,C,3.f);Line(X+S*.9f,Y,X,Y+S*.6f,C,3.f);}
void AC26HUD::Button(FName Action,const FString& Label,float X,float Y,float W,float H,bool Accent,bool Selected)
{
    Btn(Action,Label,X,Y,W,H,Accent?1:(Selected?3:0),Selected);
}
// Style: 0 secondary, 1 primary, 2 danger, 3 selected-ghost. Pressed = brief accent wash.
void AC26HUD::Btn(FName Action,const FString& Label,float X,float Y,float W,float H,int Style,bool Selected)
{
    const bool P=Pressed(Action);
    FLinearColor Bg=Style==1?Teal:(Style==2?Coral:(Style==3?FLinearColor(.035,.19,.20,.96f):Glass));
    if(P)Bg=Teal;
    Rect(X,Y,W,H,Bg);
    if(Style==1)
    {
        Rect(X,Y+H-3,W,3,FLinearColor(.6,1,.93,.9f));
        Line(X+W-30,Y+H,X+W,Y+H-30,Ink,5);
        Line(X,Y,X+W,Y,GlassLine,1.5f);
    }
    else Line(X,Y+H-1,X+W,Y+H-1,Style==3||Selected?Teal:GlassLine,Style==3||Selected?2.f:1.5f);
    const float FS=H>64?30:(H>50?26:23);
    Text(Label,X+W*.5f,Y+(H-FS)*.5f-2,FS,(Style==1||Style==2||P)?Ink:Paper,true);
    Zones.Add({Action,FBox2D(FVector2D(X,Y),FVector2D(X+W,Y+H))});
}
void AC26HUD::NavBtn(FName Action,const FString& Label,float X,float Y,float W,bool Selected)
{
    if(Selected){Rect(X-16,Y+6,4,34,Teal);Text(Track(Label),X+6,Y+6,30,Paper);}
    else Text(Track(Label),X+6,Y+6,28,Muted);
    Zones.Add({Action,FBox2D(FVector2D(X-16,Y),FVector2D(X+W,Y+50))});
}
void AC26HUD::ScrimLeft(float Strength)
{
    for(int I=0;I<64;++I)Rect(I*13,0,14,900,FLinearColor(ScrimC.R,ScrimC.G,ScrimC.B,.88f*Strength*(1-FMath::Pow(I/64.f,2.2f))));
}
void AC26HUD::ScrimBottom(float Strength)
{
    for(int I=0;I<24;++I)Rect(0,900-13*(I+1),1600,14,FLinearColor(ScrimC.R,ScrimC.G,ScrimC.B,.55f*Strength*(1.f-I/24.f)));
}
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
void AC26HUD::HeroCrest(float X,float Y,float Size,int Team){Crest(X,Y,Size,Team);}
void AC26HUD::Tag(const FString& S,float X,float Y,bool Accent)
{
    const float W=Width(S,20)+26;
    Rect(X,Y,W,32,Accent?FLinearColor(.05,.35,.30,.92f):FLinearColor(.05,.09,.13,.82f));
    if(Accent)Rect(X,Y,3,32,Teal);
    Text(S,X+13,Y+4,20,Accent?Teal:Muted);
}
void AC26HUD::TopUtility()
{
    Text(Track(TEXT("CRICKET 26")),70,30,20,Muted);
    Btn(TEXT("nav_help"),TEXT("?"),1396,22,52,44,0);
    Btn(TEXT("nav_settings"),TEXT("SET"),1456,22,76,44,0);
}
void AC26HUD::NavRail(int Selected)
{
    static const TCHAR* Items[]={TEXT("HOME"),TEXT("PLAY"),TEXT("MY TEAM"),TEXT("CAREER"),TEXT("LEAGUES"),TEXT("ONLINE"),TEXT("NETS"),TEXT("WORLD")};
    static const FName Acts[]={TEXT("nav_home"),TEXT("nav_play"),TEXT("nav_myteam"),TEXT("nav_career"),TEXT("nav_tour"),TEXT("nav_online"),TEXT("nav_train"),TEXT("nav_world")};
    static const int Map[]={0,1,5,6,7,8,9,10};
    float Y=196;
    for(int I=0;I<8;++I){NavBtn(Acts[I],Items[I],78,Y,220,Map[I]==Selected);Y+=54;}
}
void AC26HUD::Toast()
{
    if(!Match||Match->ToastText.IsEmpty()||Match->Clock>Match->ToastUntil)return;
    const float W=Width(Match->ToastText,24)+60;
    Rect(800-W/2,826,W,50,FLinearColor(.010,.020,.034,.92f));
    Rect(800-W/2,826,4,50,Teal);
    Text(Match->ToastText,800,836,24,Paper,true);
}
void AC26HUD::Confirm()
{
    if(!Match||Match->PendingConfirm.IsNone())return;
    Rect(0,0,1600,900,FLinearColor(.004,.008,.016,.62f));
    const bool Restart=Match->PendingConfirm==TEXT("restart");
    Panel(550,330,500,240,Restart?Gold:Coral);
    Text(Restart?TEXT("RESTART MATCH?"):TEXT("LEAVE MATCH?"),800,358,34,Paper,true);
    Text(Restart?TEXT("The over starts over. Scores reset."):TEXT("Progress in this over is lost."),800,414,23,Muted,true);
    Btn(TEXT("yes"),Restart?TEXT("RESTART"):TEXT("LEAVE"),590,478,210,64,Restart?1:2);
    Btn(TEXT("no"),TEXT("KEEP PLAYING"),810,478,210,64,0);
}
void AC26HUD::BackBtn(FName Action)
{
    Btn(Action,TEXT("<  BACK"),70,828,170,56,0);
}
void AC26HUD::PageHead(const FString& Kick,const FString& Title,const FString& Sub)
{
    const float E=Enter();
    Rect(330+(1-E)*26,86,6,20,Teal);
    Text(Track(Kick),348+(1-E)*26,80,22,Paper);
    Text(Title,330+(1-E)*26,112,84,Paper);
    if(!Sub.IsEmpty())Text(Sub,332+(1-E)*26,218,25,Muted);
}
void AC26HUD::Menu()
{
    FA=Match->ScreenFade;
    Vignette();ScrimLeft(.85f);
    TopUtility();
    NavRail(Match->MenuScreen);
    switch(Match->MenuScreen)
    {
    case 1:Play();break;
    case 2:Teams();break;
    case 3:Matchup();break;
    case 4:Toss();break;
    case 5:Future(0);break;
    case 6:Future(1);break;
    case 7:Future(2);break;
    case 8:Future(3);break;
    case 9:Future(4);break;
    case 10:Future(5);break;
    case 11:SettingsHub();break;
    case 12:Help();break;
    default:Home();break;
    }
    FA=1.f;Toast();
    if(!Match->PendingConfirm.IsNone())Confirm();
}
void AC26HUD::Home()
{
    const float E=Enter(),SX=(1-E)*30;
    Ghost(TEXT("06"),1050,300,340);
    // Wordmark block: kicker, display, rule — stacked, never touching.
    float Y=104;
    Rect(330+SX,Y+6,6,20,Teal);
    Text(Track(TEXT("ECLIPSE OVAL / NIGHT CRICKET")),348+SX,Y,22,Paper);Y+=TH(22)+18;
    Text(TEXT("CRICKET"),330+SX,Y,104,Paper);
    Text(TEXT("26"),330+SX+Width(TEXT("CRICKET"),104)+24,Y-16,140,Teal);Y+=TH(104)+16;
    Rule(332+SX,Y,300);Y+=26;
    // Hero card: content stacks, card fits content.
    const float HX=330+SX,HW=648,Pad=28;
    float CY=Y+Pad;
    Panel(HX,Y,HW,392,Teal);
    Tag(TEXT("PLAYABLE NOW"),HX+Pad,CY,true);CY+=32+16;
    Crest(HX+Pad+46,CY+44,46,Match->PlayerTeam);
    Text(TEXT("SUPER OVER"),HX+Pad+140,CY-6,62,Paper);
    Text(TEXT("Six balls. Two wickets."),HX+Pad+140,CY+62,25,Muted);
    Text(TEXT("One chance to own the night."),HX+Pad+140,CY+92,25,Muted);CY+=132;
    Rule(HX+Pad,CY,HW-Pad*2);CY+=14;
    Text(TEXT("1 OVER"),HX+Pad,CY,22,Teal);
    Text(TEXT("VS AI"),HX+Pad+104,CY,22,Teal);
    Text(TEXT("~5 MIN"),HX+Pad+208,CY,22,Teal);CY+=TH(22)+12;
    Text(Match->TeamName(Match->PlayerTeam)+TEXT("  v  ")+Match->TeamName(1-Match->PlayerTeam),HX+Pad,CY,22,Paper);CY+=TH(22)+18;
    Btn(TEXT("quickplay"),TEXT("PLAY NOW  >"),HX+Pad,CY,300,60,1);
    Btn(TEXT("nav_play"),TEXT("ALL MODES"),HX+Pad+316,CY,240,60,0);
    // Secondary row: two airy modules.
    CY=Y+392+20;const float CW=312;
    Panel(HX,CY,CW,160,Hairline);
    Text(TEXT("HOW TO PLAY"),HX+24,CY+16,24,Paper);
    Text(TEXT("Batting, bowling and"),HX+24,CY+50,22,Muted);
    Text(TEXT("timing in 60 seconds."),HX+24,CY+78,22,Muted);
    Btn(TEXT("nav_help"),TEXT("LEARN  >"),HX+24,CY+160-52,180,40,0);
    Panel(HX+CW+24,CY,CW,160,Hairline);
    Text(TEXT("CRICKET WORLD"),HX+CW+48,CY+16,24,Paper);
    Text(TEXT("Final-ball thriller lights"),HX+CW+48,CY+50,22,Muted);
    Text(TEXT("up Eclipse Oval."),HX+CW+48,CY+78,22,Muted);
    Btn(TEXT("nav_world"),TEXT("OPEN  >"),HX+CW+48,CY+160-52,180,40,0);
    // Right status column.
    Tag(TEXT("SINGLE PLAYER"),1130,120);
    Text(TEXT("12"),1130,168,110,Paper);
    Text(TEXT("BALLS"),1246,192,56,Paper);
    Rule(1130,300,220);Y=316;
    Text(TEXT("EVERY MOMENT MATTERS"),1130,Y,24,Muted);Y+=TH(24)+10;
    Text(TEXT("ORIGINAL TEAMS"),1130,Y,21,Muted);Y+=TH(21)+6;
    Text(TEXT("INSTANT REMATCH"),1130,Y,21,Muted);
    if(Match->MenuScreen!=0)BackBtn(TEXT("nav_home"));
}
void AC26HUD::Play()
{
    PageHead(TEXT("MODE SELECT"),TEXT("PLAY"),TEXT("One way in matters. The rest is the road ahead."));
    Ghost(TEXT("01"),1150,420,300);
    const float E=Enter(.08f),SX=(1-E)*30;
    const float X=330+SX,Y=290,W=560,H=420;
    Panel(X,Y,W,H,Teal);
    Tag(TEXT("PLAYABLE"),X+28,Y+24,true);
    Crest(X+96,Y+168,56,Match->PlayerTeam);
    Text(TEXT("SUPER OVER"),X+182,Y+108,64,Paper);
    Text(TEXT("Six balls a side. Two wickets."),X+182,Y+188,25,Muted);
    Text(TEXT("The purest shootout in cricket."),X+182,Y+220,25,Muted);
    Rule(X+28,Y+286,W-56);
    Text(TEXT("~5 MIN   /   VS AI"),X+28,Y+302,22,Teal);
    Btn(TEXT("mode_super"),TEXT("CONTINUE  >"),X+28,Y+H-92,300,64,1);
    static const TCHAR* Modes[]={TEXT("QUICK MATCH"),TEXT("WORLD CUP"),TEXT("CAREER"),TEXT("ONLINE H2H")};
    static const TCHAR* Subs[]={TEXT("Full T20 nights"),TEXT("Lift the trophy"),TEXT("Build your legacy"),TEXT("Challenge the world")};
    for(int I=0;I<4;++I)
    {
        const float CX=920+SX,CY=290+I*108,CW=400,CH=94;
        Panel(CX,CY,CW,CH,Hairline);
        Text(Modes[I],CX+24,CY+14,28,Paper);
        Text(Subs[I],CX+24,CY+52,21,Muted);
        Tag(TEXT("PREVIEW"),CX+CW-122,CY+16,false);
        Zones.Add({FName(TEXT("mode_soon")),FBox2D(FVector2D(CX,CY),FVector2D(CX+CW,CY+CH))});
    }
    BackBtn();
}
void AC26HUD::Teams()
{
    PageHead(TEXT("SUPER OVER / SETUP"),TEXT("PICK YOUR SIDE"),TEXT("Two original clubs. One night."));
    Ghost(TEXT("XI"),1150,440,300);
    const float E=Enter(.08f),SX=(1-E)*30;
    for(int T=0;T<2;++T)
    {
        const bool Mine=Match->PlayerTeam==T;
        const float X=(T==0?330:820)+SX,Y=310,W=440,H=400;
        Panel(X,Y,W,H,TeamColor(T));
        Crest(X+W/2,Y+118,64,T);
        Text(Match->TeamName(T),X+W/2,Y+208,42,Paper,true);
        Text(Match->TeamShort(T)+TEXT("  /  ")+TeamTagline(T),X+W/2,Y+262,22,TeamColor(T),true);
        Btn(T==0?TEXT("pick0"):TEXT("pick1"),Mine?TEXT("YOUR SIDE"):TEXT("SELECT  >"),X+W/2-130,Y+H-92,260,60,Mine?3:0,Mine);
    }
    Btn(TEXT("nav_matchup"),TEXT("CONTINUE  >"),820+SX,736,440,68,1);
    BackBtn();
}
void AC26HUD::Matchup()
{
    const float E=Enter();(void)E;
    ScrimBottom(.8f);
    Ghost(TEXT("VS"),700,240,260);
    float Y=170;
    Text(Track(TEXT("SUPER OVER / ECLIPSE OVAL / NIGHT")),800,Y,24,Paper,true);Y+=TH(24)+34;
    const int A=Match->PlayerTeam,B=1-Match->PlayerTeam;
    Crest(560,Y+72,72,A);Crest(1040,Y+72,72,B);
    Text(TEXT("VS"),800,Y+44,72,Teal,true);Y+=160;
    Text(Match->TeamName(A),560,Y,46,Paper,true);
    Text(Match->TeamName(B),1040,Y,46,Paper,true);Y+=TH(46)+8;
    Text(TeamTagline(A),560,Y,22,TeamColor(A),true);
    Text(TeamTagline(B),1040,Y,22,TeamColor(B),true);Y+=TH(22)+26;
    Rule(560,Y,480);Y+=24;
    Text(TEXT("SIX BALLS  /  TWO WICKETS  /  ONE WINNER"),800,Y,24,Muted,true);Y+=TH(24)+30;
    Btn(TEXT("matchup_go"),TEXT("TO THE TOSS  >"),650,Y,300,68,1);
    BackBtn();
}
void AC26HUD::Toss()
{
    PageHead(TEXT("SUPER OVER / TOSS"),TEXT("THE TOSS"),TEXT("Call it. Win it. Own the night."));
    Ghost(TEXT("50"),1100,380,320);
    const float E=Enter(.08f);(void)E;
    const float CX=800;
    float Y=340;
    if(Match->TossStage==0)
    {
        Text(TEXT("THE COIN IS READY"),CX,Y,30,Muted,true);Y+=TH(30)+24;
        Circle(CX,Y+64,64,Gold,2.5f);Circle(CX,Y+64,50,Hairline,1.5f);
        Text(TEXT("C26"),CX,Y+40,44,Gold,true);Y+=148;
        Btn(TEXT("tossflip"),TEXT("FLIP THE COIN"),CX-170,Y,340,68,1);Y+=68+16;
        Btn(TEXT("tossquick"),TEXT("SKIP  /  BAT FIRST"),CX-170,Y,340,56,0);
    }
    else
    {
        const float K=Match->TossStage==1?FMath::Abs(FMath::Sin(Match->TossClock*9.f)):1.f;
        const float RW=64*FMath::Max(.12f,K);
        for(float R=RW;R>0;R-=8)Circle(CX,Y+64,R,Gold,2.f);
        Text(TEXT("C26"),CX,Y+40,44,Gold,true);Y+=148;
        if(Match->TossStage==1){Text(TEXT("IN THE AIR..."),CX,Y,30,Paper,true);}
        else
        {
            const FString Who=Match->TossPlayerWon?TEXT("YOU WIN THE TOSS"):Match->TeamName(1-Match->PlayerTeam)+TEXT(" WIN THE TOSS");
            Text(Who,CX,Y,40,Match->TossPlayerWon?Teal:Paper,true);Y+=TH(40)+14;
            if(Match->TossPlayerWon)
            {
                Text(TEXT("WHAT WILL IT BE?"),CX,Y,24,Muted,true);Y+=TH(24)+20;
                Btn(TEXT("batfirst"),TEXT("BAT FIRST"),CX-320,Y,300,64,Match->TossPlayerChoseBat?3:0,Match->TossPlayerChoseBat);
                Btn(TEXT("bowlfirst"),TEXT("BOWL FIRST"),CX+20,Y,300,64,!Match->TossPlayerChoseBat?3:0,!Match->TossPlayerChoseBat);Y+=64+20;
                Btn(TEXT("tosscontinue"),TEXT("START MATCH  >"),CX-170,Y,340,64,1);
            }
            else
            {
                Text(Match->TossAIChoiceBat?TEXT("THEY CHOOSE TO BAT FIRST"):TEXT("THEY CHOOSE TO BOWL FIRST"),CX,Y,26,Paper,true);Y+=TH(26)+24;
                Btn(TEXT("tosscontinue"),TEXT("START MATCH  >"),CX-170,Y,340,64,1);
            }
        }
    }
    BackBtn();
}
void AC26HUD::Future(int Kind)
{
    static const TCHAR* Kicks[]={TEXT("MY TEAM"),TEXT("CAREER"),TEXT("LEAGUES"),TEXT("ONLINE"),TEXT("NETS"),TEXT("CRICKET WORLD")};
    static const TCHAR* Titles[]={TEXT("MY TEAM"),TEXT("CAREER"),TEXT("LEAGUES"),TEXT("ONLINE"),TEXT("NETS"),TEXT("WORLD")};
    static const TCHAR* Subs[]={
        TEXT("Your club. Your colours. Your call."),
        TEXT("Build your cricket legacy."),
        TEXT("Trophies worth staying up for."),
        TEXT("Challenge players around the world."),
        TEXT("Groove the skills that win overs."),
        TEXT("News, events and the wider game.")};
    static const TCHAR* Body[][3]={
        {TEXT("SQUAD"),TEXT("Two original clubs, full XIs on the way."),TEXT("KITS  /  IDENTITY  /  RATINGS")},
        {TEXT("ROAD TO GLORY"),TEXT("Create a player. Earn your place. Lead club and country."),TEXT("PLAYER  /  SEASONS  /  HONOURS")},
        {TEXT("WORLD CHAMPIONSHIP"),TEXT("T20 Premier Cup, Champions Trophy and the Invitational."),TEXT("GROUPS  /  KNOCKOUTS  /  FINAL")},
        {TEXT("HEAD TO HEAD"),TEXT("Ranked Super Overs against real opponents. Netcode pending."),TEXT("RANKED  /  FRIENDLIES  /  LADDER")},
        {TEXT("ACADEMY"),TEXT("Batting nets, bowling lab, timing drills, fielding practice."),TEXT("DRILLS  /  TARGETS  /  BESTS")},
        {TEXT("METEORS EDGE EMBERS"),TEXT("Final-ball thriller lights up Eclipse Oval. Full story soon."),TEXT("NEWS  /  EVENTS  /  RANKINGS")}};
    PageHead(Kicks[Kind],Titles[Kind],Subs[Kind]);
    Ghost(TEXT("SOON"),1050,480,240);
    const float E=Enter(.1f),SX=(1-E)*30;
    const float X=330+SX,Y=300,W=620,H=270;
    Panel(X,Y,W,H,Kind==5?Teal:Hairline);
    Tag(TEXT("COMING SOON"),X+28,Y+24,true);
    Text(Body[Kind][0],X+28,Y+72,52,Paper);
    Text(Body[Kind][1],X+28,Y+140,25,Muted);
    Text(Body[Kind][2],X+28,Y+186,21,Teal);
    if(Kind==0)
    {
        for(int T=0;T<2;++T)
        {
            const float CX=980+SX,CY2=300+T*140,CW=330,CH=120;
            Panel(CX,CY2,CW,CH,TeamColor(T));
            Crest(CX+60,CY2+60,40,T);
            Text(Match->TeamName(T),CX+118,CY2+22,28,Paper);
            Text(Match->TeamShort(T)+TEXT("  /  ")+TeamTagline(T),CX+118,CY2+60,20,TeamColor(T));
        }
    }
    else if(Kind==5)
    {
        static const TCHAR* News[]={TEXT("EMBERS NAME THEIR SUPER OVER XI"),TEXT("NIGHT FINAL SELLS OUT IN HOURS"),TEXT("ACADEMY TRIALS OPEN NEXT WEEK")};
        for(int I=0;I<3;++I)
        {
            const float CX=980+SX,CY2=300+I*100,CW=330,CH=86;
            Panel(CX,CY2,CW,CH,Hairline);
            Text(News[I],CX+20,CY2+16,22,Paper);
            Text(TEXT("CRICKET WORLD  /  PREVIEW"),CX+20,CY2+48,19,Muted);
        }
    }
    else
    {
        Panel(980+SX,300,330,270,Hairline);
        Crest(980+SX+165,300+118,64,Kind%2);
        Text(TEXT("IN DEVELOPMENT"),980+SX+165,300+212,24,Teal,true);
    }
    Btn(TEXT("mode_super"),TEXT("PLAY SUPER OVER  >"),X,Y+H+28,330,64,1);
    BackBtn();
}
void AC26HUD::SettingsHub()
{
    PageHead(TEXT("SETTINGS"),TEXT("CONTROL"),TEXT("Tuned once. Saved everywhere."));
    static const TCHAR* Tabs[]={TEXT("GAME"),TEXT("AUDIO"),TEXT("LOOK"),TEXT("TOUCH"),TEXT("EASE"),TEXT("ABOUT")};
    static const FName Acts[]={TEXT("stab0"),TEXT("stab1"),TEXT("stab2"),TEXT("stab3"),TEXT("stab4"),TEXT("stab5")};
    float TY=262;
    for(int I=0;I<6;++I)
    {
        const float TX=330+I*132;
        const bool Sel=Match->SettingsTab==I;
        if(Sel)Rect(TX,296,118,4,Teal);
        Text(Tabs[I],TX+59,266,25,Sel?Paper:Muted,true);
        Zones.Add({Acts[I],FBox2D(FVector2D(TX,256),FVector2D(TX+118,300))});
    }
    TY=326;
    const float X=330,W=760;
    auto Row=[&](FName A,const FString& L,const FString& V,int I,bool On)
    {
        const float RY=TY+I*70;
        Panel(X,RY,W,60,On?Teal:Hairline);
        Text(L,X+24,RY+14,25,Paper);
        Btn(A,V,X+W-254,RY+8,230,44,0,On);
    };
    const TCHAR* D[]={TEXT("EASY"),TEXT("NORMAL"),TEXT("HARD")};const TCHAR* Q[]={TEXT("LOW"),TEXT("MEDIUM"),TEXT("HIGH"),TEXT("ULTRA")};
    switch(Match->SettingsTab)
    {
    case 1:
        Row(TEXT("master"),TEXT("MASTER"),Match->Preferences->SoundVolume>.1f?TEXT("ON"):TEXT("OFF"),0,Match->Preferences->SoundVolume>.1f);
        Row(TEXT("commentary"),TEXT("COMMENTARY"),Match->Preferences->CommentaryVolume>.1f?TEXT("ON"):TEXT("OFF"),1,Match->Preferences->CommentaryVolume>.1f);
        Row(TEXT("crowd"),TEXT("CROWD"),Match->Preferences->CrowdVolume>.1f?TEXT("ON"):TEXT("OFF"),2,Match->Preferences->CrowdVolume>.1f);
        Row(TEXT("sfx"),TEXT("SFX + UI"),Match->Preferences->SFXVolume>.1f?TEXT("ON"):TEXT("OFF"),3,Match->Preferences->SFXVolume>.1f);
        Text(TEXT("Full mix console ships with season one. Toggles are live."),X,TY+4*70,22,Muted);
        break;
    case 2:
        Row(TEXT("quality"),FString(TEXT("GRAPHICS  /  "))+Q[Match->Preferences->Quality],Q[Match->Preferences->Quality],0,true);
        Text(TEXT("Quality applies instantly. Ultra targets capable tablets."),X,TY+70,22,Muted);
        break;
    case 3:
        Row(TEXT("sensitivity"),FString::Printf(TEXT("SWIPE  /  %.1fx"),Match->Preferences->Sensitivity),FString::Printf(TEXT("%.1fx"),Match->Preferences->Sensitivity),0,true);
        Row(TEXT("vibration"),TEXT("VIBRATION"),Match->Preferences->Vibration?TEXT("ON"):TEXT("OFF"),1,Match->Preferences->Vibration);
        Row(TEXT("hints"),TEXT("FIRST-BALL HINTS"),Match->Preferences->Hints?TEXT("ON"):TEXT("OFF"),2,Match->Preferences->Hints);
        break;
    case 4:
        Row(TEXT("subtitles"),TEXT("COMMENTARY SUBTITLES"),Match->Preferences->Subtitles?TEXT("ON"):TEXT("OFF"),0,Match->Preferences->Subtitles);
        Row(TEXT("reducedmotion"),TEXT("REDUCED MOTION"),Match->Preferences->ReducedMotion?TEXT("ON"):TEXT("OFF"),1,Match->Preferences->ReducedMotion);
        Text(TEXT("Selection is never colour-only: label plus marker, always."),X,TY+2*70,22,Muted);
        break;
    case 5:
        Panel(X,TY,W,220,Teal);
        Text(TEXT("CRICKET 26"),X+28,TY+24,52,Paper);
        Text(TEXT("Super Over vertical slice. Original teams, original ground,"),X+28,TY+100,23,Muted);
        Text(TEXT("original commentary. Mobile-first, landscape-first."),X+28,TY+130,23,Muted);
        Text(TEXT("v0.3  /  SINGLE PLAYER"),X+28,TY+168,21,Teal);
        break;
    default:
        Row(TEXT("difficulty"),FString(TEXT("DIFFICULTY  /  "))+D[Match->Preferences->Difficulty],D[Match->Preferences->Difficulty],0,true);
        Row(TEXT("hints"),TEXT("FIRST-BALL HINTS"),Match->Preferences->Hints?TEXT("ON"):TEXT("OFF"),1,Match->Preferences->Hints);
        Row(TEXT("vibration"),TEXT("VIBRATION"),Match->Preferences->Vibration?TEXT("ON"):TEXT("OFF"),2,Match->Preferences->Vibration);
        break;
    }
    BackBtn();
}
void AC26HUD::Help()
{
    PageHead(TEXT("GUIDE"),TEXT("OWN THE MOMENT"),TEXT("Three cards. Sixty seconds. Then play."));
    Ghost(TEXT("HOW"),1100,420,300);
    static const TCHAR* TT[]={TEXT("BATTING"),TEXT("BOWLING"),TEXT("SUPER OVER")};
    static const TCHAR* HD[]={TEXT("Swipe right to aim. Time it as the ball arrives."),TEXT("Pick a plan. Drag the pitch marker. Nail the gold zone."),TEXT("Six balls. Two wickets. Dots are currency.")};
    static const TCHAR* FT[]={TEXT("LOFTED = AERIAL POWER"),TEXT("YORKER + PACE = GOLD"),TEXT("BOUNDARIES WIN CHASES")};
    for(int I=0;I<3;++I)
    {
        const float X=330+I*336,Y=306,W=306,H=320;
        const float E=Enter(.08f+I*.07f);
        Panel(X,Y+(1-E)*24,W,H,I==2?Teal:Hairline);
        Circle(X+54,Y+76+(1-E)*24,26,I==2?Gold:Teal,2.5f);
        Text(FString::Printf(TEXT("%d"),I+1),X+54,Y+56+(1-E)*24,28,I==2?Gold:Teal,true);
        Text(TT[I],X+26,Y+124+(1-E)*24,34,Paper);
        Text(HD[I],X+26,Y+174+(1-E)*24,22,Muted);
        Text(FT[I],X+26,Y+262+(1-E)*24,21,Teal);
    }
    Btn(TEXT("nav_teams"),TEXT("PICK YOUR SIDE  >"),330,656,330,64,1);
    BackBtn();
}
void AC26HUD::Score()
{
    const auto& S=Match->Rules.Now();
    const int Bat=Match->BattingTeam();
    const auto TC=TeamColor(Bat);
    // Broadcast bug, glass. Internals measured — nothing touches.
    const float BX=48,BY=36,BW=620;
    Panel(BX,BY,BW,100,TC);
    Crest(BX+44,BY+50,22,Bat);
    const float SX=BX+76+Width(Match->TeamShort(Bat),30)+18;
    Text(Match->TeamShort(Bat),BX+76,BY+14,30,Paper);
    Text(FString::Printf(TEXT("%d/%d"),S.Runs,S.Wickets),SX,BY+4,64,Paper);
    const float DX=SX+Width(FString::Printf(TEXT("%d/%d"),S.Runs,S.Wickets),64)+24;
    Line(DX,BY+14,DX,BY+86,Hairline,1.f);
    Text(FString::Printf(TEXT("%d"),S.LegalBalls),DX+18,BY+16,44,Paper);
    Text(TEXT("/ 6"),DX+18+Width(FString::Printf(TEXT("%d"),S.LegalBalls),44)+8,BY+30,24,Muted);
    Text(TEXT("OVERS"),DX+18,BY+68,18,Muted);
    const FString Inn=Match->Rules.Current==0?TEXT("1ST INNINGS"):TEXT("THE CHASE");
    Text(Inn,BX+BW-24-Width(Inn,21),BY+30,21,TC);
    // Batter / bowler strip.
    Rect(BX,BY+104,BW,42,FLinearColor(.010,.020,.034,.82f));
    Text(Match->BatterName()+TEXT("  *"),BX+22,BY+109,22,Paper);
    Text(Match->BowlerName(),BX+BW-22-Width(Match->BowlerName(),22),BY+109,22,Muted);
    if(Match->Rules.Current==1)
    {
        Panel(BX,BY+150,BW,46,Gold);
        Text(FString::Printf(TEXT("NEED %d FROM %d"),Match->Rules.RunsRequired(),Match->Rules.BallsRemaining()),BX+22,BY+157,28,Gold);
        const FString Tg=FString::Printf(TEXT("TARGET %d"),Match->Rules.Target());
        Text(Tg,BX+BW-22-Width(Tg,23),BY+159,23,Paper);
    }
    Rect(1290,36,258,46,Ink);Rect(1308,53,7,7,Coral);Text(TEXT("LIVE  /  ECLIPSE OVAL"),1330,44,24,Paper);
    Btn(TEXT("pause"),TEXT("II"),1494,92,58,52,0);
    // Ball strip.
    Panel(560,812,480,52,Hairline);
    float X=592;
    int Start=FMath::Max(0,int(S.Ledger.size())-7);
    for(int I=Start;I<int(S.Ledger.size());++I)
    {
        const auto& O=S.Ledger[I];const bool W=O.Wicket!=C26::Dismissal::None;
        FString V=W?TEXT("W"):O.WideRuns?TEXT("Wd"):O.NoBall?TEXT("Nb"):FString::FromInt(O.BatRuns+O.Byes+O.LegByes);
        Circle(X,838,16,W?Coral:O.BatRuns>=4?Teal:Muted,1.5f);Text(V,X,823,23,W?Coral:Paper,true);X+=58;
    }
    if(S.Ledger.empty())Text(TEXT("THE OVER STARTS HERE"),800,821,23,Muted,true);
    if(S.FreeHit)Tag(TEXT("FREE HIT"),660,36,true);
    if(Match->Rules.BallsRemaining()==1&&Match->Rules.Now().LegalBalls>0&&Match->Phase!=EC26Phase::Replay)
    {Panel(1050,92,380,44,Gold);Text(TEXT("FINAL BALL"),1240,99,26,Gold,true);}
}
void AC26HUD::Controls()
{
    const auto Phase=Match->Phase;
    if(Phase==EC26Phase::Ready||Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)
    {
        if(Match->PlayerBatting())
        {
            Circle(150,700,66,FLinearColor(.4,.65,.68,.35f),2);Circle(150+Match->Footwork*40,700,22,Teal,2);
            Text(TEXT("FOOTWORK"),150,786,22,Paper,true);
            Circle(1420,706,88,FLinearColor(.4,.65,.68,.30f),2);Line(1420,744,1420,662,Teal,3);Line(1406,680,1420,662,Teal,3);Line(1434,680,1420,662,Teal,3);
            Text(TEXT("SWIPE TO PLAY"),1420,810,23,Paper,true);
            const float SY=560;
            Panel(1230,SY,320,56,Hairline);
            Btn(TEXT("loft"),TEXT("LOFT"),1236,SY+5,154,46,0,Match->Intent.Loft);
            Btn(TEXT("defend"),TEXT("DEFEND"),1396,SY+5,148,46,0,Match->Intent.Defend);
            if(Phase==EC26Phase::Ready)Btn(TEXT("ready"),TEXT("FACE DELIVERY  >"),1090,330,420,68,1);
            else if(Phase==EC26Phase::Delivery)
            {
                const float T=Match->TimingCountdown();float P=FMath::Clamp(1-T/.45f,0.f,1.f);
                Panel(620,748,360,52,Hairline);
                Rect(644,764,312,6,FLinearColor(.12,.21,.24,.9f));Rect(644,764,312*P,6,Teal);
                Rect(644+312*.42f,758,312*.16f,18,FLinearColor(1,.70,.27,.35f));
                Text(Match->ShotQueued?TEXT("SHOT COMMITTED"):TEXT("WATCH THE BALL"),800,786,24,Paper,true);
            }
            if(Match->Preferences->Hints&&Match->Rules.Now().LegalBalls==0&&Phase==EC26Phase::Ready)
            {Panel(480,220,640,50,Hairline);Text(TEXT("MOVE LEFT  /  SWIPE RIGHT TO AIM  /  TIME IT"),800,231,23,Paper,true);}
        }
        else
        {
            const auto& Plan=Phase==EC26Phase::Ready?Match->Bowling:Match->LockedBowling;
            if(Phase==EC26Phase::Ready)
            {
                Text(Track(TEXT("PLAN THE BALL")),72,512,22,Muted);
                Btn(TEXT("delivery"),FString(C26Delivery::Name(Plan.Type))+TEXT("  >"),72,548,300,58,0);
                Btn(TEXT("length"),FString(C26Delivery::LengthName(Plan.Length))+TEXT("  >"),72,614,300,58,0);
                const TCHAR* LineName=Plan.Line>35.f?TEXT("OUTSIDE OFF"):Plan.Line>5.f?TEXT("OFF STUMP"):Plan.Line> -25.f?TEXT("MIDDLE"):TEXT("LEG SIDE");
                Btn(TEXT("line"),FString(LineName)+TEXT("  >"),72,680,300,58,0);
                Text(TEXT("OR DRAG ON THE PITCH"),72,756,21,Muted);
            }
            else
            {
                Panel(72,664,340,76,Teal);
                Text(C26Delivery::Name(Plan.Type),94,674,28,Paper);
                Text(FString(C26Delivery::LengthName(Plan.Length))+TEXT("  /  LOCKED"),94,712,20,Teal);
            }
            const FVector P=Canvas->Project(FVector(Plan.Line,Plan.Length,8));
            if(P.Z>0){float X=(P.X-OffsetX)/Scale,Y=(P.Y-OffsetY)/Scale;Circle(X,Y,17,Teal,2);Line(X-26,Y,X+26,Y,Teal);Line(X,Y-26,X,Y+26,Teal);}
            if(Phase==EC26Phase::Ready)Btn(TEXT("ready"),TEXT("START RUN-UP  >"),1150,720,360,80,1);
            if(Phase==EC26Phase::RunUp)
            {
                Panel(610,650,380,88,Gold);
                Text(TEXT("RELEASE IN THE GOLD ZONE"),800,660,24,Paper,true);
                Rect(640,706,320,8,Muted);Rect(897,700,38,20,Gold);Rect(640+320*Match->BowlingMeter(),694,5,32,Teal);
                Btn(TEXT("release"),Match->ReleaseLocked?TEXT("LOCKED"):TEXT("RELEASE"),1150,720,360,80,1);
            }
        }
    }
    if(Phase==EC26Phase::InPlay)
    {
        if(Match->PlayerBatting())
        {Btn(TEXT("run"),Match->Running?TEXT("TWO?"):TEXT("RUN"),1230,716,300,80,1);Btn(TEXT("cancel"),TEXT("BACK"),80,730,180,60,0);}
        static const TCHAR* Timing[]={TEXT("PERFECT"),TEXT("GOOD"),TEXT("EARLY"),TEXT("LATE"),TEXT("EDGE"),TEXT("MISS")};
        if(Match->PhaseTime<1.10f)
        {
            const float K=FMath::Clamp(Match->PhaseTime/.18f,0.f,1.f);
            const bool P=Match->LastContact.Timing==EC26Timing::Perfect;
            Panel(686,222,228,50,P?Gold:Teal);
            Text(Timing[int(Match->LastContact.Timing)],800,228,30+(1-K)*8,P?Gold:Paper,true);
        }
        if(Match->Running)Text(FString::Printf(TEXT("%d COMPLETED"),Match->CompletedRuns),800,782,27,Gold,true);
    }
    if(Phase==EC26Phase::Reaction)
    {
        const float Hold=(Match->Callout==TEXT("WICKET")||Match->Callout==TEXT("SIX"))?1.35f:.95f;
        float A=1.f;
        if(Match->PhaseTime<.22f)A=Match->PhaseTime/.22f;
        else if(Match->PhaseTime>Hold)A=FMath::Max(0.f,1.f-(Match->PhaseTime-Hold)/.3f);
        const float Y=620+(1-FMath::Min(1.f,Match->PhaseTime/.22f))*30;
        const float SavedFA=FA;FA=FMath::Min(FA,A);
        const auto Edge=Match->Callout==TEXT("WICKET")?Coral:(Match->Callout==TEXT("SIX")?Gold:Teal);
        const float W=640;
        Text(Match->Callout,800,Y-18,106,FLinearColor(Edge.R,Edge.G,Edge.B,.13f),true);
        Panel(800-W/2,Y,W,108,Edge);
        Text(Match->Callout,800,Y+6,72,Paper,true);
        Text(Match->Detail,800,Y+74,24,Edge,true);
        FA=SavedFA;
    }
    if(Phase==EC26Phase::Delivery&&Match->PhaseTime<.85f)
    {
        Panel(1150,220,360,80,Teal);
        Text(FString::Printf(TEXT("%.0f KPH  /  %s"),Match->Bowling.Speed*.036f,C26Delivery::Name(Match->Bowling.Type)),1170,230,28,Paper);
        Text(Match->PlayerBatting()?C26Delivery::LengthName(Match->Bowling.Length):C26Delivery::ReleaseName(Match->ReleaseQuality),1170,268,21,Teal);
    }
    if(Phase==EC26Phase::Replay)
    {
        Panel(72,96,220,46,Coral);
        Text(TEXT("REPLAY"),122,103,26,Paper);
        Text(FString::Printf(TEXT("%.2fx"),Match->Director?Match->Director->ReplaySpeed():1.f),198,107,22,Coral);
        Btn(TEXT("skip"),TEXT("SKIP  >"),1330,770,220,60,0);
    }
}
void AC26HUD::Result()
{
    Vignette();ScrimBottom(.9f);
    Ghost(TEXT("FT"),1150,180,300);
    const bool Won=Match->Callout==TEXT("VICTORY"),Tie=Match->Callout==TEXT("MATCH TIED");
    const auto Edge=Tie?Gold:(Won?Teal:Coral);
    float Y=120;
    Text(Track(TEXT("SUPER OVER  /  FULL TIME")),800,Y,24,Paper,true);Y+=TH(24)+10;
    Text(Match->Callout,800,Y,110,Paper,true);Y+=TH(110)+6;
    Text(Match->Detail,800,Y,28,Edge,true);Y+=TH(28)+8;
    {
        const auto& A=Match->Rules.Scores[0];const auto& B=Match->Rules.Scores[1];
        FString Margin;
        if(Tie)Margin=TEXT("HONOURS EVEN");
        else
        {
            const bool ChaseWon=B.Runs>A.Runs;
            Margin=ChaseWon?FString::Printf(TEXT("WON BY %d %s"),2-B.Wickets,B.Wickets==1?TEXT("WICKET"):TEXT("WICKETS"))
                           :FString::Printf(TEXT("WON BY %d %s"),A.Runs-B.Runs,A.Runs-B.Runs==1?TEXT("RUN"):TEXT("RUNS"));
        }
        Text(Margin,800,Y,26,Gold,true);Y+=TH(26)+22;
    }
    for(int I=0;I<2;++I)
    {
        const auto& S=Match->Rules.Scores[I];int Team=I==0?Match->FirstBattingTeam:1-Match->FirstBattingTeam;
        Panel(440,Y,720,86,TeamColor(Team));
        Crest(492,Y+43,30,Team);
        Text(Match->TeamName(Team),542,Y+10,30,Paper);
        const FString Sc=FString::Printf(TEXT("%d / %d"),S.Runs,S.Wickets);
        Text(Sc,1160-24-Width(Sc,44),Y+8,44,Paper);
        int Best=0;for(int B=1;B<3;++B)if(S.BatterRuns[B]>S.BatterRuns[Best])Best=B;
        int Fours=0,Sixes=0;for(const auto& O:S.Ledger){if(O.Rope==C26::Boundary::Four)++Fours;else if(O.Rope==C26::Boundary::Six)++Sixes;}
        Text(FString::Printf(TEXT("BEST %d   /   %d x 4   /   %d x 6"),S.BatterRuns[Best],Fours,Sixes),542,Y+52,21,Muted);
        Y+=86+10;
    }
    Y+=12;
    Btn(TEXT("again"),TEXT("PLAY AGAIN  >"),500,Y,300,68,1);
    Btn(TEXT("menu"),TEXT("HOME"),820,Y,280,68,0);Y+=68+14;
    Text(TEXT("CROWD AND COMMENTARY CARRY ON BEHIND THIS CARD"),800,Y,20,Muted,true);
}
void AC26HUD::Preferences()
{
    if(Match->ControlsOpen)
    {
        Rect(0,0,1600,900,FLinearColor(.006,.012,.022,.72f));
        Panel(430,170,740,560,Teal);
        Text(TEXT("OWN THE MOMENT"),800,208,52,Paper,true);
        float Y=300;
        Text(TEXT("BATTING"),482,Y,32,Teal);Y+=TH(32)+6;
        Text(TEXT("Left stick area moves feet. Swipe right to aim and time."),482,Y,26,Paper);Y+=TH(26)+4;
        Text(TEXT("LOFTED adds aerial power. RUN banks the single."),482,Y,26,Paper);Y+=TH(26)+22;
        Text(TEXT("BOWLING"),482,Y,32,Gold);Y+=TH(32)+6;
        Text(TEXT("Pick a plan. Drag the pitch marker. Release in gold."),482,Y,26,Paper);Y+=TH(26)+4;
        Text(TEXT("Space = action. R = run. Mouse works like touch."),482,Y,23,Muted);
        Btn(TEXT("close"),TEXT("DONE"),642,646,316,60,1);
        return;
    }
    Rect(0,0,1600,900,FLinearColor(.006,.012,.022,.62f));
    Panel(480,180,640,540,Teal);
    Text(Match->Paused?TEXT("MATCH PAUSED"):TEXT("MATCH SETTINGS"),800,214,50,Paper,true);
    auto Mini=[&](FName A,const FString& L,float X,bool On)
    {
        Rect(X,292,150,76,Glass);Rect(X,292,150,2,On?Teal:Hairline);
        Text(L,X+75,300,19,Muted,true);
        Text(On?TEXT("ON"):TEXT("OFF"),X+75,322,30,On?Teal:Paper,true);
        Zones.Add({A,FBox2D(FVector2D(X,292),FVector2D(X+150,368))});
    };
    Mini(TEXT("master"),TEXT("MASTER"),490,Match->Preferences->SoundVolume>.1f);
    Mini(TEXT("commentary"),TEXT("COMM"),650,Match->Preferences->CommentaryVolume>.1f);
    Mini(TEXT("crowd"),TEXT("CROWD"),810,Match->Preferences->CrowdVolume>.1f);
    Mini(TEXT("sfx"),TEXT("SFX"),970,Match->Preferences->SFXVolume>.1f);
    Btn(TEXT("pause"),TEXT("RESUME"),540,384,520,64,1);
    Btn(TEXT("help"),TEXT("HOW TO PLAY"),540,458,520,56,0);
    Btn(TEXT("confirm_restart"),TEXT("RESTART MATCH"),540,524,255,56,2);
    Btn(TEXT("confirm_exit"),TEXT("EXIT TO HOME"),805,524,255,56,0);
    Btn(TEXT("close"),TEXT("BACK TO MATCH"),540,590,520,52,0);
    if(!Match->PendingConfirm.IsNone())Confirm();
}
void AC26HUD::Subtitle()
{
    if(!Match->Preferences->Subtitles)return;
    if(!Match->Audio||Match->Audio->ActiveSubtitle.IsEmpty())return;
    if(GetWorld()->GetTimeSeconds()>Match->Audio->SubtitleUntil)return;
    const FString& S=Match->Audio->ActiveSubtitle;
    float FS=23;float W=Width(S,FS)+44;
    if(W>1020){FS=20;W=Width(S,FS)+44;}
    const float X=800-W*.5f,Y=742;
    Rect(X,Y,W,40,FLinearColor(.010,.018,.032,.82f));
    Rect(X,Y,4,40,Teal);
    Text(S,800,Y+7,FS,Paper,true);
}
void AC26HUD::DrawHUD()
{
    Super::DrawHUD();Match=Cast<AC26MatchGameMode>(GetWorld()->GetAuthGameMode());if(!Match||!Canvas||!Match->Preferences)return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);OffsetX=(Canvas->SizeX-1600*Scale)*.5f;OffsetY=(Canvas->SizeY-900*Scale)*.5f;Zones.Reset();
    FA=1.f;
    if(Match->Phase==EC26Phase::Menu)Menu();
    else if(Match->Phase==EC26Phase::Intro)
    {
        Vignette();ScrimBottom(.85f);
        Ghost(TEXT("LIVE"),1150,300,300);
        float Y=220;
        Text(Track(TEXT("SUPER OVER  /  ECLIPSE OVAL  /  NIGHT")),800,Y,24,Teal,true);Y+=TH(24)+14;
        Text(Match->TeamName(Match->FirstBattingTeam)+TEXT("  v  ")+Match->TeamName(1-Match->FirstBattingTeam),800,Y,56,Paper,true);Y+=TH(56)+10;
        Text(Match->TossText,800,Y,24,Muted,true);Y+=TH(24)+40;
        static const TCHAR* Tips[]={TEXT("TIP  /  PERFECT TIMING MEANS CLEAN CONTACT"),TEXT("TIP  /  YORKERS AT PACE ARE GOLD AT THE DEATH"),TEXT("TIP  /  DOTS ARE CURRENCY IN A SUPER OVER")};
        Text(Tips[int(Match->Clock/2.2f)%3],800,700,24,Gold,true);
        Rect(700,742,200,4,FLinearColor(.2,.3,.36,.8f));
        Rect(700,742,200*FMath::Clamp(Match->PhaseTime/6.5f,0.f,1.f),4,Teal);
        Btn(TEXT("skip"),TEXT("SKIP  >"),1300,770,200,60,0);
    }
    else if(Match->Phase==EC26Phase::Result)Result();
    else if(Match->Phase==EC26Phase::Interval)
    {
        Vignette();ScrimBottom(.85f);
        Ghost(TEXT("16"),1050,240,320);
        float Y=210;
        Text(Track(TEXT("INNINGS COMPLETE")),800,Y,30,Muted,true);Y+=TH(30)+8;
        Text(Match->Callout,800,Y,100,Paper,true);Y+=TH(100)+6;
        Text(Match->Detail,800,Y,28,Teal,true);Y+=TH(28)+12;
        const auto& F=Match->Rules.Scores[0];
        int Best=0;for(int B=1;B<3;++B)if(F.BatterRuns[B]>F.BatterRuns[Best])Best=B;
        Text(FString::Printf(TEXT("%s  %d/%d   /   BEST %d"),*Match->TeamShort(Match->FirstBattingTeam),F.Runs,F.Wickets,F.BatterRuns[Best]),800,Y,25,Paper,true);Y+=TH(25)+8;
        Text(FString::Printf(TEXT("%s NEED %d FROM 6"),*Match->TeamShort(1-Match->FirstBattingTeam),Match->Rules.Target()),800,Y,28,Gold,true);Y+=TH(28)+28;
        Btn(TEXT("skip"),Match->PlayerBatting()?TEXT("TAKE THE BALL  >"):TEXT("START THE CHASE  >"),600,Y,400,68,1);
    }
    else{Score();Controls();}
    if(Match->Phase!=EC26Phase::Menu){Subtitle();Toast();}
    if(Match->SettingsOpen||Match->ControlsOpen||Match->Paused)Preferences();
    if(!Match->PendingConfirm.IsNone()&&!Match->SettingsOpen&&!Match->ControlsOpen&&!Match->Paused)Confirm();
}
