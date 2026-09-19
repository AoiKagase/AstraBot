# CSBot Behavioral Parity Matrix

Create `docs/parity/PARITY_MATRIX.md` from this template during P00. Add rows; do not delete a reference behavior simply because Astra has no implementation yet.

Status values:

- `UNMAPPED` — Astra location not identified.
- `MISSING` — reference behavior understood, Astra implementation absent.
- `PARTIAL` — some semantics implemented but mismatch exists.
- `MATCH-CODE` — source-level semantic review indicates parity; differential proof not yet available.
- `MATCH-TRACE` — deterministic differential fixture matches.
- `MATCH-LIVE` — representative live test also matches.
- `DEVIATION` — project-owner-approved difference with rationale in `KNOWN_DEVIATIONS.md`.

| Domain | Reference file/symbol | Observable contract | Astra file/symbol | Status | Test/trace | Notes |
|---|---|---|---|---|---|---|
| Runtime | `game_shared/bot/bot.cpp::CBot::BotThink` | update cadence/order | | UNMAPPED | | |
| Runtime | `CBot::ExecuteCommand` | usercmd/fake-client execution | | UNMAPPED | | |
| State | `CCSBot::SetState` | transition side effects/timestamp | | UNMAPPED | | |
| State | Attack overlay | attack update supersedes normal state update | | UNMAPPED | | |
| State | Idle | enter/update/exit behavior | | UNMAPPED | | |
| State | Buy | enter/update/exit behavior | | UNMAPPED | | |
| State | DefuseBomb | enter/update/exit behavior | | UNMAPPED | | |
| State | EscapeFromBomb | enter/update/exit behavior | | UNMAPPED | | |
| State | FetchBomb | enter/update/exit behavior | | UNMAPPED | | |
| State | Follow | enter/update/exit behavior | | UNMAPPED | | |
| State | Hide | enter/update/exit behavior | | UNMAPPED | | |
| State | Hunt | enter/update/exit behavior | | UNMAPPED | | |
| State | InvestigateNoise | enter/update/exit behavior | | UNMAPPED | | |
| State | MoveTo | enter/update/exit behavior | | UNMAPPED | | |
| State | PlantBomb | enter/update/exit behavior | | UNMAPPED | | |
| State | UseEntity | enter/update/exit behavior | | UNMAPPED | | |
| Perception | vision | visibility/enemy knowledge | | UNMAPPED | | |
| Perception | listen/noise | audible event knowledge | | UNMAPPED | | |
| Navigation | pathfind | route/cost/tie behavior | | UNMAPPED | | |
| Navigation | movement | path following/posture/stuck | | UNMAPPED | | |
| Combat | weapon/aim | aim/fire/reload/scope | | UNMAPPED | | |
| Objective | bomb | carry/fetch/plant/defuse/escape | | UNMAPPED | | |
| Objective | hostage | find/use/escort/rescue | | UNMAPPED | | |
| Team | radio/follow | commands/cooperation | | UNMAPPED | | |
| Economy | buy | purchase sequence/limits | | UNMAPPED | | |
| Profile | bot_profile | skill/personality/preferences | | UNMAPPED | | |
| Manager | cs_bot_manager | creation/quota/commands | | UNMAPPED | | |
| Chatter | cs_bot_chatter | behavior-affecting chatter/radio timing | | UNMAPPED | | |
