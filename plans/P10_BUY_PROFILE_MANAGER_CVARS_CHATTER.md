# P10 — Buying, Profiles, Bot Manager, CVARs and Chatter Parity

## Goal

Cover the configuration/personality/manager layer that makes two otherwise identical bots behave differently.

## Reference focus

- `game_shared/bot/bot_profile.*`
- `game_shared/bot/bot_manager.*`
- `dlls/bot/cs_bot_manager.*`
- `dlls/bot/cs_bot_chatter.*`
- Buy state
- bot-related CVAR definitions and command handlers

## Tasks

1. Inventory BotProfile fields that affect behavior: skill, aggression, teamwork, weapon preferences, reaction/personality attributes and any other pinned-reference fields.
2. Determine the intended compatibility source for stock `BotProfile.db`/pack/profile data. Prefer reading compatible data rather than hardcoding approximate profiles.
3. Map profile selection, duplicate/profile-in-use rules and bot creation defaults.
4. Reproduce buying sequence/decision timing, budget constraints, armor/kit/grenade decisions and primary/secondary upgrade logic.
5. Map bot quota modes and join behavior that affect runtime bot count and lifecycle.
6. Map bot commands/CVARs that alter behavior in the pinned reference, including freeze/deathmatch/join/team/difficulty or other relevant controls.
7. Map chatter/radio timing only to the extent it is part of the baseline. If audio assets are not a project goal, still preserve behavior-changing delays/events that chatter introduces.
8. Ensure compatibility CVAR values are captured in traces/reference fixtures.
9. Add fixtures for representative profiles and buy-money states.
10. Add manager lifecycle tests: add bot, remove bot, round transition, map transition, quota adjustment.

## Acceptance criteria

- profile-driven behavior is not replaced by one global Astra personality;
- buy behavior has deterministic fixtures for common money/loadout cases;
- bot creation/quota/lifecycle semantics are mapped;
- relevant CVARs are represented/documented;
- chatter/radio behavior that can alter decisions/timing is not accidentally omitted;
- build/smoke gate passes.

## Commit

Suggested message:

`feat(parity): align compatibility profiles buying and bot management`

Update STATUS/matrix, mark P10 complete, set P11 next, commit, stop.
