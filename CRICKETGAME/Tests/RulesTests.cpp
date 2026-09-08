#include "../Source/CRICKETGAME/SuperOver/Core/C26Rules.h"
#include <cassert>
#include <iostream>
using namespace C26;
static uint32_t Id;
static DeliveryOutcome Ball(Match& M,int Runs=0) { DeliveryOutcome O; O.Epoch=M.Epoch; O.Id=++Id; O.BatRuns=Runs; O.CompletedRuns=Runs<4?Runs:0; return O; }
static void First(Match& M,int Runs=0) { for(int i=0;i<6;++i) assert(M.Apply(Ball(M,i==0?Runs:0))==Commit::Accepted); }
int main()
{
    int Checks=0;
    { Match M;M.Reset();First(M);assert(M.Now().Closed&&M.Now().LegalBalls==6);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.WideRuns=1;M.Apply(O);assert(M.Now().Runs==1&&M.BallsRemaining()==6);++Checks; }
    { Match M;M.Reset();auto O=Ball(M,2);O.NoBall=true;M.Apply(O);assert(M.Now().Runs==3&&M.Now().LegalBalls==0&&M.Now().FreeHit);++Checks; }
    { Match M;M.Reset();for(int i=0;i<2;++i){auto O=Ball(M);O.Wicket=Dismissal::Bowled;M.Apply(O);}assert(M.Now().Closed&&M.Now().Wickets==2);++Checks; }
    { Match M;M.Reset();First(M,4);M.StartChase();M.Apply(Ball(M,6));assert(M.Winner==Result::SecondTeam&&M.Now().LegalBalls==1);++Checks; }
    { Match M;M.Reset();First(M,6);M.StartChase();First(M,4);assert(M.Winner==Result::FirstTeam);++Checks; }
    { Match M;M.Reset();First(M,4);M.StartChase();First(M,4);assert(M.Winner==Result::Tie);++Checks; }
    { Match M;M.Reset();auto O=Ball(M,4);M.Apply(O);assert(M.Apply(O)==Commit::Duplicate&&M.Now().Runs==4);++Checks; }
    { Match M;for(int i=0;i<10;++i){M.Reset();First(M);M.StartChase();M.Apply(Ball(M,1));assert(M.Winner==Result::SecondTeam);}auto Old=Ball(M);M.Reset();assert(M.Now().Runs==0&&M.Current==0&&M.Scores[1].Ledger.empty()&&M.Apply(Old)==Commit::WrongEpoch);++Checks; }
    { Match M;M.Reset();auto O=Ball(M,1);O.NoBall=true;O.Wicket=Dismissal::RunOut;M.Apply(O);assert(M.Now().Runs==2&&M.Now().Wickets==1&&M.Now().LegalBalls==0);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.Rope=Boundary::Four;M.Apply(O);O=Ball(M);O.Rope=Boundary::Six;M.Apply(O);assert(M.Now().Runs==10);++Checks; }
    { Match M;M.Reset();M.Apply(Ball(M,1));assert(M.Now().Striker==1);M.Apply(Ball(M,2));assert(M.Now().Striker==1);++Checks; }
    { Match M;M.Reset();assert(!M.StartChase());First(M,6);assert(M.StartChase()&&!M.StartChase()&&M.Now().Runs==0&&M.Target()==7);++Checks; }
    { Match M;M.Reset();First(M,6);M.StartChase();M.Apply(Ball(M,2));assert(M.RunsRequired()==5);++Checks; }
    { Match M;M.Reset();M.Apply(Ball(M));M.Apply(Ball(M));assert(M.BallsRemaining()==4);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.NoBall=true;O.Wicket=Dismissal::Bowled;M.Apply(O);O=Ball(M);O.WideRuns=1;M.Apply(O);O=Ball(M);O.Wicket=Dismissal::Caught;M.Apply(O);assert(M.Now().Wickets==0&&!M.Now().FreeHit);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.Byes=1;O.CompletedRuns=1;M.Apply(O);O=Ball(M);O.LegByes=2;O.CompletedRuns=2;M.Apply(O);assert(M.Now().Runs==3&&M.Now().Extras==3&&M.Now().Striker==1);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.WideRuns=1;O.Rope=Boundary::Four;M.Apply(O);assert(M.Now().Runs==5&&M.Now().LegalBalls==0&&M.Now().Striker==0);++Checks; }
    { Match M;M.Reset();auto O=Ball(M);O.BatRuns=-1;assert(M.Apply(O)==Commit::Invalid&&M.Now().Ledger.empty());++Checks; }
    std::cout << "PASS: " << Checks << " rules scenarios (including 10 complete restart cycles)\n";
}
