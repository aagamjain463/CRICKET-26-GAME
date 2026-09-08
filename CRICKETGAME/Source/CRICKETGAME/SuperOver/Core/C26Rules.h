#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>

// Engine-independent authoritative rules. Presentation may only read this model.
namespace C26
{
enum class Dismissal : uint8_t { None, Bowled, Caught, RunOut };
enum class Boundary : uint8_t { None, Four, Six };
enum class Result : uint8_t { Playing, FirstTeam, SecondTeam, Tie };
enum class Commit : uint8_t { Accepted, Duplicate, WrongEpoch, Invalid, InningsClosed };
struct RulesConfig
{
    int Balls = 6;
    int MaxWickets = 2;
    int WidePenalty = 1;
    int NoBallPenalty = 1;
    bool FreeHitAfterNoBall = true;
};
struct DeliveryOutcome
{
    uint32_t Epoch = 0;
    uint32_t Id = 0;
    int BatRuns = 0;
    int CompletedRuns = 0;
    int Byes = 0;
    int LegByes = 0;
    int WideRuns = 0; // Total wide extras, including penalty.
    bool NoBall = false;
    Boundary Rope = Boundary::None;
    Dismissal Wicket = Dismissal::None;
    int DismissedBatter = -1; // -1 means striker; explicit identity for run outs.
    bool CrossedOnRunOut = false;
};
struct Innings
{
    int Runs = 0, Wickets = 0, LegalBalls = 0, Extras = 0;
    int Striker = 0, NonStriker = 1, NextBatter = 2;
    std::array<int,3> BatterRuns{};
    std::array<int,3> BatterBalls{};
    bool FreeHit = false;
    bool Closed = false;
    std::vector<DeliveryOutcome> Ledger;
};
class Match
{
public:
    RulesConfig Config;
    std::array<Innings,2> Scores{};
    uint32_t Epoch = 0;
    int Current = 0;
    bool ChaseStarted = false;
    Result Winner = Result::Playing;

    void Reset()
    {
        ++Epoch; Scores = {}; Current = 0; ChaseStarted = false;
        Winner = Result::Playing; LastId = 0;
    }
    const Innings& Now() const { return Scores[Current]; }
    int Target() const { return Scores[0].Runs + 1; }
    int RunsRequired() const { return Current == 1 ? std::max(0, Target() - Now().Runs) : 0; }
    int BallsRemaining() const { return std::max(0, Config.Balls - Now().LegalBalls); }
    bool StartChase()
    {
        if (!Scores[0].Closed || ChaseStarted || Winner != Result::Playing) return false;
        Current = 1; ChaseStarted = true; return true;
    }
    Commit Apply(DeliveryOutcome O)
    {
        if (O.Epoch != Epoch) return Commit::WrongEpoch;
        if (!O.Id || O.Id <= LastId) return Commit::Duplicate;
        if (Winner != Result::Playing || Now().Closed) return Commit::InningsClosed;
        if (O.BatRuns < 0 || O.Byes < 0 || O.LegByes < 0 || O.WideRuns < 0 || O.CompletedRuns < 0
            || O.CompletedRuns > 6 || O.BatRuns > 12 || O.Byes > 12 || O.LegByes > 12 || O.WideRuns > 13)
            return Commit::Invalid;
        if ((O.BatRuns && (O.Byes || O.LegByes)) || (O.Byes && O.LegByes)) return Commit::Invalid;
        // No-ball overrides wide. A struck delivery is never a wide.
        if (O.NoBall || O.BatRuns) O.WideRuns = 0;
        if (O.WideRuns && (O.Byes || O.LegByes)) return Commit::Invalid;
        Innings& S = Scores[Current];
        const bool Legal = !O.NoBall && O.WideRuns == 0;
        if (O.Wicket != Dismissal::RunOut && (O.NoBall || O.WideRuns || S.FreeHit)) O.Wicket = Dismissal::None;
        if (O.Rope != Boundary::None)
        {
            if (O.Wicket != Dismissal::None) return Commit::Invalid;
            if (O.Rope == Boundary::Six && (O.Byes || O.LegByes || O.WideRuns)) return Commit::Invalid;
            const int Value = O.Rope == Boundary::Six ? 6 : 4;
            if (O.WideRuns) O.WideRuns = Value + Config.WidePenalty;
            else if (O.Byes) O.Byes = Value;
            else if (O.LegByes) O.LegByes = Value;
            else O.BatRuns = Value;
            O.CompletedRuns = 0;
        }
        if (O.Wicket == Dismissal::Caught || O.Wicket == Dismissal::Bowled)
        { O.BatRuns = 0; O.CompletedRuns = 0; O.Byes = 0; O.LegByes = 0; }
        if (O.Wicket == Dismissal::RunOut && O.DismissedBatter != -1
            && O.DismissedBatter != S.Striker && O.DismissedBatter != S.NonStriker) return Commit::Invalid;
        LastId = O.Id;
        const int Extras = O.Byes + O.LegByes + O.WideRuns + (O.NoBall ? Config.NoBallPenalty : 0);
        S.Runs += O.BatRuns + Extras; S.Extras += Extras;
        S.BatterRuns[S.Striker] += O.BatRuns;
        if (O.WideRuns == 0) ++S.BatterBalls[S.Striker];
        if (Legal) ++S.LegalBalls;
        const int OutId = O.DismissedBatter == -1 ? S.Striker : O.DismissedBatter;
        if (O.CompletedRuns % 2) std::swap(S.Striker,S.NonStriker);
        if (O.Wicket == Dismissal::RunOut && O.CrossedOnRunOut) std::swap(S.Striker,S.NonStriker);
        // Target takes precedence over a later dismissal in the same reported play.
        const bool Won = Current == 1 && S.Runs >= Target();
        if (!Won && O.Wicket != Dismissal::None)
        {
            ++S.Wickets;
            if (S.Wickets < Config.MaxWickets)
            {
                if (S.Striker == OutId) S.Striker = S.NextBatter++;
                else S.NonStriker = S.NextBatter++;
            }
        }
        if (Legal) S.FreeHit = false;
        if (O.NoBall && Config.FreeHitAfterNoBall) S.FreeHit = true;
        S.Ledger.push_back(O);
        S.Closed = Won || S.LegalBalls >= Config.Balls || S.Wickets >= Config.MaxWickets;
        if (Current == 1 && S.Closed)
            Winner = S.Runs > Scores[0].Runs ? Result::SecondTeam : S.Runs == Scores[0].Runs ? Result::Tie : Result::FirstTeam;
        return Commit::Accepted;
    }
private:
    uint32_t LastId = 0;
};
}
