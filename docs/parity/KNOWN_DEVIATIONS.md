# P00 Known Deviations and Blockers

These are audit findings, not implementation instructions executed in P00.

## Critical

1. **No proven compatibility-mode boundary.** Current `compat` code provides
   commands, CVARs, profiles, and safety controls, but no explicit runtime mode
   that guarantees enhanced behavior is inert. `PluginRuntime` owns core combat,
   objective, and navigation controllers directly. This is the first P01 concern.
2. **Think timing is different.** CSBot has a 30Hz command interval and a 10Hz
   full-AI interval inside `CBot::BotThink`. Astra calls
   `updateManagedBotMovement()` from the post-StartFrame hook and does not expose
   the same two-clock order.
3. **RNG parity is absent.** Astra production code has no CSBot-compatible RNG
   source, seed capture, random-call tape, or call-site ordering. The roam
   controller uses a deterministic actor/generation-derived route index, which is
   known to differ from reference `RANDOM_*` use.
4. **Private state is unavailable.** Active weapon/ammo/recoil/reload/FOV,
   CSBot profile values, recognized-enemy queues, `CSGameState`, hostage state,
   and many player-private fields are not reachable through the current public
   boundary. Several current inputs are inferred or synthetic.
5. **The live action gate is not accepted.** Existing Windows evidence records
   team/class entry and human damage/death, but no sustained autonomous movement,
   Bot combat, or C4 event. Linux x86 live evidence and a pinned differential
   reference trace remain open.

## Major behavioral gaps

- The 13 reference state objects and `OnEnter`/`OnUpdate`/`OnExit` side effects
  are not represented by Astra's seven-state high-level machine.
- Live perception selects the nearest hostile public edict; it does not perform
  CSBot visibility, FOV, visible-part, hearing, event, or recognition-queue
  behavior.
- NAV parsing, corridor following, traversal, and stuck recovery are independent
  implementations. CSBot path costs, places, hiding/encounter spots, ladder
  details, tie breaks, and random choices are not proven equivalent.
- The runtime combat path constructs a synthetic rifle/weapon record and maps
  intents to public `IN_ATTACK`, `IN_USE`, and `reload` boundaries. This proves an
  action translation contract, not weapon behavior or damage.
- Objective integration is currently a narrow bomb carry/planted scan. Hostage,
  VIP, rescue, escape, follow, guard, chatter, and radio delivery are not live
  behavior.
- Bot manager/quota/profile buy behavior is not equivalent to
  `CCSBotManager`/`BotProfile`; profile parsing tests do not prove decision use.

## Evidence boundaries

- The fresh P00 offline baseline is configure/build + CTest 42/42 on Windows x86
  with the current working tree. It proves compilation and portable contracts
  only.
- PowerShell phase8 fixture/self-tests pass, including the action boundary check,
  but their logs are synthetic or source-boundary checks.
- Python manifest/artifact tests were not run because `py -3` could not create
  the installed Windows Store Python process. This is an environment gap, not a
  source pass/fail.
- No new HLDS/ReHLDS run was performed during P00. Existing live reports remain
  partial and are cited as historical evidence only.
