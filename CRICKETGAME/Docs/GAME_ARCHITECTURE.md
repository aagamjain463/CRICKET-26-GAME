# Active architecture

Authoritative baseline: `322e23c`; see transformation audit for current acceptance.
Preserve `Source/CRICKETGAME/SuperOver/Core/C26Rules.h` as the only score writer.

| Concern | Existing owner |
|---|---|
| Input / UI commands | C26PlayerController → C26MatchGameMode |
| Match / restart / innings | C26MatchGameMode → C26::Match::Apply |
| Scale / coordinate constants | C26Types.h / C26Field (centimetres) |
| Athlete rig scale / equipment | C26Athlete::BeginPlay / BuildEquipment / PlaceKit |
| Pitch / grass / lights / crowd | C26Stadium::BuildVenue and generated materials |
| Batting and shot tracking cameras | C26CameraDirector::Direct |
| Bowler run-up / release event | MatchGameMode::Tick / ReleaseBall |
| Ball flight, bounce and impact outcome | C26Simulation::Release / Integrate / Hit |
| Straight-drive pose | C26Athlete::Animate (procedural; asset blocker) |
| Bat-contact clock | C26MatchGameMode::UpdateDelivery; C26Field::BatContactPoseTime |
| Interception / pickup / throw | C26MatchGameMode::UpdateFielding / Collect |
| Replay | C26CameraDirector::Record / PlayReplay / Restore |
| Sound / haptics | C26Audio; MatchGameMode::Haptic |
| UI / persistence | C26HUD Canvas; C26Settings SaveGame |

Flow: Menu → Intro → Ready → RunUp → Delivery → InPlay → Reaction → Replay
→ Ready/Interval → chase → Result. Presentation observes rules; never move scoring
into cameras or animation. StartMatch resets all participating systems in place.

Assets: `/Game/Cricket26/Characters/SK_Cricketer`, matching skeleton and Remy materials;
four generic animations under Animations; 14 sound waves under Audio; runtime
geometry uses Materials and Stadium assets. Templates outside Cricket26 remain intact.
The map serializes native default subobjects: do not casually rename them.
