## 2026-11-18 repair unit08: Follow requests from planting plans

Base d67cb7e375034630c5866978b73d44e400891a3d. Native reference b0889847fe6d03898be88acc9e366660efb40ab5. `GoingToPlantBomb` attaches a Follow request when a carrier reports the plan. Natural completion delivers it in client-slot order. Receivers check Rogue, Busy, and SAFEST travel in that order, accept a distance of 0 through 1000 units, then follow and say `CoveringFriend`.

- On the arrival frame, the MoveTo state evaluates cover before the planting plan. The remaining-path check sums native NAV center distances after the current segment. Plan timing uses the strict two-second threshold and native shared 10-second interval. Voice selection uses the captured goal Place.
- The receiver follows its qualified managed leader as the leader moves. The runtime fixture exercises natural voiced completion, place-specific samples, movement commands, and slot-serial replacement. Core fixtures also cover callback invalidation, exact time and distance boundaries, defined silence, and same-key human/managed ownership.
- The final review found four important issues. The fixes preserve arrival-frame reports, managed leader identity, Place-aware voice selection, and the invalidated timer value of −1. The reviewed candidate passed six focused Linux tests before the complete builds.

| Gate | Result |
|---|---|
| Windows x86 Debug build / CTest | 138/138 passed, 11.30 seconds, MSVC 14.51.36231 Hostx86/x86 |
| Linux x86 Debug build / CTest | 136/137 passed, 14.14 seconds. The existing `astrabot_combat_policy` chest-threshold failure remains unit22 |
| Strict runtime characterization | Passed on both platforms with the existing baselines |
| Independent PE and ELF checks | x86 and exactly seven exports on both platforms |
| Source manifest and identity | 319 entries, 311 C/C++ files. All 358 build inputs, four user file hashes, and both module hashes matched across the gates |

Evidence is in `.agent-state/sessions/csbot-follow-meme-111c6372-9209-4aa2-ad25-066a116473fb/`. That session records local integration and artifact identity. The exact Windows candidate DLL and PDB were redeployed at the user's request, with matching target hashes and verified backups. ReHLDS was stopped at deployment. Live acceptance remains unit25, and the broader Follow state audit remains unit23.

## 2026-11-17 repair unit07: Help transmission and assistance movement

Base b3e4ec67a883a403464ecbc32f623853208b8c0a; read-only native reference b0889847fe6d03898be88acc9e366660efb40ab5. Original07 is implemented and offline verified;07A/07B remain its completed prerequisites. This does not establish overall CSBot or live physics parity.

- Natural last-part completion transfers an immutable typed Help payload before queue erasure/owner finish. Static NeedBackup and dynamic enemy-count Help preserve captured Place; cancellation/expiry/death does not transmit a successful completion.
- Actual Plugin recipient delivery follows native slot order, lazy shared Rogue, task-based Busy, SAFEST travel<=3000, strict3D750/current-position and native Place-selection rules. Accepted movement stores a copied goal and preserves the actual task; immediate OnMyWay uses normal alive/catalog admission. Dead teammates retain native state/task/attack/last-area memory for Help decisions and produce no living movement/acknowledgement. Normal reentry/round/respawn/owner/engine changes invalidate stale execution.
- Mixed floor/ladder routes keep typed ladder segments and resume the saved suffix. SDK-free native ladder phases, public entity/trace identity checks, native local movement/walking/recovery and the shared view spring are connected to actual RunPlayerMove commands. Runtime fixtures prove assistance ascent/dismount/copied-goal movement; Core covers native descending/phase/boundary/reentry behavior. Real server ladder physics remains live25.
- Native ResetValues clears Rogue status while retaining its absolute countdown; Spawn enters Buy, and ordinary navigation completion does not run a second normal-state update. Underlying retained Help task rules wait during Attack. Fresh final review found3Important issues (current sender NAV after Rogue, task suspension under Attack, pre-entry stuck forwarding); all were reproduced RED and repaired in one pass, with no re-review.
- The user explicitly prioritized CSBot parity on2026-11-17. Independent pre-feature controls reproduce149155 historical records on bothOS, then nine native prerequisite edits reproduce149075 current records byte-for-byte. Existing gzip baselines/two corrections are retained; a control-hash-bound600-edit correction keeps exact contexts, offsets, counts and whole-trace hashes. Native RNG/decision changes include37 fewer objective-visibility samples; none are globally ignored. See tests/fixtures/runtime_characterization/NATIVE_HELP_CONTROL.md.

| Final gate | Result |
|---|---|
| Windows x86 complete Debug build / CTest | PASS,137/137,12.68s; MSVC14.51.36231 Hostx86/x86, no compiler warnings |
| Linux x86 complete Debug build / CTest |135/136,13.99s; sole existing combat_policy chest-threshold failure remains22. Independently reproduced from pre-feature combat source, not waived |
| Strict characterization / checker | PASS on bothOS;149075 records;12checker cases |
| Separate actual Python PE / ELF verifier | PASS,x86 and exact7exports on bothOS |
| Manifest / input identity | PASS,317entries/309C++files;354build/test inputs frozen; owned/user/artifact fingerprints retained |

Evidence: .agent-state/sessions/csbot-help-meme-00ac5b3f-d412-446e-becb-904ba60872fd/ (final-*-5.log, native-control-proof-17.json, source/owned/user/artifact fingerprints). Exact commit/localFF/postmerge identity is recorded in its RESULT.json after integration. Unrelated user files, worktrees, configuration and evidence are preserved; no push/deployment/live acceptance. Remaining08–25, authoritative hostage/sniper producer audit19/23, RNG21/replay24 and C4/Aim/turn/multiplayer acceptance25 stay open.

## 2026-11-15 repair unit07B: public ladder travel prerequisite

Base0b15098d1ffa88ce3bfc5f803fe8758cd9423fc5. Reference ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5 nav_area.cpp564–613/973–1009/1520–1724, nav_area.h700–825/860–882 and cs_bot.h1574–1714. Original07 Help transmission/reaction/actual movement remains unfinished.

SDK-free public bounds/trace reconstruction preserves orientation, clearance scans, asymmetric top links, pre-adjustment length, highest connected top and strict72-unit dangling boundary. Read-only travel queries share native A* cost rules, search floor then the native first non-dangling up ladder then down, and return end-to-start extent-center distance along the selected route. Reverse connectivity for fall estimates includes all ladder references, independently of upward traversability. Existing ground movement never receives a ladder corridor.

Public runtime service lazily enumerates func_ladder with public Engine/entvars/traces, qualifies map/NAV/frame/round, Engine callback identity and entity serial/bounds across callbacks (including IndexOfEdict), rejects partial/stale results and caches accepted geometry per map/NAV. Separate probe availability distinguishes invalid inputs from legitimate completed no-ground, so a transient bad trace cannot cache a false disconnected bottom. No NAV authoring/private hooks/native bodies/assets/exports added; navigation-owner cache explicitly registered in ownership inventory.

Initial Core semantic RED0.04s; additional native iterator and actual runtime provider RED0/2 0.15s; high-descent reverse-connectivity RED0.05s; IndexOfEdict replacement RED0.14s. Tests exercise four orientations, clearance/height/length/dangling boundaries, actual public topology→real travel query, loaded floor/ladder ties, danger/crowd and Fastest/Safest selection, asymmetric/duplicate connections, stale/malformed topology, callback entity/Engine/actual round reset and cache retry. Existing actual Ack/Help/audio/reset/Agree controls retained. Fresh independent review0Critical1Important0Minor, source345/owned16/user4 fingerprints verified; the Important invalid-ground cache poisoning reproduced4semantic RED assertions0.14s, fixed in one pass; final focused6/6PASS2.10s. No re-review.

Final complete Windows x86 build/CTest135/135PASS12.11s under cached VS18/MSVC14.51.36231 Hostx86/x86/NMake (full confirmation build and CTest in the same initialized cmd process). Final complete Linux x86 build/CTest133/134PASS13.98s; sole unchanged astrabot_combat_policy weapon/skill/recoil chest threshold remains ordered22. Full OS suites serialized, no golden regeneration. Separate actual py -3 PE and ELF x86 exact7exports and manifest312entries/304C-C++PASS.345final source/build inputs,2artifacts and4user fingerprints match after full suites. DLL SHA256650cd8d4dd8fabb1d904f63a9e29320c01d4e30d6c340055c1587162ac253e3b; SO57ca7e851771a53ca654d7e9a3ce486b985bc9d4813fd8b155dc808ec3c7a64a. Initial pre-fix builds/Linux suite are separately named evidence and do not substitute for final gates.

Rulings: deliver required travel-query foundation as07B while keeping original07 Help/ladder locomotion unfinished (assistance movement not yet provided); live25 and exhaustive floating/replay23/24/RNG21 remain open (runtime/threshold divergence not excluded). Shared checkout/caches/user edits/evidence retained with fingerprints (mixed-evidence risk). Final execution gates are parent-run, independent of source review (stale binary risk checked). Reviewer's native Git permission limit resolved by parent raw Git pin and clean bot/vector/mathlib paths (reference identity risk checked). Known22 stays explicitly open per ordered repairs. Exact17owned paths, no push/deploy/live certification. Evidence .agent-state/sessions/csbot-nav-ladder-travel-d0ecc1a9-c281-48b4-8296-5c020ac7fca2/; commit/localFF/postmerge receipt is its RESULT.json. Parent07 investigation .agent-state/sessions/csbot-help-meme-00ac5b3f-d412-446e-becb-904ba60872fd/STATE.md must resume. Full goal active07 onward.

## 2026-11-15 repair unit07A: native NAV-center prerequisite

Source base26eaea2dc99e213ee85c33fd10b678102e9d53e4; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, nav_area.cpp214–216/230–232/245–247 and nav_area.h860–882. This prerequisite was discovered while implementing07 Help; original07 transmission/reaction/lifetime/distance reporting remains unfinished, not waived or declared complete.

- Actual Compatibility path-cost contexts opt into extent-midpoint x/y/z for shared A* edge distance and heuristics, matching native CNavArea centers. Default/Enhanced contexts retain floor-center behavior. Native coincident centers retain a valid0-cost edge rather than the port minimum; strict cost improvement and loaded open-list order remain. The SDK-free extentCenterOf utility supplies the factual geometry for later Help distance/place work.
- Surface lookup, portal steering and fall-damage height remain bilinear. Safest danger, health, crouch, jump and crowd costs remain. A global replacement of floor geometry would have changed those meanings; a separate Help A* would duplicate path rules. The context-selected shared implementation repairs production routing without new services, private/test-only API, exports, assets or native bodies. NAV files remain read-only.

Initial real A* Core2semanticRED0.04s and actual public-mode/service-context runtime2semanticRED0.14s preceded behavior implementation. Hand-checked saddle paths distinguish native1→2→4/cost128 from default1→3→4; danger still selects a safer detour, coincident native centers cost0, and a lethal one-way floor drop remains blocked. Runtime mode Compatibility→Enhanced→Compatibility drives actual NavQuery through real Plugin cost contexts and restores the original mode; existing Ack/Help/reset/Agree controls remain. Focused6/6PASS2.11s. Fresh independent source review0Critical/0Important/0Minor independently checked343source and4user fingerprints; no fixpass/re-review.

Complete Windows x86 build/CTest135/135PASS11.62s under cached VS18/MSVC14.51.36231 Hostx86/x86/NMake; complete Linux x86 build/CTest133/134PASS13.82s, sole unchanged astrabot_combat_policy weapon/skill/recoil chest-threshold failure remains ordered22. Full OS suites serialized; no frozen/golden regeneration. Separate PE/ELF x86 exact7exports and manifest310entries/302C-C++PASS.343reviewed/full-build inputs,2artifacts and4user fingerprints match after the suites. DLL SHA2568e276d00d4795f85377bd067851d9563d5ae6432c43df9fcfdfcce802ee0bd8d; SOb2fdb76f02ca072986f20d2e637f29026dd9296ed2840211516f3c9a2787ad47.

Rulings: deliver this required geometry dependency as07A while retaining original07 and the full goal; cost is that Help remains unavailable. Parent07 still requires exact read-only public ladder topology, natural-completion meme transport and actual response/movement integration; this change does not establish full NAV reachability. Global RNG/replay21/24, broader native audit23, known22 and live HLDS/ReHLDS/multiplayer25 remain open. Source approval is separate from current executor full/artifact gates. Existing shared checkout/caches/user edits/evidence are retained with exact paths/fingerprints (mixed-evidence risk); ordered integration keeps22 open instead of changing unrelated combat policy here. Exact9owned paths only, no push/deploy/live certification. Evidence .agent-state/sessions/csbot-nav-centers-bab59a63-89dd-4326-bd59-f92f9f3b4ded/, per-unit commit/localFF RESULT.json; parent07 investigation .agent-state/sessions/csbot-help-meme-00ac5b3f-d412-446e-becb-904ba60872fd/STATE.md must resume afterward.

## 2026-11-15 repair unit06: native same-plan agreement conversion

Source basea07f7e7e42f7b363827da94df06ecf6718ab91ee; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp935–962/1451–1527. User-approved ordered01→25/per-unit commit/localFF continues.

- Pending MyPlan pairs compare the actual prepared first phrase's default-catalog ID. A chatter-only ID lookup avoids same-name Place shadowing. Same Place, including undefined0, replaces only the first part with AgreeWithPlan and shifts start by the native random.5–1seconds. Different places keep the original phrase and repeatedly shift start by3–4seconds. Already-agreed, speaking, invalid, obsolete and different-topic/phrase entries do not draw. A missing agreement definition becomes a delayed null part. Creation, expiry, subject, conditions, remaining parts and linked insertion position remain.
- Production maintenance follows selected action then second team selection. The fixed metadata/profile parser recognizes AgreeWithPlan, and the stored converted program reaches real playback. Core revision checks fence callback-capable RNG before reusing a node. Runtime reuses the existing pure peer receipt for owner/catalog/frame/team and all managed-peer/exclusion checks without another Engine lookup; own queue changes are independently checked per draw. Reset/replacement or changed peer receipt cannot publish stale conversion or remove a fresh node. No new virtual/private/test-only API, assets, native bodies or exports.

Core15semanticRED preceded conversion behavior. The actual registered component fixture preserves existing Ack/Help/reset controls, creates a real second managed owner through the public command, binds real Plugin services and supplies the existing typed escort-decision input. It proves normal escort→wait→agreement and actual Engine RNG reset/peer-team callbacks. Fixture prerequisites were corrected from source evidence: quota2/unique profile, a real round boundary to clear the preceding visibility cache, and initial RNG binding because EngineRandomSource copies callbacks at configuration. Initial runtime normal2RED is valid; initially unbound callback assertions are not claimed as mutation proof. A postimplementation peer-guard negative control failed exactly1semantic assertion0.12s. Proper source restoration by writeFile and actual radio.cpp recompilation preceded final7/7PASS2.08s. Independent fresh source review0Critical/0Important/0Minor, no fixpass/re-review.

Complete Windows x86 build/CTest135/135PASS12.88s under cached VS18/MSVC14.51.36231 Hostx86/x86/NMake; complete Linux x86 build/CTest133/134PASS15.44s, sole unchanged astrabot_combat_policy weapon/skill/recoil chest-threshold failure remains22. Full OS suites serialized; no frozen/golden regeneration. Independent PE/ELF x86 exact7exports and manifest310entries/302C-C++PASS.343reviewed/full-build inputs,2artifact and4user fingerprints remain equal after the suites. DLL SHA256d48f622742d56cbadd5680a185befe52debd7114bb22bb330aa73c34695f9ef6; SO4aa54814e028e9d8396726bfd47292d40329a5048502956246d74f88b396d5d5.

Rulings: preserve shared checkout/caches/evidence with exact paths/fingerprints (mixed-evidence risk); extract/reuse pure peer validation (callback/performance regression risk tested offline, not live FPS); full hostage producer/new MyPlan constructors stay12/15 (component input does not certify them); global RNG/native replay stays21/24; reference-wide scheduler/trigger/lifetime audit stays23; live HLDS/audio/multiplayer/deployment stays25. Source review is separate from current full/artifact gates. Honor ordered integration with known22 explicitly open rather than changing unrelated combat policy in06. Exact12owned paths only, unrelated edits retained. Evidence .agent-state/sessions/csbot-plan-agreement-9b0062dc-e4f8-427e-9a98-a69f619c2581/, including per-unit commit/localFF RESULT.json. Full goal active07 onward; no push/deployment/live certification.

## 2026-11-15 repair unit05: delayed human acknowledgement reset

Source base56a5af1765779a51975d6c946dbe677db2ef2439; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_init.cpp178/272–275, cs_bot_radio.cpp79–86/216 and cs_bot_chatter.cpp1256–1267.

- Deferred Affirmative/OnMyWay reservations now cancel on discard, death and metadata reset. Their saved admission must retain actor/entity/connection/map/round/lifetime identity; ordinary forward ticks/time and legacy revision0/null clock remain valid. Cancellation preserves the consumed source sequence, so an old order cannot rearm while a fresh higher sequence can still acknowledge after the native1+2reaction delay.
- Observation independently invalidates unprepared radio input on actor/shared/round reset, remember/removal and qualified public death. Its own input revision is independent of speech completion and initial owner synchronization. Receiving revisions are captured before MessageBegin callbacks; immutable SendAudio fields, frame/receiver checks and a source-sequence baseline prevent a packet spanning reset or old callback completion from replacing a fresh command. No coordinates, new service ports, private/test-only API, native bodies/assets or exports are introduced.

Initial Core semantic RED8 and actual observation-decoder RED7 preceded production changes. The registered runtime fixture uses the real heap observation component, real Plugin services and production protocol/reset methods; whole-Plugin reset remains private. Same actor/serial/map/round/tick, same-time fresh input, current Ack/Help and forward-time controls remain. Two additional postimplementation mutation controls independently failed for message-ID reset requalification/newer-packet overwrite, then passed after proper source restoration and actual recompilation. Final focused4/4PASS2.19s. Independent fresh source review0Critical/0Important/0Minor; no fixpass/re-review.

Complete Windows x86 build/CTest135/135PASS11.18s under cached VS18/MSVC14.51.36231 Hostx86/x86/NMake; complete Linux x86 build/CTest133/134PASS15.57s. Sole unchanged combat_policy weapon/skill/recoil chest-threshold failure remains ordered unit22; no new failure or frozen/golden regeneration. OS suites serialized. Independent PE/ELF x86 exact7exports and manifest310entries/302C-C++PASS.343reviewed/full-build inputs,2artifacts and4user fingerprints remain identical through verification. DLL SHA25645ae5ef6c4a7662cb97ba72c2492ef836286295381af4e53260dea0c662134e3; SOb2e49ca8a92e3c238af308c477cd73cf7df0162b21446a609437fa6d6a835149.

Review rulings: broader nested decoding/team/lifecycle/trigger audit stays23 (not certified by this local reset contract); native RoundEnd preservation stays14 (old constructor behavior remains); source verdict does not certify binaries, so current executor full/artifact gates are recorded separately; live HLDS/audio/quota/multiplayer stays25 (deployment behavior remains unaccepted). Existing shared checkout/cache/evidence and unrelated edits are preserved. Explicit10owned paths only; per-unit commit/localFF receipt in .agent-state/sessions/csbot-ack-reset-2881f644-4920-44c4-b9f3-1c2f17c2d9cc/RESULT.json. No push/deploy.06conversion,07–20constructors/memes/intake,21RNG,22knownLinux,23fullaudit,24reference replay and25live remain active.

## 2026-11-15 repair unit04: native one-shot standard radio

Source basea76e69dda4ef570dbfd386147757662e689151d2; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp1105–1182/1599–1616, cs_bot_radio.cpp309–347, states/cs_bot_idle.cpp518–521 and cs_bot_manager.cpp114–126/1882–1905.

- Mapped radio-mode and unavailable-custom-voice fallback parts attempt one public GameDLL menu/select/close sequence, independent of dispatcher return or SendAudio. They reserve native2seconds+.1 even when the host declines quota. Each command revalidates exact owner/part receipts; obsolete completion cannot remove a fresh admission. Radio-mode silence resets before the attempt; fallback silence resets after it. Undefined versus defined-unmapped0+.1 behavior remains03's distinction.
- Direct VIP FollowMe consumes only its exact request before callback-capable dispatch and adds no Chatter speaking duration or silence reset. The used FollowMe event timestamp is shared per team, starts0, resets map/round, and is stamped before commands for any mapped1/5 attempt. Eligibility is strict absolute now−teamTime>30. The old actor-local interval is removed, so team change/reconnection cannot import another team's cooldown; ordinary Chatter silence remains independent.
- Obsolete receipt/attempt counters, retry-part storage/helpers and the internal own-SendAudio observation port are removed. Public human-order intake and GameDLL-owned radio/audio/UI remain. No private player state, native bodies/assets, ReAPI or exports are introduced.

Initial semantic RED3/3FAIL0.27s covers ordinary/direct quota-declined repeats and Core initial/shared30/nonsilence differences. An additional team-transfer RED isolated the harmful actor clock; its removal passed Core1/1. Actual multipart tests cover one3/4 command sequence without receipt in radio and missing-custom-API modes, strict2.1 reservation, a separate accumulation update, later3/2, and round mutation at each command while preserving fresh CommanderDown. Actual declined VIP, accepted transport, death, voice, queue, ports and ownership controls pass. Independent final source review0Critical/0Important/0Minor; no fixpass/re-review needed. Parent independently verified the native pin and clean used source directories after the reviewer's native git read was denied.

Full verification exposed two old VIP time preconditions: the runtime fixture was at27.26 with0opens/0sends, and Core requested at15. Both contradicted native zero-initialized strict>30. Fixtures now qualify the real VIP control at31 and preserve Core death cancellation against a shared team attempt at31/next32; existing assertions remain. Focused11/11PASS3.27s. Final complete Windows x86 build/CTest135/135PASS11.48s under cached VS18/MSVC14.51.36231 Hostx86/x86; Linux x86 complete build/CTest133/134PASS16.81s, sole unchanged combat_policy chest-threshold failure remains22. Suites serialized; no frozen/golden regeneration.

Independent PE/ELF x86 exact7exportsPASS; manifest310entries/302C-C++PASS.343source/build/project inputs,2artifact and4user-file fingerprints match final verification; documentation-only closeout is outside those build inputs. DLL SHA25684d3e50cb69c7e7a02b2d32bacc3c8798bbc8efce770a2b379bfc79f5e9a5237; SOa8dee4ad72ff44e273ffed7f4c59ac76bc0fe31ffb6d81d302cb3123c86a4a59. Exact19owned paths are staged separately from unrelated user edits; existing caches and generated evidence are retained. Evidence: .agent-state/sessions/csbot-radio-once-348ad649-4451-4e54-ae7a-7777bdb2be96/, including RED/GREEN/diagnostic/full logs, single review, identities and per-unit commit/localFF RESULT.json.

05 delayed same-owner acknowledgement reset,06 conversion,07–20 remaining constructors/memes/exact event intake,21CRT RNG,22known Linux failure,23full source/lifetime audit including older noise probe and NAV spatial performance,24reference replay and25live acceptance remain active. Standard transport source review/offline tests do not certify real HLDS quota/audio, C4/Aim/turning/multiplayerFPS or full native parity. No push/deployment.

## 2026-11-15 repair unit03: production compound chatter

Source base804fe13b2ad7ed962d602773ac6d1b19ab1a5ec6; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp964–1185/1451–1582/1625–1657/1796–1818, cs_bot_vision.cpp846–979, cs_bot_update.cpp274–299 and nav_area.cpp1839–1916/4318–4392/4774–4875. User-approved01→25 order and per-unit commit/local FF remain binding.

- Production statements retain one stored program and exact node/part tokens. NeedBackup composes own Place→Help; EnemySpotted composes current-visible enemy Place→accumulation→dynamic count/Help. Selected action precedes second team selection and own maintenance. Logical speech persists through null parts, strict delays, suppressed Place audio and the separate completion update. Combat-condition payload cancels after the selected action; death during silent accumulation emits native pain. The01Minor distinction is now integrated: null skips with nextTime0, while a defined inaudible part reserves0+.1.
- Current-visible enemy Place uses native client-slot/distance ordering and the online strict-leader count rule. NAV-local scheduling places and metadata criteria IDs remain separate. Managed actors retain current/last-known areas using Contains→GetNavArea and clear ownership on lifecycle changes; current area can become null while last-known remains. Humans use native quick floor selection, public ground layers/brush classification, strict3D nearest cutoff, LOS and loaded-order ties. No private player state or NAV authoring is introduced.
- One shared64Place history serves both teams and resets map/round. Sample selection/duration probing precedes the20sec decision; suppression retains duration+.1 without audio/silence/feedback effects. Shared bank ownership, copied clips and map/round/catalog/part receipts fence load/filter/shuffle/wave/ground callbacks. Interrupted first profile loading removes only its exact unpublished bank so the next round can retry. Public custom-audio API absence still falls back to mapped radio; zero eligible or ignored listeners still consume usable voice playback.

Actual registered runtime coverage includes NAV Place77 versus metadata1, ordered same-sender Place→Help, suppressed-duration retention, strict2sec accumulation, changed enemy counts/later outnumbering, silent death, round mutation during waveform/ground queries with fresh CommanderDown survival, stale duration-cache rejection and managed last-area production. New fixtures explicitly bound7client storage, establish real combat, register DeathMsg and use a genuinely off-mesh coordinate. Initial runtime/Core semantic REDs precede implementation. One fresh final review found0Critical/1Important/1Minor; both first-bank load/free round controls failed before the exact-entry retry fix and passed afterward. Focused9/9PASS0.41s; no re-review. The Minor linear NAV lookup cost remains an explicit23 performance follow-up, not measured live acceptance.

Complete suites initially exposed the existing native completion-only timing expectation and a custom-audio-unavailable fallback regression. The timing fixture now also asserts no radio on the completion update, then the unchanged KilledFriend on the next update; fallback preflight was repaired without changing zero-recipient semantics. Focused5/5PASS0.50s. Final complete Windows x86 build/CTest135/135PASS15.52s, VS18/MSVC14.51.36231 Hostx86/x86 matches cached NMake. Final complete Linux x86 build/CTest133/134PASS16.46s, sole unchanged combat_policy weapon/skill/recoil chest-threshold failure remains22. OS suites serialized; frozen baselines were not regenerated.

Independent PE/ELF x86 exact7exportsPASS; manifest310entries/302C-C++PASS.343source/build/project inputs match the final full-build/full-suite input snapshot before documentation-only closeout; final fingerprints retain that distinction. Final source/artifact/user identities are in the session. DLL SHA2562bc59cbca7f3fc30b6ab30e502f7c5eee77c42cc2ef1e5deaab11119093b3b52; SOb188082678ab979d227e496a226a06ea0fc02813d62dbea2f7b966993f8454a7. Used native files remain clean at the oracle pin;4user files are preserved.

04 still owns native one-shot radio/quota-rejected duration and direct-VIP retry removal;03 intentionally retains qualified receipt retries and does not claim their parity.05 delayed Ack reset,06 conversion,07–15 memes/new constructors,16–20 exact event intake,21CRT RNG,22known Linux failure,23full audit (including older noise ground probe and spatial NAV performance),24reference replay and25live acceptance remain active. Shared checkout/caches/unrelated files are preserved; no push, deployment or HLDS/client/multiplayer/FPS certification. Evidence: .agent-state/sessions/csbot-compound-runtime-9dda5683-56b6-405d-97d1-8788d4d7a643/; per-unit commit/local FF receipt is RESULT.json.

## 2026-11-15 repair unit02: Place voice metadata and shared repeat policy

Source base909c885a31c214ec589e9224cd0862348a36c381; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp274–775/1105–1185, chatter.h284–355 and nav_area.cpp4906–5033. User-approved sequence01→02→03 and per-unit commit/local FF remain binding.

- Default definitions retain all source Chatter/Place IDs and separate name namespaces, Place Important/radio and duplicate occurrences. Profile attachment requires an existing same-kind name and preserves default-owned flags; Place samples have their own bank cursors, count/place criteria, availability filtering and Place-first shuffle. Missing profile samples do not fall back to bank0. Unrepresented Chatter names validate without extending fixed dispatch15.
- Named criteria bind numeric IDs at parse time: default prefix visibility, Place-before-Chatter lookup, unknown→UNDEFINED and no-Place-only104name fallback. Independent sorted factual lookup includes no native implementation/audio assets. Copied filter/shuffle candidates and mutation revisions prevent stale reentrant publication; shuffle preserves callback cursor changes. Borrowed sample/outer bank lifetime and actual transport remain03; exact CRT/global RNG order remains21.
- Native shared repeat component uses64tracked IDs across teams, first/exact20sec release, denied attempts without timer reset, untracked full-table entries, explicit reset and rewind suppression.03owns the one manager instance, map/round reset, post-selection/probe invocation and duration/audio/feedback semantics.

Existing-API loader RED dropped Place while Helpcontrol passed; criteria RED showed unknown literal misbinding, and the history no-op stub denied the first Place. Focused Core4/4 and real existing runtime3/3 passed before review. Three runtime fixture DBs now declare empty Speaker/Noise definitions rather than relying on nonnative literal name matching. A fourth missing-default-pain fixture now omits that nonexistent profile definition, preserving its real CommanderDown control and all assertions under native same-kind validation. Its focused4/4PASS0.32s precedes final full sweeps. No audio assets, extra shuffle draws or frozen baselines were introduced.

Single fresh source review0Critical/1Important/0Minor. I1 default metadata operand/empty-quote parsing was reproduced by6semantic failures0.04s, then repaired in one pass. Keyword Place names Important/Radio/End remain values, BaseDir's operand is consumed, and a closed empty quoted BaseDir no longer truncates definitions. Repaired focused5/5PASS0.10s; no re-review. Verified owned pre-fix build process trees were stopped and partial logs preserved. Final complete platform/artifact gates and identities: Windows x86 complete build/CTest127/127PASS9.57s, VS18/MSVC14.51.36231 Hostx86/x86 matches cached NMake compiler. Linux x86 complete build/CTest125/126PASS14.80s, sole unchanged astrabot_combat_policy chest-threshold failure remains22. OS suites serialized. PE/ELF x86 independently exact7exportsPASS; manifest301entries/293C-C++PASS. Final334source/build/project/ownership inputs,2artifacts and4user files match saved identities. DLL SHA25622750102c6911a7be40093c7950c77e6c6a4c6e2afcedfaec8111080b5867bed; SObe1009d010e8423247d230d63a0b14b1d198c6ef0642e27d75a45b53794babaf. Used native chatter/header/NAV/shared parser files remain clean at the oracle pin.

Existing shared checkout/caches/evidence and unrelated user files are preserved with exact owned paths/fingerprints. Transactional malformed-load,1MiB/128samples safety limits remain explicit port policies; no malformed-file parity waiver. Source review does not certify offline platform/artifact/live gates. Actual Place packet delivery/history suppression and01Minor skip/null vs defined0+.1 remain03. Full goal and known Linux combat_policy22 remain active; no push/deployment/live acceptance claim. Evidence: .agent-state/sessions/csbot-place-audio-5a2b972e-13b7-4b29-828e-6498a4402639/, local commit/FF receipt RESULT.json.

## 2026-11-15 repair unit01: compound playback Core

Source based7f403ff5c6fef0b42c43140a5b213a38eefd386; native read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp964–1185/header358–455. User explicitly requests inventory01 onward in order, committing and locally merging each repair unit. This unit supplies the SDK-free model;02Place metadata/history and03actual runtime multipart transport remain open.

- Max4 inline fixed/place/current-count/remaining-count/short/long/accumulation parts advance one part per selected invocation, retain logical speaking through silent contexts, and finish on a separate strict update. Current context is evaluated at part start; accumulation uses >3 enemies or strict >2sec from statement speaking start.
- State-scoped step receipts reject stale completion/delay/cancellation; in-flight delay prevents reentrant advancement and can retry unavailable results. Short/long ranges and ordinary duration+.1/native2sec radio input are exposed as values without Core Engine/RNG callbacks. Queue/actor ownership is explicitly the03adapter's responsibility.
- Initial Core interface stub RED precedes implementation. Existing delay failure was a fixture precision mismatch: stored float32 .1 versus excess-precision x87 decimal expression. A diagnostic proves exact values; strict float32-bit equality follows the existing compatibility RNG boundary, with no epsilon or production numeric change. Expanded Core progression/minimum/range/null/Place identity/Emote/invalid-time/empty-program cases pass.

Single fresh source review0Critical/0Important/1Minor. Minor deferred to03: make skip=true null-phrase semantics explicit in adapter-facing docs/tests; a valid but inaudible phrase reserves0+.1. Current model supports both, no Important fix pass/re-review. Immutable-program/state-scoped Core contract is explicit; no runtime/metadata/transport/live certification from this unit. Pending02/03 fixture/history bytes and original actual missing-Place RED remain preserved in the parent compound session.

Final offline gates: Windows x86 complete build/CTest123/123 PASS11.72s, VS18/MSVC14.51.36231 Hostx86/x86 matches cached NMake compiler; Linux x86 complete build/CTest121/122 PASS14.29s. Sole failure remains unchanged astrabot_combat_policy (weapon/skill/recoil chest policy matches reference thresholds), repair unit22. All new Core and existing frozen/ownership/ports/architecture/performance/runtime gates pass; no golden regeneration. OS suites serialized.

PE/ELF x86 independently exact7exports PASS; manifest297entries/289C-C++ PASS. Final300source/build/ownership inputs,2artifacts and4user files match saved identities. SHA256 DLL381b33c4476f4171a88edc6b60e44d472d996257e2344b0d481b15c4a302cdcd; SO2e09551b57a0a0b2a06356fd23a0cdf23d77c991a513cc17560110694d5aebd5. Native HEAD and used chatter.cpp/.h remain clean at the oracle pin. Evidence: .agent-state/sessions/csbot-playback-core-dc287233-e121-431f-95fe-98f64f66a1d8/ (STATE, original parent RED/diagnostic references, focused/full gates, final review, artifact/manifest, fingerprints). Local commit/FF receipt is session RESULT.json.

Decisions: existing shared checkout/caches/user evidence retained with exact paths/fingerprints (mixed-evidence/disk risk); user's explicit per-unit split is honored (runtime behavior remains pending03 rather than claimed solved); source review is separate from platform/artifact/live gates. Full native parity goal stays active, including known Linux combat_policy baseline and deferred Minor; no push/deployment/live acceptance claim.

## 2026-11-15 common pending statement collection

Source base7105bd88b168df8ebd1371ed006b97f3602d2ede; read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp777–910/1256–1374/1451–1582/1884–1905/2254–2262 and represented fixed constructors.

- Generic/count/praise/friend-death statements enter one linked per-actor collection. Stable start/list ordering, shared own pending/retained topic-place-subject redundancy, native exceptions and default-catalog existence/Important admission replace split single slots. o/O and m/M gate new admissions only; r/R selects standard-radio transport. Old admitted statements survive later mode changes. Constructor RNG precedes admission gates; event admission does no profile/wave I/O.
- Qualified monotonic node IDs and queue mutation revisions survive reset. Exact-node confirmation/discard cannot erase a fresh same-phrase node. Production event ownership is captured before callback-capable RNG/catalog work and revalidated afterward. Actor copy resets are removed; direct VIP retains an independent token.
- Allocation-free team selection streams actual slot/list order without a fixed6candidate view cap. All managed eligibility/exclusion and queue/retained revisions are fenced across Engine callbacks. Selected action precedes second team selection and the updating owner's entire pending maintenance even when unselected; strict obsolete time, speaking exemption and native redundancy follow represented validity checks.
- Actual registered Go/visibility/SendAudio regression first reproduces dropped Help while acknowledgement succeeds; after migration both clips arrive. CountMany decoy verifies fixed Ack undefined count and later OFF verifies retained admission. Core covers12simultaneous acknowledgements, stable insertion, exception/cross-source/retained/place/subject redundancy, modes, strict maintenance and stale handle/owner resets. Frozen baselines are unchanged.

Single fresh final source review0Critical/1Important/0Minor. Important current-round event loss was reproduced independently by the existing actual kill-praise fixture and Core admission-before-prepare test. The one fix pass removes the late all-queue round reset and separates ordinary trigger metadata from qualified queue lifecycle synchronization. New events admitted before first update survive. No re-review or deferred Minor.

Final offline gates: complete Windows x86 build/CTest122/122 PASS9.56s under VS18/MSVC14.51.36231 Hostx86/x86; complete Linux x86 build/CTest120/121 PASS14.22s. The sole remaining failure is unchanged astrabot_combat_policy (weapon/skill/recoil chest policy matches reference thresholds), still open. Actual new queue/boundary/unselected-expiry and existing death/icon/voice/praise/noise, unchanged frozen characterization, ownership/architecture/ports/performance contracts pass. OS suites serialized; no golden regeneration. First whole sweeps exposed default-definition/no-friend fixture omissions and prior pre-action-expiry expectations; source-backed corrected fixtures plus added actual blocked-owner-expiry coverage pass4/4 before final complete suites.

PE/ELF x86 exact7exports independently PASS; manifest295entries/287C-C++ PASS. Final298source/build/ownership inputs,2artifacts and4user files match saved identities. SHA256: DLL82a1078b503220e5dad7956cb44e10f09b46a95e6f9f05a78a918c93c12c0dad; SO2e09551b57a0a0b2a06356fd23a0cdf23d77c991a513cc17560110694d5aebd5. Native HEAD and used chatter/radio/idle files remain clean at the above oracle pin; public Metamod-P7ec9b014f8c0a947a724644aebe34eb33706e44b. Evidence: .agent-state/sessions/csbot-statement-queue-b9c665bd-2f84-41b4-a720-58ca74e5dc2e/ (STATE, single final review, actual RED/GREEN, full builds/suites, separate artifact/manifest, fingerprints). Verified owned pre-fix build trees were stopped and partial logs preserved; complete corrected builds provide accepted artifacts. Local commit/FF receipt is session RESULT.json.

Decisions retained: established shared checkout/cache/user evidence with exact paths/fingerprints (mixed-evidence/disk risk); existing continuous implementation/local integration authority used without renewed menu (builder choices remain ours); this is common storage/admission and represented maintenance, while compound composition/conversion/memes/unrepresented round-end/dead constructors and broader trigger/transport parity remain open (no parity waiver). Pre-existing retained EnemySpotted combat-condition payload and same-owner delayed Ack reset cancellation remain named bridge followups; Core validity is not production parity for them. Source review does not certify OS/artifact/live gates. Live HLDS/ReHLDS, client/audio and multiplayer/FPS acceptance remain unverified; no push/deployment/full-goal completion claim.

## 2026-11-15 team-wide statement arbitration

Source base c60ffcf4ccc65d32dcd1730cedc6e2ec5e16e0c1; read-only ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp877–910/1290–1374/1451–1582, chatter.h612 and cs_bot_radio.cpp134/206/303, states/cs_bot_idle.cpp520–521.

- All currently represented generic/count/praise/friend-death pending entries share actual actor-wide successful-admission ordering. Candidate views preserve creation and start times, original native report topic/place/subject, actor/serial/connection/lifetime ownership and retained speaking. Core remains SDK-free; no new exports/private data/assets/RNG/profile I/O.
- Actual voice and mapped radio authorization scans teammates in client-slot and owner-list order. First speaking wins; otherwise native compares start against earlyTime but assigns creation timestamp, then rejects a selected future start. This intentionally differs from a minimum-ready-start heuristic. Both friend-death entries are visible; only the selected owner evaluates speech context.
- A speaking statement remains visible to peers beyond nominal audio duration until its owner completes it. Completion uses strict now>nextTime and consumes that owner update. Native topic/place/subject redundancy, including plan/help/critical/acknowledge exceptions, is applied against represented active teammate statements. OnMyWay is a native Say/intention; Affirmative is acknowledge.
- Peer scans snapshot every managed peer, including exclusions, before callback-capable team/name lookup and validate all identity/eligibility receipts again without callbacks. Earlier enemy/unnamed/dormant/reused peer changes cannot authorize a stale scan. Direct VIP FollowMe bypasses peer scans and keeps independent expiry/confirmation/discard, own receipt checks and pending ordinary speech.
- Actual registered DeathMsg/SendAudio tests record original senders and exercise reverse update order, native future blocker, retained peer past duration, completion gap, eligibility and public TeamInfo/disconnect callbacks. Public VIP ScoreAttrib plus safety-zone navigation creates a real Escape decision and GameDLL command/public SendAudio confirms independent direct transport. Core tests cover native selection/ordering and redundancy tables plus blocked generic readiness. Frozen inputs are not regenerated.

Single fresh final source review0Critical/2Important/0Minor. Both Important findings were reproduced in review-red.log (excluded-peer requalification and direct VIP suppressed by unrelated scan failure), then fixed together; review-green.log7/7 PASS0.19s. No re-review/deferred Minor. Earlier pre-review evidence distinguishes actual REDs from explicit mutation controls and setup errors: initial older-peer0.125s gap was not valid native expectation and was corrected to0.875s; actual own-only-scan control fails four routing/retention assertions, then restored full scan passes. SHORT protocol tests from the previous feature are untouched.

Complete-suite verification first exposed an unregistered admission-clock owner, extra Engine name/team callbacks in empty candidate sets, and an old praise same-update continuation expectation. The owner inventory is now explicit and included in the manifest; candidate snapshots skip empty-set callbacks and also validate queued-state changes. Praise now asserts the native completion-only update, then the unchanged count output on the next tick. Focused ownership/unchanged characterization/praise/team gate4/4 PASS2.34s; no frozen evidence regeneration.

Final offline gates: complete Windows x86 build/CTest120/120 PASS9.91s under VS18/MSVC14.51.36231 Hostx86/x86; complete Linux x86 build/CTest118/119 PASS12.62s. The sole remaining failure is unchanged astrabot_combat_policy (weapon/skill/recoil chest policy matches reference thresholds); it stays open. All source/frozen/ownership/death/icon/ordinary voice/praise/ports/architecture/performance contracts pass. OS suites serialized; no golden regeneration.

Separate PE/ELF x86 exact7exports PASS; manifest291entries/283C-C++ PASS. Final294source/build/owned inputs include the newly registered ownership JSON;2artifacts and4user files are fingerprinted. SHA256: DLL8a3c4b095b9c22b0b8fd64e0c07eb650dbe6c313e9055ec66b4cf6968e4f8073; SO1e288ea72bacf03fb14e02026fb840876036be995eeac79a83fcf4e02c0d0cbc. Native HEAD and used chatter/radio/idle files are clean at the above oracle pin; public Metamod-P7ec9b014f8c0a947a724644aebe34eb33706e44b. Evidence: .agent-state/sessions/csbot-team-chatter-8e4e8c08-f815-4e9d-8642-6aca3a713d48/ (STATE, builder/final review, original/control RED/GREEN, complete OS builds/suites, separate artifact/manifest and identity gates). Local commit/FF receipt is RESULT.json. Pre-fix whole builds were intentionally stopped by their verified owned process trees after review; only interrupted object timestamps were invalidated, then complete corrected builds supplied final artifacts.

Decisions retained: established shared checkout/cache/evidence with exact paths/fingerprints (mixed-evidence/disk risk); existing continuous implementation/local integration authorization used without a renewed menu (builder choices remain ours); represented-candidate selector is a bounded dependency, while generic admission capacity, exact full Update phase order, multi-part context/place/delay composition, plan conversion/memes and new event constructors remain open (missing native behavior is not waived); source reviewer does not certify OS/artifact/live gates, which are separately owned/evidenced (platform/runtime differences remain possible). No Minor findings deferred. Live HLDS/ReHLDS, client/audio and multiplayer/FPS acceptance remain unverified; no push/deployment/full-goal completion claim.

## 2026-11-15 death Radio attachment/UI

Source base062824e0f29d060c56a76d455c2c9648c539c5a0; oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp1168/1428–1448, player.h476, player.cpp Radio, weapons.cpp398 and game.cpp130. Public-intake audit: PUBLIC_EVENT_INTAKE_AUDIT.md.

- Native normal custom BotStatement passes showIcon=false; OnDeath pain uses Radio's true default. Only the qualified death interruption emits a head attachment immediately after accepted SendAudio to the same eligible/nonignored recipient. Existing GameDLL standard radio owns its presentation; no new BotVoice starts for pain.
- Exact public wire is MSG_ONE/SVC_TEMPENTITY23, BYTE TE_PLAYERATTACHMENT124, BYTE original sender, COORD35, SHORT host sprites/radio.spr model, SHORT15 (1.5seconds). Optional mp_show_radioicon=0 disables only the icon; absent vanilla cvar and nonzero values enable it. Missing coordinate/model API or invalid nonpositive/short-overflow model preserves audio. No asset precache, RNG or duration probe is added.
- Metadata callback replacements cancel stale output. Public recipient serial, connection generation and flags are checked after callback-capable eligibility work and SendAudio; a reused or same-serial reconnected recipient gets no old follow-up UI/feedback. A reset during an icon finishes the current packet and cancels later recipients.
- Registered-hook runtime tests cover exact ordered BYTE/SHORT/COORD/STRING fields, each icon paired with immediately preceding same-recipient pain audio, routing, ordinary omission/no metadata lookup, cvar/model/transport variants and callback round/recipient replacement. Frozen baselines are unchanged.

Independent final source review originally graded0Critical/0Important/1Minor and ready for production. Parent regraded the untyped recorder/order/recipient-pair gap as one Important verification gap because the plan promised exact wire acceptance. One test-fix pass adds the typed ordered recorder and pairing assertion. Deliberate SHORT→BYTE and COORD/SHORT-swap controls each fail the new payload assertion; exact correct production bytes were restored before final complete builds. No re-review and no deferred Minor finding. Earlier actual icon and recipient-reuse RED/GREEN evidence is retained separately.

Final offline gates: Windows x86 complete build and CTest119/119 PASS9.94s under VS18/MSVC14.51.36231 Hostx86/x86; Linux x86 complete build and CTest117/118 PASS12.49s. The sole failure is unchanged astrabot_combat_policy (weapon/skill/recoil chest policy matches reference thresholds); it remains open. The3death-interruption variants, ordinary voice and frozen contracts pass in the complete suites. OS suites were serialized; no golden regeneration.

PE and ELF x86 independently expose exactly the existing7symbols. Manifest288entries/281C-C++ PASS.291source/build inputs,2artifacts and4user files are held against final fingerprints. SHA256: DLLc519858887ebdbe0716ac206fb083827d3a38b3817c9eea997ef57961ffd878b; SO21d3c019774baddd782e35057fd1d8519a5ba80dbc68ef4afb93515f065e666b. Native HEAD/used files are clean at the above pin; public Metamod-P7ec9b014f8c0a947a724644aebe34eb33706e44b. Evidence: .agent-state/sessions/csbot-death-radio-icon-3d5c41c6-07ea-438e-93a9-d7596fb72fa1/ (STATE, single final review/regrade/resolution, actual icon/recipient/typed-wire RED/GREEN, complete OS builds/suites, separate PE/ELF/manifest and identity records). Local commit/FF receipt is session RESULT.json.

Decisions retained: established shared checkout/caches/user evidence with exact-path staging/fingerprints (mixed-evidence risk); exact injury remains unresolved while independently implementable native UI is added (delayed injury parity, no waiver); session evidence retained under project preservation rules (disk cost); no exhaustive impossibility or full-reference parity claim from the public-intake audit (further exact event/scheduler/composition work remains); source and offline/artifact checks remain distinct from live HLDS/ReHLDS, client icon rendering/lifetime, audio and multiplayer/FPS acceptance (runtime differences remain unobserved). No push/deployment/live or full-goal completion claim.

## 2026-11-15 death interruption audio

Source base2bd5e138341534224efa4d72902c349296b986e2; oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_chatter.cpp1428–1448, cs_bot_chatter.h577–602, cs_bot.cpp171–187 and public player Radio.

- Native pain is a speaking-at-death interruption, not an ordinary injury grunt. Qualified public broadcast/own targeted DeathMsg captures a retained speaking statement; queued-unsaid statements do not emit it. Already-dead senders and DeathMsg-before-deadflag are supported. Native speaking begins before voice/radio transport callbacks and remains until the next statement update removes it.
- NORMAL/MINIMAL use native case-insensitive first-character verbosity. Default catalog must define pain; profile bank supplies the ANY-place/undefined-count (CountMany) sample. Missing definitions/samples never produce a standard-radio fallback. The interruption uses existing teammate/dead-teammate/spectator/ignore routing, original sender/pitch, no duration probe or new BotVoice start, and ends old feedback. Silence resets for a defined pain even without a speakable.
- Death cancels ordinary pending/direct/delayed acknowledgements and dedicated topics while retaining cooldown/consumed-order history. Only its exact qualified team phrase/busy-until lock is released; callback replacement speech survives. A qualified frame authorizes audio separately from ordinary cancellation, which also releases old owned locks.
- Victim actor/entity/connection/frame/time/speech revision are captured before callback-capable navigation death work. Both NAV danger publication and subsequent equipment/public-observation/behavior resets reject replaced receipts; message payload/frame/recipient are pinned across reentrancy. Feedback is tracked before external starts, and stop lookups reject changed owner/revision/new feedback.
- Core and actual registered-hook tests cover pending/retained/finished speech, modes/catalog/asset absence, exact sender/pitch, dead and spectating recipients, repeated deaths, frame/round/clock/serial, in-flight audio death, callback life replacement and direct VIP Follow me GameDLL dispatch/public SendAudio consumption. Frozen baselines remain unchanged.

Independent final review:0Critical/4Important/0Minor. One combined RED→GREEN fix pass covers five actual failures: VIP None-phrase dispatch/confirmation, partial feedback start without stop, stale stop lookup suppressing fresh feedback, delayed acknowledgement resurrection, and NAV-callback life replacement erasing new speech. A real accepted Go command and actual public VIP role/safety-zone Escape decision serve as controls. Final focused gate4/4 PASS0.28s; complete OS/artifact gates follow below.

Final offline gates: Windows x86 full build and CTest119/119 PASS9.22s under VS18/MSVC14.51.36231 Hostx86/x86. Linux x86 full build and CTest117/118 PASS13.23s; only the unchanged combat_policy weapon/skill/recoil chest threshold failure remains open. New4tests, prior chatter and frozen/architecture/ports/ownership/performance contracts pass. OS suites ran sequentially; no golden regeneration.

PE and ELF x86 each expose exactly the existing7symbols. Manifest288entries/281C-C++ files PASS. Final291source/build inputs,2artifacts and4user files match saved identities. SHA256: DLLc35b35108291a89ba13584ffabfe63fd6e7825ef89c54afc80f8f7eced1d86cd; SOf90357dd2ff11ef007dde5887c94cdc61066f9c6e4a478f3245383ffa9fa2342. Native oracle HEAD and used files are clean at the above pin; public Metamod-P7ec9b014f8c0a947a724644aebe34eb33706e44b. Evidence: .agent-state/sessions/csbot-death-interrupt-84900dd2-91d1-4ba6-b045-998e4ca1958f/ (STATE, review, RED/GREEN, complete OS builds/suites, separate artifact and identity records). Local commit/FF receipt is session RESULT.json.

Decisions retained, in order: shared checkout/caches/user evidence retained with exact owned-path staging and fingerprints (mixed-evidence risk); next native pain path implemented while exact FriendlyFire/injury intake remains active (injury parity is deferred, not waived); session evidence retained under project preservation rules (disk cost); optional internal receipt frame separates legitimate death audio from ordinary cancellation, and an internal speech-revision getter guards callback-capable death work (internal coupling cost; public ABI unchanged); broader FriendlyFire/global scheduler/composition and native Radio attachment/UI effects remain in the active backlog (missing behavior remains); source/offline/artifact evidence remains distinct from live HLDS/ReHLDS, real audio and multiplayer/FPS acceptance (unobserved runtime differences remain). No Minor findings. No push/deployment/live acceptance or full-goal completion claim.

## 2026-11-15 friend-death chatter

Source basea146109; oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_event.cpp71–155, cs_bot_chatter.cpp2254–2262 and native Say/AddStatement/IsRedundant/playback.

- The native friend-death retaliation early return, including a rejected Attack attempt, suppresses speech. Otherwise a Career human teammate death requests CommanderDown before Hunt/reposition; a human friendly non-self killer requests KilledFriend. Neither requires victim visibility or proximity. Original observer/victim/killer receipt identities are revalidated across callbacks.
- CommanderDown uses REPORT_MY_INTENTION and20seconds from creation, sharing pending/current redundancy with NiceShot variants. KilledFriend uses its own topic, draws float(0.5,1) before verbosity/default-catalog/duplicate admission, and expires strictly after2seconds from creation. Both preserve start/insertion order, selected-only no-friends checks and teammate speaking redundancy.
- Default catalog owns existence/Important/radio metadata; profile banks only provide speakables. Fixed phrases select UNDEFINED_PLACE and CountMany even on named NAV. Missing unavailable speech releases following statements. Event admission does no profile/wave I/O.
- Actual voice and mapped GameDLL radio/public SendAudio confirmation retain qualified own currently-speaking admission. Monotonic reset revisions, captured owner/frame and strict callback completion checks reject stale completion without discarding fresh statements or publishing cancelled feedback/team busy state. Death/serial/connection/map/round/clock lifecycle resets are covered.
- Core and registered-hook runtime tests cover native branch/RNG/boundary matrices, two independently capable managed speakers, OFF/MINIMAL/profile-only definitions, pending/current shared intention, exact lifetimes, named NAV, callback reconnect and same-owner reset during voice/radio transport. Frozen inputs remain unchanged.

Independent final review:0Critical/3Important/0Minor. One combined RED→GREEN fix pass resolved unavailable CommanderDown starvation, named-NAV fixed-phrase criteria, and stale voice/radio completion. No second review was used. Broader native global statement ordering/composition and other reports remain in IMPLEMENTATION_BACKLOG.md; the overall user goal remains active.

Final offline verification: Windows x86 full build/CTest115/115 PASS10.29s under VS18/MSVC14.51 Hostx86/x86. Linux x86 full build/CTest113/114 PASS12.69s; only the unchanged combat_policy weapon/skill/recoil chest threshold failure remains open. Full-suite frozen characterization first exposed a dead/spectator update callback regression; the update receipt now allows stable dead/spectator maintenance while actual transport still requires a living receipt. The unchanged frozen test and new3tests passed4/4 after correction, then both complete directories/suites were refreshed. No baseline regeneration.

PE/ELF x86 exact7exports PASS independently. Manifest286entries/279C-C++ PASS;289source/build inputs,2artifacts and4user files match saved identities. Final SHA256: DLL40aff4ca2e840877286b438314f7a5a2f93f29f73c65fd8fe1932dfbac1f0135; SOcddcb78b70d790405da2ad1f54a8beeb91aa294b9804a02a9aa4d27adaf6d85e. Evidence: .agent-state/sessions/csbot-death-chatter-54c45e32-e249-4604-9e34-6f5d5042d654/ (STATE, final review, RED/GREEN, OS suites, separate PE/ELF and identity records).

Decisions retained: preserve the shared checkout/caches/evidence/user edits, using exact-path staging and forced affected-object rebuild plus fingerprints to control mixed-evidence risk. The broad global scheduler/composition audit remains separate, so bounded speech-path completion does not overstate full parity. Source/offline/binary evidence remains distinct from live HLDS/ReHLDS, real audio, multiplayer/FPS acceptance; live differences remain possible. No Minor review findings. Local commit/FF receipt is recorded in session RESULT.json; no push/deployment.

## 2026-11-15 friend-death behavioral reactions

Source base18a2cc4; oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_event.cpp71–155, cs_bot_statemachine.cpp293–367, cs_bot_update.cpp429–480 and native MoveTo/Attack/state disposition exits.

- Witnessing a teammate death without a victim FOV limit starts native Hurry10–15; inventory snipers are excluded, hiding knife ambush rolls strictly below50, and rejected reload/zombie/mimic Attack retains the early return. Otherwise Hunt reconsiders before the lost-sight attack branch, including world/friendly kills.
- A qualifying native Attack receipt captures the killer public origin once as an event exception. Hidden contacts stay absent and cannot grant firing. Acquired enemy LOS skips FOV while unknown contacts retain it; blind/smoke/public LOS still apply. Full and cached Aim use the retained point until actual visibility updates it.
- Lost sight strictly over0.4seconds while attacking starts native IgnoreEnemies1 and fixed-goal fastest-route MoveTo. Ignore permits actual visible-target fire while moving, cancels on native state exits (including same-state MoveTo and nested Follow/Idle), and expires after its inclusive deadline.
- Current combat enemy and pursuit task entity have separate serial/connection identities. Death/disconnect/reuse/map/round/owner/time rollback invalidate stale memory. Actual arrival/path failure and CT planted-bomb notice end pursuit; ordinary scenario planning cannot replace it. Hurry influences native walking decisions rather than forcing all commands to run.
- Registered DeathMsg/public vision/flash/smoke/CurWeapon/NAV/full-cached command tests exercise the complete branch matrix, fixed hidden point, moving fire, source reuse, disconnect, cached Aim and real path end/failure. Frozen baselines remain unchanged. Speech (KilledFriend/CommanderDown), broader general attack parity and live multi-player/FPS acceptance remain open.

Independent final review:0Critical/2Important/0Minor. Both Important findings were reproduced and fixed in one pass: receipt-time connection replacement across callbacks, and ordinary Guard/Use/Stop actions leaking into protected pursuit. The parent also reproduced/fixed native safe-time boundary handling (negative early sighting and no adjustment after the prior safe deadline). Existing ForceRun command coverage now explicitly releases Attack before isolating noise walking; native knife Attack retains pursuit after sight loss, so sight loss alone is not a noise-investigation handoff. Original speed/cached/expiry assertions remain.

Final offline verification: Windows x86 full build and CTest112/112 PASS8.64s under VS18/MSVC14.51 Hostx86/x86. Linux x86 full build and CTest110/111 PASS12.55s; only the unchanged combat_policy weapon/skill/recoil chest threshold failure remains open. New friend-death and existing gunshot/frozen/architecture contracts pass. PE and ELF x86 each expose exactly the existing7symbols. Manifest283entries/276C-C++ files passes;286source/build inputs,2artifacts and4user files match their recorded identities. No baseline regeneration, push, deployment or live acceptance.

Final artifacts (SHA-256): DLL7bf9a8b6f64676b37be03fccf7976af292f25019475ddbd6e88a18f04ee919ef; SO6bb69c272023c7853c760ef2c427fb4bd50a87c7ca111e247cc2da9e13a461de. Evidence: .agent-state/sessions/csbot-friend-death-5dbf62ae-a71b-498a-8bed-88b211bf4657/ (STATE, review, RED/GREEN, OS suites, PE/ELF and identity records).

## 2026-11-15 knife enemy fire run

Source base3452978; oracle ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_event.cpp267–273, cs_bot.h1328–1332, cs_bot.cpp67–77 and game_shared/bot/bot_util.h63–73.

- A knife-equipped managed actor reacts only to its previously recognized enemy's public firing/dry-fire receipt. Public edict serial retention prevents reused slots from triggering a former enemy reaction; no enemy position or visibility is invented. Direct dispatch precedes sound deduplication and hearing/NAV/noise admission. No new trace, asset I/O or RNG is added.
- Existing mapped server gun PlaybackEvent and exact standard fire/dryfire samples establish receipts. Private/delayed/unmapped playback and incidental/reload samples do not. The five-second timer restarts on new eligible events, remains active at the exact deadline, and survives loss of sight or switching away from the knife. Native walking requests yield to running during the timer; crouch, ladder, freeze, objective holds and ordinary goal slowdown retain their existing controls.
- Observer/serial/map/round/life/clock/frame checks and public death, actor removal/remember, execution reset and round reset cancel stale timers. Enemy death clears retained target identity without cancelling another living observer's already started run timer.
- Core policy and actual registered-hook runtime tests cover exact deadline, refresh, identity/life/clock, genuine enemy recognition, range/sight independence, knife/private/unknown/reload gates, source reuse, weapon switch, public death/round and actual full/cached command speed240 versus walk96, including expiry restoration. The inert engine fixture resets its unrelated stuck-progress history after a5.1second gap and respects the existing native path throttle.
- This is a bounded public receipt slice. Native reload/knife/grenade animation event intake and friend-death reactions remain open. No live HLDS/ReHLDS acceptance, deployment or push is claimed.

Independent final review:0Critical/2Important/0Minor. Fixed the cached-walking-start reaction and the extra recognition callback in one pass. The cached-start regression now advances40ms (beyond native30Hz command upkeep), asserts a new command with no full update, and detects the original early walking cap in a temporary RED control before restored GREEN. Frozen characterization also reproduced RED on extra callbacks and is now unchanged/GREEN. No control/debug code or regenerated golden input remains.

Final offline gates: Windows110/110 PASS9.77s; Linux108/109 PASS12.72s, only the unchanged combat_policy chest-threshold baseline fails. Complete directories were built under matching Windows Hostx86/x86 MSVC14.51.36231 and Linux x86; OS suites ran sequentially. PE/ELF32bit exact7exports PASS independently. Manifest279entries/272C-C++ files; source282input hashes and4preserved user files unchanged across final gates.

Artifact SHA256: DLL f3fc9dc35538abb4918fb845529de4388572840561ed13f50f0a7410548f734e; SO9295c1d7e64c122cdf9cd4d96db608cb16b7e5d461ecc90ba8dc04ba3654f7dc. Local commit/FF receipt lives in the feature session; no push/deploy.

Decisions retained: existing checkout/caches and evidence are preserved to avoid disturbing user files; exact-path staging and input/artifact hashes control the risk of mixing work. Native receipts beyond this slice stay in the backlog (missing behavior remains); live acceptance stays open (runtime differences remain possible). Full suite/artifact gates declined by the source reviewer were executed by the parent. The known Linux failure remains an open issue, not overall completion.

## 2026-11-15 kill congratulations

Source based1878dd; read-only behavior oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_event.cpp159–189,
cs_bot_vision.cpp229–251, basemonster.cpp630–649, player.cpp5959,
cs_bot_chatter.h612–621 and phrase/statement playback in cs_bot_chatter.cpp.
Public SDK pfnIsCareerMatch directly supplies the ordinary/Career distinction.

- Eligible public DeathMsg first executes the existing remaining-count draw.
  Only another friendly player's enemy kill, with <=1 recently watched living
  enemy and a currently visible victim origin, can request congratulations.
  Visibility uses current public blind/smoke receipts, native2D forward projection
  (including body pitch), strict0.5 cone and ignore-actors/glass LOS fraction1.
  Source reuse across the count RNG/visibility/admission callbacks is rejected.
  No target, Aim, firing or hidden contact is created.
- Shared named float(2,3) delay precedes the Bot killer float(0,100)<40 chance.
  Human killers choose NiceShotSir or NiceShotCommander from the public Career
  getter; an unavailable getter does not invent a human variant. Delay/chance
  precede mute, missing default definition and redundant-topic admission. The
  three variants share one pending REPORT_MY_INTENTION topic; later eligible
  kills draw but retain the earliest statement. Original event+3 expires strictly
  after its deadline, and no-friends speech context is evaluated only when selected.
- Default BotChatter.db alone defines phrase existence, Important and radio
  metadata. Profile banks supply speakables and cannot add a global definition
  or grant Important. Event admission does not load profile banks, probe wave
  files or shuffle variants; playback preparation retains the existing behavior.
  Fixed praise and remaining-count messages use UNDEFINED_PLACE and native
  UNDEFINED_COUNT0xFFFF (saturating to Count Many), never the speaker's NAV place
  or a wildcard matching arbitrary explicit counts. Other phrase behavior remains.
- Generic, remaining-count and praise topics maintain life/time/admission first,
  then arbitrate by pending start while preserving every unselected topic and
  older silent delays. Per-topic send/discard owns only the selected statement.
  Same-team praise makes all3 variants redundant even before their start.
  Actor/entity serial/map/round/clock/death/removal changes cancel; public death
  prevents revival before the next full update from replaying pending praise.
- Policy/catalog and registered-hook runtime tests cover all variants, exact RNG
  order/chance, strict lifetime/duplicates, source reuse, named NAV and conflicting
  count/place criteria, default-missing profile and conflicting Important flags,
  no event-time wave probes, hidden/rear/self/off/unreceived cases, public flash/
  smoke, queue coexistence, round/death and transport cancellation. Independent
  review found3 Important and1 Minor; corrected native criteria/default authority
  tests reproduced RED, then all7 related tests passed GREEN. All findings fixed;
  no deferred findings. MSVC's test-only getenv warning was fixed with its owned
  environment-copy API, without disabling warnings or changing production code.

Offline gates: complete MSVC14.51.36231 HostX86/x86 Debug build PASS; CTest
**109/109 PASS** (8.64 seconds). Complete Debian Linux x86 Debug build PASS; CTest
**107/108 PASS** (12.50 seconds); sole failure is the unchanged
astrabot_combat_policy weapon/skill/recoil chest-policy baseline. Its code/expectations
and frozen characterization inputs are unchanged; frozen characterization passes
without regeneration on both OSes. Manifest277 entries/270 C/C++, ports, ownership,
architecture/whitespace and independent PE/ELF x86/7exact exports pass. All280
tested inputs, verified artifacts and preserved user files/profile match after suites.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: c9efd45d23bfa76b089bb2f79892293ade0947f4148a9b47f21819035a1775ca
- Linux SO: 6ec93b9ff116cbce4bd3d52f851a735afe05ccadd88c14d4da1acbea3595290b

Evidence: .agent-state/sessions/csbot-kill-praise-ec407381-2fe6-4b6b-8926-827593e2710a/
(RESULT.json,fingerprints,corrected RED/GREEN/review and compiler/build logs,
sequential full CTests). Referenced GameDLL/SDK files are unchanged at their
pinned commits b0889847fe6d03898be88acc9e366660efb40ab5 and
7ec9b014f8c0a947a724644aebe34eb33706e44b.

Limits: full native global statement-list/phrase-parser composition and other death
reactions are still tracked in IMPLEMENTATION_BACKLOG.md; this does not close the
full user goal. Private blind/smoke/timing state is not recovered by public receipts.
Live HLDS/ReHLDS, client audibility/FPS/multiplayer acceptance are NOT RUN. No push/deploy.

## 2026-11-15 remaining-enemy death reports

Source base7c7de92; read-only behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5, cs_bot_event.cpp159–169 and
cs_bot_chatter.cpp1884–1893/context statement playback and redundancy.

- Valid public DeathMsg feeds alive managed recipients only. An enemy victim,
  friendly player killer and at most one recent watched living enemy admit the
  report without requiring victim sight. World/friendly-fire/unknown sources,
  unrelated targeted recipients and reused recipient generations are rejected.
  Source serial0 remains valid. No enemy target/position/Aim is manufactured.
- Shared named float(2,4) is drawn before AddStatement-equivalent OFF/MINIMAL and
  duplicate checks. This remaining-count context is non-important even if the
  host eventual count phrase is Important. Earliest redundant pending report
  remains; later valid deaths still draw. Cached three-second contact counts
  validate observer/map/round/clock identity; event-time scans do not add a new
  per-frame visibility scan.
- Original event time owns strict now>created+5 expiry. At the selected speech
  slot, current living opponents choose host NoEnemiesLeft/OneEnemyLeft/
  TwoEnemiesLeft/ThreeEnemiesLeft; outside0..3 or no living friends consumes the
  statement silently. Spectators/dormant/invalid/dead players are excluded.
  Older generic topics retain the slot, including silent delays; count context
  is not consumed while waiting behind them. Unselected generic topics remain.
- Any teammate currently speaking a count phrase makes the same topic redundant,
  even before our start. Actor/entity serial/map/round/death/removal/clock changes
  and current OFF/MINIMAL cancel it. Public death cancels immediately, preventing
  revival before the next Full Update from replaying it. Existing profile banks,
  SendAudio/BotVoice, explicit host radio mappings and normal receipt transport
  remain; unavailable voice without a mapping consumes rather than invents audio.
- Policy and registered-hook runtime regressions cover exact RNG/time ordering,
  strict lifetime, dynamic0..3, duplicates/teams, cached watch identity, targeted
  receipt reuse, serial0, modes, current living counts, hidden death/no firing,
  round/death cancellation and unavailable transport. Independent review found
  one Important premature-context-consumption bug, reproduced RED and fixed
  GREEN with an actual older generic scheduler topic and later dynamic count.
  No Critical/Minor or deferred findings.

Offline gates: complete MSVC14.51.36231 HostX86/x86 Debug build PASS; CTest
**106/106 PASS** (8.57 seconds). Complete Debian Linux x86 Debug build PASS; CTest
**104/105 PASS** (12.33 seconds), only the unchanged astrabot_combat_policy
weapon/skill/recoil chest-policy baseline remains. Combat code/expectations and
frozen characterization inputs are unchanged. Frozen runtime characterization
passes on both OSes without regeneration; source manifest274 entries/267 C/C++,
ownership/ports/architecture/whitespace and independent x86 PE/ELF7exports pass.
All277 tested source inputs, verified artifacts and preserved user files/profile
match after the suites.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 764a7e40045edffc92252eae02cd09847102a3816032f95ae58b30f1582ec851
- Linux SO: c46754f561c75048840446063318087de8ed54b8b0eea441a0a7643ff92376b5

Evidence: .agent-state/sessions/csbot-enemy-count-chatter-75575d0e-760e-4351-a7cc-354789b15e8e/
(RESULT.json,source/artifact/user fingerprints,RED/GREEN/review logs,compiler identity,
complete builds and sequential full CTests).

Limits: this implements the remaining-count statement path, not the full native
statement list/global arbitration or other death reactions. Existing bounded generic
scheduler and public-message timing remain; those broader implementation gaps are
tracked in [IMPLEMENTATION_BACKLOG.md](IMPLEMENTATION_BACKLOG.md). Live HLDS/ReHLDS,
client audibility/FPS/multiplayer acceptance are NOT RUN. No push/deploy. The user
goal remains active for the other implementation items; this entry does not close it.

## 2026-11-15 PM_Move footstep intake

Source base dc3f3ca; read-only behavior oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5, public HLSDK PM definitions and
ReHLDS public movement callback plumbing. Independently observes actual movement
sounds that bypass the normal EmitSound hook; does not infer steps from timers.

- Paired public PM_Move pre/post hooks temporarily wrap PM_PlaySound. A bounded
  eight-scope adapter stack supports same/different-move nesting, forwards original
  sound arguments exactly once and restores only callback fields still owned by us.
  Foreign saved-thunk chains resolve their outer callback without replaying a wrapper
  already in flight. Capture stays suspended during forwarding. No new exports.
- Only actual standard material steps enter existing Low/strict1100 footstep hearing,
  NAV localization, looking, investigation, reports and raw<300 sniper equipment.
  Live movement origin replaces stale edict origin for the observation without
  writing the edict. A scoped public EmitSound echo shares that position and dedups.
- Server/runfuncs, current slot range, alive nonspectator client identity/serial
  (including zero), map/round, finite monotonic clock and bounded coordinates are
  checked before intake. Rejected, STOP and non-step calls still reach original audio.
  Map/round/reset/overflow cancel capture and restore owned fields; logical scopes
  remain active until their paired posts. Active detach is refused before mutation.
- Tests pin exact float32 argument bits, nested forwarding/reset/overflow/foreign
  ownership/orphan thunks, actual registered hooks, stale edict/live movement origin,
  dual-feed dedup, invalid source/client/clock conditions, lifecycle/unload and actual
  near-footstep pistol command. Public SDK and ReHLDS x86 PM layouts independently
  compile to identical accessed-member offsets and sizes; product uses public fields.
  Independent review found1 Important and0 Critical/Minor; the foreign-chain audio
  loss reproduced RED and passed GREEN after the single fix pass. No deferred findings.

Offline gates: complete MSVC14.51.36231 HostX86/x86 Debug build PASS; CTest
**104/104 PASS** (10.82 seconds). Complete Debian Linux x86 Debug build PASS; CTest
**102/103 PASS** (12.80 seconds). Only the pre-existing astrabot_combat_policy
weapon/skill/recoil chest-policy failure remains, with its code/expectations unchanged
from [the baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Frozen runtime characterization passes without regeneration on both OSes. Ownership,
ports, architecture, manifest (270 entries,263 C/C++ files) and whitespace gates pass.
Independent PE/ELF gates confirm x86 and seven exact exports; all273 tested inputs,
verified artifacts and preserved user files/profile match after the final suites.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 1d1104039f149ba496e19273b14ba0bb187678fbb06f6b32210c8453dcc04cdb
- Linux SO: 23cd63b2f59d7c87d4f8bde0f365564d23594fd148e66a23a26e2b04dcdc549c

Evidence: .agent-state/sessions/csbot-pm-move-footsteps-4a9c034c-421a-45a5-bbc8-faf81c9299e4/
(source/artifact/profile fingerprints, SDK/engine ABI comparison, RED/GREEN/review
logs, complete builds and sequential CTests). ReHLDS public reference identity:
0124d56c3d888d922eb045775f71c6682ad1226f; public SDK Metamod-P identity:
7ec9b014f8c0a947a724644aebe34eb33706e44b. References remained read-only.

Rulings/limits: eight live scopes are supported; overflow/unmatched posts cancel
capture and restore owned fields while retaining logical scopes until correct unwind.
Orphan thunks cannot revive a callback after its scope ends. Foreign wrappers must
respect the public pre/post callback lifetime; callbacks retained beyond it have no
valid observation scope. The public PM sound bridge does not synthesize native
player-animation GameEvents or establish complete native scheduling equivalence.
Live HLDS/ReHLDS, client audibility/FPS/multiplayer acceptance are NOT RUN. No push/deploy.

## 2026-11-15 close-footstep sniper equipment

Source base3a7fd83; behavior-only oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5 (read-only), CCSBot::OnEvent,
EquipPistol/DoEquip and IsGameEventAudible. Independently implements the public
sound-driven close-footstep equipment slice in Compatibility mode.

- Known concrete/metal/dirt/duct/grate/tile/slosh/wade/ladder/snow step samples
  are classified as Footstep, Low priority with strict1100-unit hearing range.
  Invalid/custom prefixes are not presumed to be native footsteps. Existing STOP,
  volume and stale-source validation remains; serial0 is valid generation1, only
  negative serials are rejected. Per-event managed-handle iteration is prefiltered
  by the cached observed sniper; no extra per-frame entity scan was added.
- A living hostile player's public footstep at raw3D distance<300 can request a
  pistol for Scout, SG550, AWP or G3SG1. Own/friendly/unknown-team/dead sources do
  not trigger it. Receipt creation precedes noise attention/replacement and NAV
  randomization; neither a visible contact, NAV destination nor vacant noise memory
  is needed. No hidden target identity, Aim or fire is manufactured.
- Loaded or unknown sniper clips and low skill/no combat target do not suppress
  this reason. An owned allowed pistol still needs observed usable clip/reserve
  ammunition. Existing shared five-second equipment interval, normal action/command
  dispatch and CurWeapon acknowledgement remain in effect. USE/C4, grenade, reload,
  freeze, disabled AI and death retain existing priority/suppression.
- The receipt is moved into one transient normal-decision context and cleared on
  every exit, including failed ammo/priority gates. Actor/map/round/death/reset clear
  it, and ResetHUD clears it with equipment. New ammo cannot revive a previously
  rejected request. Duplicate guard/combat passes preserve an already-selected
  typed pistol proposal; no extra command transport or new engine ABI/export.
- Pure tests cover material samples, strict300, all4 sniper IDs, clip/skill/combat
  independence, ammo/allow/exclusions and receipt identity/age. Actual runtime tests
  pin hidden enemies, no-NAV Scout, positive and valid zero serials, stronger High
  noise retained under NAV, self/friendly/dead/stop/stale rejection, ammo-rejection
  replay, ResetHUD, real pistol command/CurWeapon and no fabricated firing.
  Independent review found2 Important and0 Critical; both were reproduced in runtime
  RED logs and fixed GREEN. No remaining Important/Critical or deferred minors.

Offline gates: complete MSVC HostX86/x86 Debug build PASS; CTest **102/102 PASS**
(8.59 seconds). Complete Debian Linux x86 Debug build PASS; CTest **100/101 PASS**
(12.41 seconds). Only pre-existing astrabot_combat_policy chest-policy failure remains,
with its code/expectations unchanged from [the baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Frozen runtime characterization passes on both OSes without regeneration. Ownership,
ports, architecture, source manifest (266 entries,259 C/C++ files) and whitespace gates
pass; independent PE/ELF checks confirm x86 and seven exact exports. All269 tested
source inputs and the user's BotProfile.db match before/after the final verification.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: bfa758798a49255965c0f07059c72b26216d326c40274d6565bf2dd02c91597f
- Linux SO: 03dd20261dd36c4d123649d0e7e0d7733e9fecc3d3bd9c596ed917e6111086c4

Evidence: .agent-state/sessions/csbot-close-footstep-equipment-490402ca-24a2-436f-b7a1-a0abcfcd9470/ (RESULT.json, source/profile
fingerprints, RED/GREEN/review logs, complete builds and sequential CTests).

Rulings/limits: this uses actual public EmitSound samples; PM_Move-only sounds and
native player-animation GameEvent generation remain outside this slice, so some
native footsteps may still never arrive. A0.25-second event-to-next-Full-Update window
bounds delayed equipment requests; a slower decision drops the request. Observation
never issues an immediate weapon command. These are adapter constraints rather than
complete native scheduling equivalence. Live HLDS/ReHLDS, client feel/FPS/multiplayer
acceptance are NOT RUN. No push/deployment.

## 2026-11-14 HeardNoise investigation reports

Source base efb307c; behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5 (read-only). Independently implements
HeardNoise for successful Compatibility-mode noise attendance using existing host voice transport.

- Attendance is already gated by well-past-safe investigation. Reports additionally
  require strictly more than30 seconds since the last public visible enemy and a
  priority other than High. Public observed round-over suppresses new reports.
  A named shared RNG float(0,100)<33 selects reports; the20-second CountdownTimer
  starts before that draw, including failed chance/unavailable RNG and chatter OFF.
  Eligibility resumes strictly after the cooldown deadline.
- Report creation seconds, actor/map/round identity and noise NAV place/revision are
  retained independently of speaker movement. The existing bounded outgoing scheduler
  preserves the original five-second lifetime (strictly now>expiry), team speech delay
  and host metadata fallback. HeardNoise uses the noise place for voice criteria;
  other concepts preserve their existing speaker-place criteria. Same-topic reports
  are redundant only for equal defined places or both undefined, not different places
  or one undefined place. No private enemy identity or extra per-frame entity scan.
- Normal/minimal/radio/off reuse existing recipients, host assets, pitch and BotVoice
  cleanup. OFF and unimportant MINIMAL consume each new report independently of any
  older pending topic, preventing later re-enabling from reviving it. No equivalent
  radio mapping or voice is invented when host assets are absent.
- Observed round-over is separate from generation preparation, so even the first
  Round_End after late attachment suppresses reports. Public death cancels queued
  noise immediately; active receipt validation also protects revival between outgoing
  updates. Actor/map/round/clock changes and NAV replacement reject stale reports.
  Same-identity clock reversal retains a discarded receipt identity through catch-up.
- Policy/scheduler tests pin gates, RNG ownership/bounds, cooldown ordering, strict
  expiry, places, cancellation and clock catch-up. Runtime tests attend real public
  medium noise through NAV and inspect noise-place SendAudio; gunshots remain silent,
  closer noise respects cooldown, OFF/on does not replay, and Round_End/Start suppress
  and resume. Independent review found3 Important and0 Critical; all3 were reproduced
  RED then fixed GREEN. No remaining Important/Critical or deferred production minors.

Offline verification: complete MSVC HostX86/x86 Debug build PASS; CTest **102/102 PASS**
(9.63 seconds). Complete Debian Linux x86 Debug build PASS; CTest **100/101 PASS**
(12.62 seconds). Sole failure remains astrabot_combat_policy, weapon/skill/recoil
chest policy, unchanged from [the baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Its source/expectations and frozen characterization fixtures are unchanged. Frozen runtime
characterization passes on both OSes without regeneration; ports, ownership, architecture,
source manifest (265 entries,258 C/C++ files) and whitespace gates pass. Independent PE/ELF
checks pass x86 and seven exact exports. All268 input fingerprints and BotProfile.db
match before/after the final suites.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: b4d58dd1ab8e902279fa338475eb1a316975bd54d6a646767f9d6d1f41468c65
- Linux SO: ba611fadf7f32a04f913ef898211ea8533dc49aa08647ae7521753921ca284ba

Evidence: .agent-state/sessions/csbot-heard-noise-report-a4f77913-6d15-4f01-a014-a79c110385b6/ (RESULT.json, source fingerprints,
RED/GREEN/review logs, complete build/compiler identities and sequential full CTests).

Ruling/limits: clock rollback between attendances is handled by the existing public
LifecycleSession frame observer advancing round generation, which resets this policy
on its next attendance; standalone callers must supply those generations. If they do
not, an earlier cooldown deadline can remain until the former20-second deadline.
Existing bounded scheduling and Full Update cadence are retained, not full native
statement-list or reference-wide RNG-tape equivalence. Late attachment cannot infer an
unobserved round end. Host voice assets are required; client audibility and live
HLDS/ReHLDS/FPS/multiplayer acceptance are NOT RUN. No push/deployment.

## 2026-11-14 NAV-constrained sound positions and investigation decisions

Source base bd5db00; behavior-only oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5 (read-only). This independently
implements the NAV localization and investigation slice for Compatibility mode.

- Only admitted non-objective sounds draw localization RNG. The highest XY-overlapping
  NAV floor at/below origin+5 and no more than120 below is selected; absent containment,
  public ground/ignore-glass LOS probes select the nearest eligible area within10000.
  Uniform XY error is400*heardDistance/2000; the result is clamped to the SAME area and
  its sloped floor. Memory retains area/revision and original receipt seconds without
  exposing a hidden actor identity. No NAV/ground/LOS/RNG result means no noise destination.
  Optional resolution leaves core fixtures independent of a live NAV adapter; C4 beeps
  retain their separate objective knowledge path.
- Investigation uses current receipt age against profile ReactionTime, rather than a
  previous Full Update's cached ready flag. Safe time is15+5*aggression; strict well-past
  safety is elapsed>1.25*safeTime, adjusted by early public enemy contact. Free-task,
  life/round, visible enemy, blind/freeze/configuration and disposition gates precede
  admission. Checks are limited to0.25-second intervals. More than120 units of vertical
  separation requests one bounded route check per accepted noise. Unreachable or travel
  distance>1500 rejects; center-to-center physical path length is kept separate from
  weighted route cost and replaces straight-line distance if longer. Hiding radius is
  100+400*aggression. Probability is1-distance/2000, minus min(.5,.05*livingFriends) when
  at least3 teammates remain, using the native inclusive RNG comparison.
- Attending consumes the expected noise memory and retains its own target. New noise
  redirects only when more than100 units closer. Actual objective routing passes SAFEST
  through to the corridor query and retry recursion. Knife/few-friend/hurry rules select
  walking; standing walk speed is capped at0.4*maxspeed through command construction,
  avoidance and command-cadence delivery. Path end/failure or a visible, smoke-free point
  within200 and strict20-degree pitch/yaw alignment finishes investigation. Objective,
  follow and guard ownership remain prior to free investigation.
- Active destination/posture exports validate actor, map, round, NAV revision, clock and
  life even during an attack overlay; unconditional lifetime validation precedes priority
  gating. Death, removal, round/map/NAV/clock changes clear held state. Objective-only
  sounds do not add teammate scans. Scans/traces/path queries are exercised through the
  existing ownership/service boundaries; no private GameDLL fields or offsets are used.
- Tests cover layered/sloped floors, visible nearest fallback, admission/RNG order,
  stale consumption, current-time reaction, safe/cadence/friend/hiding/height boundaries,
  physical travel versus weighted cost, retained/closer targets, SAFEST selection and
  actual transmitted walking speed. The runtime fixture traverses two connected NAV
  areas and covers consumption, later sound, close visibility completion, combat NAV
  replacement and death. Independent review's Important stale-destination finding was
  reproduced RED then fixed GREEN. Additional RED-to-GREEN checks found death posture,
  command speed overwrite, a missed route-type query argument, cached ready use, and
  unnecessary C4 friend scans. The latter was fixed against the unchanged frozen record.
  No remaining Critical/Important findings or deferred minors.

Offline gates: complete MSVC HostX86/x86 Debug build PASS; CTest **101/101 PASS**
(8.01 seconds). Complete Debian Linux x86 Debug build PASS; CTest **99/100 PASS**
(12.98 seconds). The only failure is the pre-existing astrabot_combat_policy
weapon/skill/recoil chest-policy case from
[the coding-conventions baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Its implementation/expectations and the frozen characterization inputs are unchanged.
Frozen runtime characterization passes on both OSes without regeneration. PE and ELF
independently verify x86 and seven exact exports; source manifest, ownership, ports and
whitespace gates pass. All266 recorded source inputs and the user's BotProfile.db stayed
unchanged across the final verification runs.

Post-merge verification at implementation commit 50be3f9: complete builds PASS;
Windows CTest 101/101 PASS (10.10 seconds), Linux CTest 99/100 PASS (12.28 seconds),
with the same pre-existing combat-policy failure. PE/ELF exact-export gates pass again;
all 266 source fingerprints and BotProfile.db still match. Logs use the post-merge-* prefix.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: d5e63d1cc379cb7a43f8e418935b143b3d5ab93f26ce4d3ef8e05cfee05fcb4e
- Linux SO: 4fc60ef0d68153af1fcc2bd3fab3613ec4ecd8a50366a22ba3bd5c3897a13d61

Evidence: .agent-state/sessions/csbot-noise-navigation-8eb6dca0-0d17-4b2f-a54c-24cc0b7bb7ef/ (RESULT.json, source fingerprints,
review/RED logs, complete builds, compiler identity and sequential full CTest logs).

Rulings/limits: the existing native0.4–0.6-second ComputePath throttle is preserved:
if attending during that interval produces no path, the noise is consumed and the
investigation ends, as in cs_bot_pathfind.cpp1608 and cs_bot_investigate_noise.cpp32–76.
Runtime acceptance fixtures separate observation publication from the later decision
rather than resetting that throttle. Disposition/hurry use available public/task state;
private hurry timers and additional HeardNoise chatter concepts are not synthesized.
Observations retain the existing Full Update publication cadence. This is not a claim
of complete native scheduling or Enhanced-mode hearing parity. Live HLDS/ReHLDS behavior,
camera comfort, FPS and multiplayer acceptance are NOT RUN. No push/deployment.

## 2026-11-14 native gunshot ingestion and seconds-based sound memory

Source base 6fc332d; read-only behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. Independently implemented
public-event intake and hearing-time slice; this supersedes the upstream
frame-clock/PlaybackEvent limitations recorded in the earlier auditory-look entry.

- A public PrecacheEvent post table maps 25 standard gun scripts to the final
  engine index (including override results). META_FUNCTIONS supplies this private
  provider without adding an eighth exported symbol. Public PlaybackEvent observes
  zero-delay server firing without changing playback or inventing attack commands.
  Unknown/non-gun scripts, client/host-only events, invalid/dead/non-player/reused
  invokers fail closed. Precache entries survive activation and rounds, then clear
  on map deactivation, detach and fresh attach.
- Native gun receipts are High priority: ordinary guns 2000 units, TMP1000,
  AWP99999; M4A1 bparam1 and USP bparam2 select silenced1000-unit range. Position
  comes from public invoker origin, rather than the native zero event origin.
  Per-observer strict-radius, self/team and source-generation filters feed the
  shared bounded ring. Dual EmitSound/PlaybackEvent receipts coalesce; incidental
  same-frame weapon audio cannot suppress a High shot or widen its known range.
- Perception receives global seconds and profile ReactionTime. Noise becomes ready
  at age >=reaction, remains recent for <3 seconds, and expires only after20 seconds.
  Recent replacement requires equal/higher priority AND strictly closer distance
  measured from the current observer. Receipt time is retained across delayed
  publication; stale/future receipts and clock rollback cannot renew memory.
  Tick IDs remain frame identities/diagnostics. Clock-less offline inputs retain
  their legacy tick policy. Explicit audibility range permits valid AWP endpoints
  more than32768 units apart; diagnostic tick age saturates independently.
- New tests cover20/100/500-FPS equivalent timing and3/20-second boundaries,
  moving observers, actual pre/post callbacks, override selection, silencer range,
  AWP beyond2000/32768, duplicates, teams, serial reuse, map resets and preactivation
  precache preservation. Independent review found host-only events audible to enemies
  and an explicit-AWP validation cap; parent also reproduced invalid serial receipts.
  All three regression assertions failed RED before their fixes and pass GREEN.
  Linux exposed a C++14 constexpr-reference linkage issue, fixed with a value argument.
  No remaining Critical/Important findings or deferred minors.

Offline evidence: complete MSVC HostX86/x86 Debug build PASS, CTest **98/98 PASS**
(9.69 seconds). Complete Debian Linux x86 Debug build PASS, CTest **96/97 PASS**
(11.47 seconds). The only failure is the pre-existing astrabot_combat_policy
weapon/skill/recoil chest-policy case recorded in
[the coding-conventions baseline](../verification/coding-conventions-20261113/VERIFICATION.md).
Combat-policy source/expectations are unchanged. Frozen runtime characterization
passes unchanged on both OSes; it was not regenerated from the candidate. Source
manifest, ownership, ports and whitespace gates pass. PE and ELF separately PASS
x86 and seven exact exports. All258 recorded source inputs and root BotProfile.db
remain unchanged across verification.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 333e4b75a8d06db9d9991df794c93c61962c147f709126158049ba94e9d4299c
- Linux SO: bc5f47057a7c221957c0ec2056d88c65d0094f915e8c36abd605e48acbb5383e

Evidence: .agent-state/sessions/csbot-gunshot-hearing-time-bde84cb0-bce1-442d-8782-5bc804c44bdf/ (RESULT.json, tested-source-identity.json,
review.md, RED logs, compiler identity, complete builds and sequential OS CTest logs).
Limits/rulings: NAV-constrained noise randomization and exact investigate policy
remain existing differences; PM_Move footsteps and custom delayed playback intake
are separate work. Late attach without prior public precache receipts does not
infer unknown indices. Live HLDS/ReHLDS gunshot/camera/FPS acceptance is NOT RUN;
offline and artifact gates do not establish live acceptance. No push/deployment.

## 2026-11-14 auditory look reactions for the public sound feed

Source base f326cad; behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5, read-only. This is an independent
implementation of the auditory-look slice, not full native hearing parity.

- AudibleEvent and NoiseMemory retain explicit public receipt seconds separately
  from frame IDs. Fresh generic sounds require profile reaction delay and age
  <=0.5 seconds. A clear, nearby (<1000 units) High-priority sound without a
  visible enemy starts a 0.25-second look and random 2-4-second inhibition.
- Ordinary looks use 1-2-second holds; stationary Hide uses 2-5 seconds, or
  5-15 with one enemy remaining. Five-degree alignment starts the hold timer;
  ordinary targets within 100 units clear. Shared scripted RNG requests pin the
  bounds. The existing non-attacking spring runs at command cadence, with world
  movement reprojection and no synthesized firing or enemy identity.
- Direct auditory rays ignore monsters/glass and use neither FOV nor smoke as
  hearing occlusion. Blocked rays use the nearest read-only NAV approach and
  the existing guard bend routine, now shared without changing its geometry.
  Bend results are cached by received time/position, NAV revision and own pose
  within 10 units. Absent approaches conservatively skip the look.
- Visible enemy/attack, USE/C4, grenade, freeze/configuration, ladder and
  breakable ownership gate the modifier. Actor/map/round/death/clock changes
  invalidate held state. Ordinary guard scanning respects auditory inhibition.
- Known standard shot samples establish High priority in the EmitSound feed.
  SND_STOP creates no receipt. Independent review found same-frame Medium
  weapon audio could deduplicate away a later High shot; its actual callback
  regression failed RED, then passed after priority was included in the key.
  No Critical findings or deferred minors.

Offline gates: complete Windows HostX86/x86 Debug build PASS; CTest **96/96 PASS**
(9.28 seconds). Complete Debian Linux x86 Debug build PASS; CTest **94/95 PASS**
(11.52 seconds). Only pre-existing astrabot_combat_policy fails with the exact
weapon/skill/recoil chest-policy message in the coding-conventions-20261113
baseline; its source and expectations are unchanged. New pure and actual
EmitSound-to-RunPlayerMove regressions pass on both OSes, including reaction,
command smoothing, movement/USE ownership, no fire, stop notification, frozen/
stale/visible-enemy/beep rejection, positive NAV approach/cache reuse, death and
round reset. Pure tests also exercise actor-generation and clock reversal.
Shared guard regressions and unchanged frozen characterizations pass. Source,
ownership, ports and whitespace checks pass. PE/ELF independently verify x86
and seven exact public Metamod exports. Tested source/artifact hashes stayed fixed.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: e51346b5db1f1d9cbee617a6efd4d1a21ea67c748b12dc22c6c6196ca3da3172
- Linux SO: 8b2bd125d113901fc0080cb13876fc00046fa46cbf6a6e93bff819bccc1ce872

Receipts: .agent-state/sessions/csbot-noise-look-9bf0ec08-83d5-4573-beb8-7ea7b5e57783/.
Native camera comfort/FPS/real-server acceptance is NOT RUN. No deployment/push.
Limits: client PlaybackEvent-only gunshots are not supplied by the existing
EmitSound adapter. Upstream hearing range/position and frame-based replacement/
expiry remain the previous inferred pipeline; explicit seconds fix look freshness,
not the whole hearing system. Dynamic approach data is inferred from compiled NAV.

## 2026-11-14 CSBot chatter voice transport for existing concepts

Source base aa6abd5; pinned read-only behavioral oracle ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. Independent implementation; host
BotChatter.db and audio are read through public Engine APIs, never distributed.

- The eight existing outgoing concepts can play host voice samples in normal
  and important-only minimal modes. Radio mode uses the existing GameDLL menu
  path; off suppresses chatter. A voice-only concept no longer requires a
  standard-radio mapping. Missing voice/transport falls back only to the
  default bank's explicitly declared radio equivalent.
- Profile VoiceBank now inherits through defaults/templates with VoicePitch.
  The bounded parser handles BaseDir, Count (Many=4), Place, Radio, Important
  and End. Shared cyclic cursors and per-round shuffling avoid repeated variants.
  Current read-only NAV place and public visual enemy count filter selections.
  Missing samples are removed once on bank initialization when GetFileSize is
  available. Database loads, absent-bank results and duration lookups are cached.
- Public SendAudio sends speaker byte, sample string and profile pitch short to
  same-team clients, including dead teammates, plus public observer targets
  matching that team. Audio is never broadcast to enemies. Observed recipient-only
  TextMsg ignore/accept-radio receipts gate recipient audio by map/entity serial.
- Optional registered BotVoice feedback starts per recipient and broadcasts its
  stop after sample duration+1 second, on death, removal, round or map cleanup.
  A zero-active fast exit keeps ordinary frames free of voice polling callbacks;
  active feedback expires even while bot_stop is set. Team speech occupancy uses
  audio duration+0.1 second; GetApproxWavePlayLen supplies milliseconds, with the
  reference 1-second duration fallback. No per-frame file loads/world scans.
- Independent review found two Important issues: feedback with all audio ignored
  was not tracked, and VIP direct FollowMe could be replaced by queued chatter.
  Both were reproduced RED then fixed GREEN. Direct radio retains precedence;
  all-ignored feedback still expires. No Critical findings or deferred minors.

Offline gates: full MSVC HostX86/x86 Debug build PASS; Windows CTest **94/94 PASS**
(9.00 seconds). Full Debian WSL Linux x86 Debug build PASS; CTest **92/93 PASS**
(11.54 seconds). Only pre-existing astrabot_combat_policy fails with the same
weapon/skill/recoil chest-policy message as the coding-conventions-20261113
baseline. Combat-policy sources/expectations are unchanged. New catalog/profile/
scheduler/actual-message regressions pass on both OSes; frozen characterization
baselines remain unchanged and match. Source/ownership/ports manifests and
whitespace checks pass. PE and ELF independently verify x86 and seven exact
Metamod exports. The installed host DB parsed all eight supported concepts.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: b68dc2a620aa24a61365cb779acb5dad7d0aeb84157190f639fd3d471266624d
- Linux SO: 8139ec131aecce219ff9ffeb4e29dc66a89d73833044d298307c5d98d185b764

Receipts: .agent-state/sessions/csbot-chatter-voice-61b98bd2-eed8-4863-8e6b-0faf7101e201/.
Native client audibility/UI acceptance is NOT RUN; no deployment or remote push.
Other CSBot statement generation/composition remains open. Engine-RNG shuffle
is a portable approximation of reference CRT random_shuffle consumption, not
reference replay equivalence. Unobserved private ignore-radio state on late
attachment cannot be recovered; public observer fields may be incomplete.

## 2026-11-14 CSBot contact-driven primary weapon exchange

Obstacle work was committed as 30cd290 and fast-forward merged into master.
This follow-on starts from 30cd290; pinned behavioral reference remains
ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5, read-only.

- Grounded weaponbox-first Touch exchanges an existing primary only when the
  dropped primary comes first in the profile's ordered preferences and the
  last visible enemy was at least 2.5 seconds ago. Pistol preferences do not
  affect the choice. No distant weapon scan or weapon-seeking route is added.
- Standard world models infer ordinary primary contents; public entvars.weapons
  identifies the carried primary. Unknown/custom/shield models and ambiguous
  inventories skip exchange. Empty primary slots retain ordinary GameDLL pickup.
- A synchronous public drop command names the current primary classname before
  the original GameDLL Touch continues, including while a pistol is active.
  Inventory and pickup confirmation remain GameDLL-owned. No private layouts.
- VIP public ScoreAttrib, actor ownership/join state, map lifecycle, death,
  spectator/freeze and configuration gate the request. Enemy visibility history
  resets with a new actor/round. Player-first contacts exit before engine calls.

Offline verification: Windows x86 Debug complete build PASS; CTest **92/92 PASS**
(7.79 seconds). Debian Linux x86 Debug complete build PASS; CTest **90/91 PASS**
(11.11 seconds), with only the pre-existing astrabot_combat_policy threshold
failure. Its exact message matches the coding-conventions-20261113 baseline;
combat-policy sources and expectations are unchanged. The new weapon pickup
fixture and frozen runtime characterizations pass on both OSes. An initial
characterization delta consisted entirely of 48 extra classname queries on
player-as-box contacts; the early FL_CLIENT rejection removed those calls.
Historical baselines were not regenerated. Independent review and follow-up
found no Critical/Important findings. Source manifest and whitespace checks pass.
PE/ELF independently verify x86 and seven exact public Metamod exports.

Verified local artifact SHA-256 (not deployed):
- Windows DLL: 7986efa38846eadfb5f72eaf82c198e077e0fe006c4555f406b27959289df2df
- Linux SO: 438631d9bf4b51d4c0b99c0769699b45af6e78be35d37e4bf9da68319df0d0ff

Receipts: .agent-state/sessions/csbot-weapon-pickup-bbdb03c5-3039-48d9-a33a-b1e549a82baa/.
Native HLDS/ReHLDS pickup/physics acceptance is still open; no server deployment
or remote push was performed. Public model inference does not establish exact
private weaponbox contents on modified servers.

## 2026-11-14 CSBot contact-driven obstacle interaction

Source base: AstraBot bee8026; behavioral reference ReGameDLL-CS
b0889847fe6d03898be88acc9e366660efb40ab5. This entry records implementation
and offline evidence only; native door/breakable map acceptance is open.

- Ordinary func_door / func_door_rotating movement remains touch-open, as
  assumed by the reference game_shared/bot/nav.h. The pinned bot has no
  remote-button search routine; no invented USE or attack is sent to doors.
- Public Touch detects damageable func_breakable outside enemy attack. A
  local-path box-side rejection preserves the conservative BotTouch contract,
  using the local movement goal raised 36 units. A current controller path
  query prevents a destroyed corridor's old diagnostic goal rejecting contacts.
- Actor-local center look runs with the non-attacking spring at the command
  boundary, five-degree alignment and 0.2-second hold. Contact requests primary
  input without constructing an enemy or stopping movement; world direction
  is reprojected when the view turns. Pending contact survives a look expiry
  between Touch and the next scheduled command.
- Active grenades select an allowed carried primary, pistol, then knife, with
  a five-second equip-request throttle surviving look expiry. Publicly known
  fully empty weapons are skipped. Unknown ammunition remains unknown.
- Public KeyValue captures material and entity serial, excluding material 7
  unbreakable glass and trigger-only/nondamageable brushes. Missing material
  provenance is conservatively skipped (including late plugin attachment).
  No private CBreakable fields, pdata, offsets or per-frame entity scans.
- Death/removal, entity reuse, round/map changes and freeze invalidate or gate
  contact commands. Existing player avoidance, USE/C4 interaction ownership,
  compatibility mode isolation and public Metamod exports are preserved.

Verification receipts: .agent-state/sessions/csbot-obstacles-e16ecfdf-ed93-458e-93fc-1f72d0d61e43/.
Windows baseline 90/90 passed. The new adapter test first failed for absent
KeyValue support, then passed; expanded regressions exposed and verified
look-expiry/equip-throttle and known-empty-primary fixes. Tests cover reverse
Touch order, unbreakable/trigger-only/unknown/reused entities, door command
preservation, local-path geometry, world movement direction, grenade selection,
freeze, enemy-attack precedence and round invalidation. Source review found two P2 issues (ammo selection
and stale path ownership), both corrected; follow-up has no Important/Critical
findings. Source manifest check passes. PE and ELF independently pass x86 and
seven exact Metamod exports. Final Windows x86 Debug full build and CTest: **91/91 PASS** (7.41 seconds).
Final Debian Linux x86 Debug full build: PASS; CTest: **89/90 PASS**, only
pre-existing astrabot_combat_policy threshold test fails with
"weapon/skill/recoil chest policy matches reference thresholds". The same failure
is recorded in docs/verification/coding-conventions-20261113/VERIFICATION.md and
its baseline-linux-combat-policy.log; the combat policy files are unchanged.
The new obstacle regression passes on both OSes. Ownership inventory was
registered and both suites rerun. No tested source changed after fingerprinting.

Limits: no new native obstacle-map run or deployment was performed. Material
provenance and unknown ammo are public-information limits, not full CSBot parity.
General-purpose button solving and arbitrary func_pushable destruction are not
part of the pinned BotTouch behavior.

## 2026-11-12 A-side ramp / explicit path endpoint repair

MC11: source correction, RED/GREEN regression, both x86 builds and backed-up
deployment verified. Native: original1251->481 endpoint completion not encountered: native exact-case remains UNVERIFIED.
General endpoint-motion/END records: 8 saved examples. Other long
stops remain; do not declare full CSBot parity or general stall elimination.

### Source and BSP findings

- Saved bot1 game582.304..589.517 stopped7.213s near(1352.8,2348.1,29.5).
  NAV1251->481 is an uphill crouch strip:481 x1350..1375,y2350..2400,
  north floor11.53222, south24.03226, center(1362.5,2375,17.78225).
  Old last-area arrival accepted rectangle distance<=20 and floor distance<=64
  even before crossing y2350; retiring that corridor while retaining its Hunt
  goal caused repeated starts, endpoint stops and .4..0.6s retry gaps.
- Fixed reference CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES:
  UpdatePathMovement closeEpsilon20 compares full3D path-end distance <20.
  Explicit requested positions survive path generation and have NAV ground Z;
  Hunt's area destination uses its center projected to NAV GetZ. Path completion
  owns goal retirement. The historical two-corner center-height approximation
  differs from the four-corner floor by up to13.31203 units at actual area867;
  explicit Hunt endpoints now use surfaceZAt too.
- Actual `de_dust2.bsp` v30 SHA256 `15945389528d113562ede0a2c80647ebfa799079ed1c05a25379bcf84e4e9286`,
  9582planes/8321clipnodes/43models. Read SDK bspfile.h layouts, world hull1
  (standing32x32x72), hull3(duck32x32x36) and nearby brush model40.
  At(1359.5,2349.7), duck-origin floor29.8577 vs standing-origin47.3203;
  recorded duck origin29.9 is consistent with NAV feet11.9. A rising floor,
  not a justification for a zero-movement arrival at the boundary. Model40 is
  func_breakable tgt_a at x1280..1344/y2354..2432/z16..94. Its compiled hulls
  were sampled too. Live destroyed/enabled state, moving entities and players
  require existing public engine traces; offline BSP cannot assert those states.
  The tested straight duck-hull segment toward the endpoint clears world hull3
  but intersects the compiled box40 hull at fraction0.0022168; sliding/stepping
  and actual box state therefore matter. This is not a claim that the straight
  segment is free of obstacles or that offline clip-hulls simulate player motion.
  BSP/NAV files unchanged, no generation or map patch.
- YaPB4967a22, RealBot2f9d5c7, PODBot75f61e1, SyPB4c364fb were inspected for
  public origin/mins, trace and crouch/collision handling. YaPB directly sets
  desired jump velocity in some branches; that behavior was not transplanted.
  PODBot handles duck-height trace origins; SyPB traces hulls for obstacle
  avoidance; RealBot uses public directional TraceLine. Provenance/hashes in
  reference-sources.json; RealBot working bytes differ, so pinned excerpt was
  read separately. AI decisions remain based on CSBot.

### Contract and regression coverage

NavCorridor appends hasEndpoint/endpoint, default false for legacy/Enhanced.
Compatibility supplies true goal position or Hunt center, both with NAV surface Z.
Alternative corridors retain the endpoint. Follower keeps intermediate portal
steering, but the final segment aims at the endpoint and uses strict3D <20.
An endpoint completion retires the goal even if the actor is just outside the
NAV polygon. Finite endpoint validation and restart lifetime are explicit.
No public Metamod export changes, new private offsets, velocity writes or
GameDLL-specific hooks were required. Vanilla public-observation path retained.

RED: saved1251/481 slope/BSP duck-feet case stopped before its endpoint; second
RED reproduced failure to retire a narrow goal when already within endpoint
range. GREEN: moving/crouched intent, strict20 boundary, vertical25 vs area64,
inside-large-area continued movement, endpoint retirement, invalid endpoint,
legacy/Enhanced and existing movement/C4 suites pass. Source-derived expected
values are not captured CSBot runtime traces. A further nonplanar-area test
first confirms route start, then requires completion at interpolated floor21
rather than historical center42. It fails without the Hunt Z projection and
passes with it. Initial fixture debugging and RED/GREEN logs are retained.
No runtime speed increase masks
the stop.

Windows69/69 pass, PE x86/7 exact exports. Linux68 pass+1 external-NAV
performance skip, ELF x86/7 exact exports. DLL
`e37378893e9d35580dddaa88e43d8aca01c8e6d813fcec0a2ff28bbeee8c182d`;
backup `astrabot_mm.dll.bak-20261112-110014`. Source/build/deploy/launch identities
match. Vanilla GameDLL native and Linux native remain unverified (offline
Vanilla/provider-absence tests pass; optional API supplements zero fields).

### Native8 / acceptance limits

8BOT/0humans de_dust2, normal45 381.6s and restarted10
263.3s. Events {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 3, 'Target_Bombed': 6}.
HLDS stopped; server.cfg byte-identical restored; transient secret state removed.
Plant jump 0/333 samples.

| Observation | Saved run | Current run |
|---|---:|---:|
| Game seconds | 664.355 | 645.985 |
| Target-region >=3s stationary maximum | 7.213s | 0s |
| All transit >=3s stationary episodes | 11 | 31 |
| All transit >=7s stationary episodes | 1 | 3 |
| 1251->481 link samples | original case | 0 |
| True endpoint481 selections | original case | 0 |
| Original1251->481 completed cases | original case | 0 |
| General endpoint contract evidence | old area tolerance | 8 examples |

Zero target-region stops without target exposure is not an exact-case pass.
First new-path full-update records identify true goals; continuation diagnostic
goal_area may name an intermediate area. General endpoint evidence requires
>=3 moving full updates inside final area with horizontal endpoint distance>20,
then END within20 and >24 horizontal displacement in the same actor/map/round.
It does not replace the original481 native case.

- bot6 round2 path1 endpoint area40: moving inside area at game12.555 (line3726), END at12.92 (line3790), XY distance18.971, horizontal displacement92.481.
- bot6 round2 path8 endpoint area6: moving inside area at game54.198 (line13530), END at54.981 (line13693), XY distance16.193, horizontal displacement193.253.
- bot1 round3 path39 endpoint area54: moving inside area at game146.499 (line35846), END at147.125 (line36001), XY distance1.803, horizontal displacement151.183.

Broader native residual in exploratory iteration1 (separately archived):
bot7 game226.37..268.284 at(-236.9,1536.3,68),
41.914s Idle/neutral, UnsafeDrop on1773->139 then failed retries. This was
observed in that run; causality versus different RNG/profile/route exposure is
not established. Do not claim it was fixed or lower overall worst stop. It is
not a public-information limitation. Other friend/escape waits also remain.
Final-run longest transit records: [{"bot": "7", "seconds": 9.261, "times": [348.392, 357.653], "lines": [86252, 88374], "start": "(-660.5 -800.0 164.0)", "end": "(-680.7 -800.0 164.0)", "states": {"STATE-HUNT": 74}, "outcomes": {"1": 74}, "neutral_commands": 64, "all_commands": 74, "goals": ["(187.5 -187.5 8.0)", "(-250.0 -762.5 62.5)"]}, {"bot": "6", "seconds": 9.106, "times": [348.547, 357.653], "lines": [86311, 88372], "start": "(-621.4 -782.9 164.0)", "end": "(-642.2 -783.5 164.0)", "states": {"STATE-HUNT": 73}, "outcomes": {"1": 73}, "neutral_commands": 69, "all_commands": 73, "goals": ["(-950.0 -762.5 128.0)", "(-1975.0 1675.0 32.0)"]}].
Additional BSP check of the final-run9.261s Hunt stop at(-660.5,-800,164)
through(-680.7,-800,164): world standing-hull floor origin164 matches feet128,
inside NAV41 x-875..-500/y-900..-650 and its125..129 floor. Thus diagnostic
current_area=0 during neutral commands must not be treated as proof that the
actor physically left NAV. Route was41->42; recovery cause remains unresolved.
See residual-bsp.json and residual-nav-probe.log. Compiled floor geometry alone
does not establish current brush/player collisions or why movement was suppressed.

Limits: comparisons use different RNG/profiles and exposure. Stationary metric
is ~10Hz, anchor24, >=3s across transit states; excludes all Hide/Buy/Plant/
Defuse (including moving Hide), freeze/dead/no-nav/intentional holds. Command
intent alone is not progress. FPS measured in comparison.json; concurrent host
work and route exposure prevent a controlled performance conclusion. Graph
index datedNov8 lacks current relationships; current source/scoped diff and
executed gates used instead. FocalSpan refreshed independently.

Evidence: `build-action-adapter-x86-1451/aside-stall-evidence/` baseline617,
before/, bsp-geometry.json, nav-probe.log, reference-sources.json, RED/GREEN,
source manifest, verification/deployment/scoped patches/final-gate.
`aside-stall-live-evidence/` comparison.json, aside-comparison.json,
motion-analysis.json, qconsole.log, launch/shutdown/status receipts and
secret-free helper copies. Existing unrelated edits/rootDLL/evidence preserved;
no staging/commit/push.
The pre-height-projection DLL/SO, logs and ~21-minute exploratory run are kept
in aside-stall-iteration1-evidence/ and aside-stall-iteration1-live-evidence/.
Iteration1 captured population only at start; the final run requires both
start and end status. Neither run substitutes for a missing original-case
arrival exposure.

## 2026-11-12 B-site progress-timeout exclusion repair

MC10: source fix / regression / both x86 builds / backed-up Windows deployment
verified. Native assessment: B-approach timeout followed by >24-unit physical progress observed; see retry records.
No complete-stall-elimination or full-CSBot-parity claim.

### Cause and reference contract

- Pinned CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES enabled:
  `cs_bot_pathfind.cpp` UpdatePathMovement's giveUpDuration5 runs/stands,
  DestroyPath and PATH_FAILURE. MoveTo returns Idle after task checks;
  ComputePath throttles 0.4..0.6s and rebuilds without excluding that old edge.
  Those reference files were compared against the pinned commit (no diff).
- Saved bot7 game152.165..166.854 near(-1361.3,2447,37.9) stopped14.689s.
  Actual de_dust2 NAV query157->643 finds157->823->643 (3 expansions),
  but excluding157->823 returnsNoRoute. Old expireGroundRoute excluded that
  edge after5s; subsequent MoveTo/Idle retries could only search the graph
  with its sole approach excluded, entering the existing failure backoff.
- Fix in `NavRoamController::updateInternal` expireGroundRoute: timeout drops
  the corridor and clears that transient exclusion. Retain physical-failure
  exclusions, unsafe-drop/HP/fall validation, air/ladder continuation,
  freeze, persistent stuck monitor, retry RNG and Enhanced behavior.
  No public Engine/Metamod export or private-data/offset dependency added.
- RED `testBsiteProgressFailureDoesNotBlacklistTheOnlyApproach`: actual-NAV
  reduced sloped jump fixture157/823/643, progress timeout then sole-route
  retry failed on old code. GREEN emits157->823->643 movement on next update.
  Generic timeout's old permanent-exclusion expectation corrected. Existing
  unsafe drop/bypass, airborne/late-arrival and RNG suites pass. Expectations
  are source-derived, not a trace captured from an executing CSBot.
- `profile navigationStep` appends failure_reason/current_area/goal_area/
  path_link. Destroyed-friend next-update feedback can have zero path metadata
  and MovementNotProduced: analysis explicitly separates it from a timeout.
  New command intent alone is not accepted as physical progress.

### Build, deployment and native evidence

Windows69/69 passed, PE x86/7 exports; Linux68 passed+1 external-NAV performance
skip, ELF x86/7 exports. Source/build/deployment/launch identity verified.
DLL SHA256 `ea47c34c608d0e5f9af374f19f89f47dcd4bb5c8f7843fa06422c416ca8583a0`.
Backup `astrabot_mm.dll.bak-20261112-003114`. Previous sources, evidence and root DLL
preserved; scoped changes only, no stage/commit/push.

de_dust2, 8BOT/0humans: normal45 398.6s; restarted10
264.9s. Events: {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 1, 'Target_Bombed': 8}.
HLDS stopped, original server.cfg restored byte-for-byte, temporary secret state
removed. Plant jump: 0/331 samples.

| Observation | Saved pre-fix run | This run |
|---|---:|---:|
| Game duration | 645.346s | 664.355s |
| B-region maximum >=3s stationary episode | 14.689s | 5.227s |
| All transit stationary episodes>=3s | 5 | 11 |
| All transit stationary episodes>=10s | 1 | 0 |
| 157->823 samples in this run | unavailable at full-update granularity | 576 |
| Original goal643 approach samples | saved original case | 0 |
| B-approach timeout recoveries with >24 physical progress | original failure | 1 |

Exposure actors: 1, 2, 3, 4, 5, 6, 7, 8; rounds: 2, 3, 4, 5, 6, 10, 11.
- bot6 round6 game372.505: STATE-HIDE path65 -> path66, (157->823) -> (157->823), command after0.139s, horizontal>24 after0.882s, max horizontal5s=90.48. Original goal643 differs from this goal424.
Do not promote friend-repath recoveries to proof of this timeout fix. Command
and displacement records are in `bsite-comparison.json` (line/time scoped).

Limits: different RNG/profiles/combat exposure, observational run comparison.
Same-team Hunt agreement is also retained in comparison.json; not full-path
identity. Physical stops use ~10Hz live dispatched navigation, 24-unit anchor,
>=3s across transit states; freeze/dead/no-navigation and objective holds excluded.
The stationary summary excludes all Hide/Buy/Plant/Defuse states (including
moving Hide approaches), so it is not an all-behavior maximum. Explicit timeout
recovery records also inspect moving Hide states and require horizontal>24,
not just a vertical jump. Broader stationary totals are not an overall fix claim.
Windows ReHLDS native only; Linux/Vanilla native unverified. Exact goal643
condition is unverified unless its recorded exposure and recovery say otherwise.
Shorter B-side friend waits, A-side retreat waits and prior ledge1734->96 launch
validation mismatch remain separate residuals; no unimplemented case is labelled
an unavailable public observation. Graph review index datedNov8 lacks current
flow/test relationships; current source, scoped baseline diff and executed
build/test/native evidence were used instead. FocalSpan updated separately.

Evidence: `build-action-adapter-x86-1451/bsite-path-evidence/` baseline617,
before/, RED/GREEN, actual-NAV probe, source manifest, verification, deployment,
scoped patches and final-gate.json. `bsite-path-live-evidence/` comparison.json,
bsite-comparison.json, motion-analysis.json, qconsole.log, launch/shutdown/status
receipts and secret-free helper copies. Previous hunt-route evidence retained.

## 2026-11-11 Hunt destination selection follow-up

CSBot source mismatch fixed and regression/build/deployment verified. Native
route concentration decreased in this run; general stall improvement is not
established. Long stops remain open, not public-information limitations.

- Reference: `b0889847fe6d03898be88acc9e366660efb40ab5`, FIXES enabled;
  `HuntState::OnUpdate` unconditionally draws `RANDOM_LONG(0, areaCount-1)`
  and its final traversal overwrites the oldest-area candidate. Compatibility
  now does so even with eligible large areas. Enhanced reservations unchanged.
- RED: new eight-actor/shared-history/large-area test failed on old source.
  GREEN: actor-scoped typed/ranged/ordered tape consumed, SAFEST route retained,
  corridor continuation does not redraw; failure/reselection tapes explicit;
  equal draws can still share a route. Old oldest-area expected result corrected.
- Windows x86: 69/69 passed, PE/7 exports passed. Linux x86: 68 passed,
  1 external-NAV performance skip, ELF/7 exports passed. Windows DLL SHA256
  `b78eb76173a7d7a79361a52a69a3d32c3846da19001b460819bcb0f343c25e29`.
  Backup `astrabot_mm.dll.bak-20261111-223639`; build/deploy/launch hashes match.
- Native de_dust2: 8 BOT / 0 humans, normal45 382.4s,
  restarted short10 269.5s. Final HLDS stopped,
  original server.cfg restored byte-for-byte. Events: {'Round_Start': 10, 'Planted_The_Bomb': 10, 'Bomb_Defused': 2, 'Target_Bombed': 7}.

| Same-team Hunt pair agreement | Previous saved run | This run |
|---|---:|---:|
| Same destination | 13.70% (421/3074) | 3.20% (96/2998) |
| Same next link | 11.55% | 3.04% |
| Same current NAV area | 11.03% | 3.57% |
| Observed game duration | 665.538s | 645.346s |
| Transit stationary episodes >=3s | 17 | 5 |
| Episodes >=3s per10 game minutes | 15.33 | 4.65 |
| Transit stationary episodes >=10s | 3 | 1 |
| Maximum stationary episode | 13.005s | 14.689s |

Concentration: 1Hz samples grouped by same game-second/team/round, >=2 Hunt
bots; not necessarily all four teammates. Next-link agreement is not full-path
identity. Different RNG/profiles/combat exposure and duration: observational
comparison, not controlled CSBot execution or causal FPS/stall proof. Physical
stops use ~10Hz dispatched/live navigation, 24-unit anchor, >=3s across states;
exclude freeze, dead/no-navigation and objective holds. Counters alone do not
prove cause. Source-derived tests are not captured CSBot traces. Linux/Vanilla
native execution remains unverified.

Residuals: B-side bot7 at game152.165..166.854 stopped14.689s across
MoveTo/Idle near (-1361.3,2447.0,37.9); current area157, objective643,
PathSearchFailed/neutral commands repeatedly. Record as unresolved; no claim
that random destination selection repairs this objective-path failure. Earlier
ledge1734->96 launch mismatch and original A14->1214 cases remain open/unverified.
Plant jump commands: 0 of 330 sampled
Plant commands; nonattack waits>=1s: 0.

Evidence: `build-action-adapter-x86-1451/hunt-route-evidence/` (baseline,
RED/GREEN, source manifest, verification, deployment, scoped patches),
`hunt-route-live-evidence/comparison.json`, `concentration.json`,
`motion-analysis.json`, `qconsole.log`, launch/shutdown/status receipts and
secret-free helper copies. Previous evidence preserved. No commit/push/staging.

# CSBot Parity Status

## 2026-11-11 movement/C4 detailed audit: current revision

This section supersedes earlier November11 claims about the legacy64/96-unit
interaction gates and delayed navigation-failure delivery. Reference remains
b0889847fe6d03898be88acc9e366660efb40ab5 with REGAMEDLL_FIXES.
See [MOVEMENT_C4_AUDIT.md](MOVEMENT_C4_AUDIT.md) for MC01–MC08 contracts,
inputs/lifetimes/RNG, source-derived expectations, safety rulings and coverage.

Implemented: actor/map/round/goal/path/update-tagged NavigationStep and a
same-update state completion pass; current path-point friend checks; five-sample
physical monitoring across route changes; reference directional movement and
public maxspeed; Idle/Hunt30s lifecycle; known-position feet-distance<100 defuse
after path completion/failure; time-bounded BarTime receipts and1s reevaluation;
state-owned objective holds/plant5s timeout; escape search/draw deduplication.
Compatibility Plant directly owns C4 selection/attack inside a public bombsite,
independent of the old corridor destination. Enhanced retains its old paths.
The legacy CurWeapon cache also retains an active weapon when a different
inactive inventory entry follows it; own deactivation and reset still apply.

The wide-trigger plant regression failed before its fix and passes afterward.
Other RED/GREEN contracts are documented in the audit; no executed CSBot trace
was captured. Same-input source-derived tests are not a reference-runtime replay.
Windows x86 complete build:69/69 passed; PE x86/seven exact
exports passed. Linux x86 complete build:68 passed/one external-NAV performance
case skipped,69 registered; ELF x86/seven exact exports passed.

DLL SHA256:464a82e5dc44e8ffa280100ab4bf59af1bfa1bc8881c7101efceac6c715bb813
Linux SO SHA256:da7d8252d6969840c84fa11fb944d88738d7a49ae449ed506db0930aeb8b5c11
205-file source manifest SHA256:84787c999402bb6dbff8aee1c0c9066be5b77ef0bcdc289bb9edb0a7a8bf106d
Build/deployed/launch identity verified; backup:astrabot_mm.dll.bak-20261111-195030.
615-file pre-task baseline retained; unrelated changes are rejected by the
preservation check. Existing dirty/untracked files and root DLL are preserved.

Final8-bot/zero-human de_dust2 Compatibility run:
2026-11-11T19:50:36.501943+09:00 through 2026-11-11T20:01:45.495128+09:00.
Normal fuse45:389.6 wall seconds;
fuse10 after restart command:276.3 wall seconds
(one-second restart delay separately recorded). Teams:{'1': 4, '2': 4}.
Events:{'Round_End': 11, 'Round_Start': 10, 'Planted_The_Bomb': 10, 'Target_Bombed': 8, 'Bomb_Defused': 2}. Profile FPS:{'windows': 655, 'median': 68.49, 'minimum': 36.37}.
This includes diagnostic/indexing load and changing RNG/profiles, so it is not
a controlled FPS or timing A/B. HLDS stopped; original server.cfg restored and
hash-verified. No commit/push or unrelated server changes.

Full-update command samples:41192;
plant jump:0;
plant attack without crouch:0.
Plant nonattack waits of at least1s:[].
A-region transit exposure:{'STATE-MOVE-TO': 11, 'STATE-ESCAPE-BOMB': 6}.
Original A-region MoveTo14→1214 link samples:0.
Regional Escape/MoveTo exposure does not establish the original friend/goal setup.
Longest transit stationary episode (24-unit anchor, across state changes):
Bot3, 13.005s, game[610.217, 623.222], (-638.5 -881.2 164.0) to (-660.3 -874.9 164.0); states{'STATE-HUNT': 103}, neutral97/103 commands. See motion-analysis.json for goals and line ranges.
Defuse exit receipts:{'0': 26, '3': 1} (0=Unknown,1=Self,2=Other,3=None).
Unknown is not treated as observed absence. A receipt gap can cause bounded
retry; public BarTime capture is implemented, but hidden defuser/reload state
and rejected GameDLL action reasons are not inferred as exact observations.

| Judgment | Evidence / remaining work |
|---|---|
| 修正・検証済み | Listed offline contracts, both builds/export checks, native planting posture and state/command delivery; actual sampled displacement is archived |
| 未実装 / 未解決 | Final Hunt13.005s neutral wait; earlier area1734→96 Drop launch rejects feet11.55 above NAV floor (outer tolerance64 vs launch8), causing38.372s Idle/neutral loop. Safe recovery and dedicated regressions are pending; the whole equivalence goal is not closed |
| 実機未検証 | Exact original A-site7/13/26-second causal setup unless fully reproduced; Linux and Vanilla native play; executed CSBot trace comparison |
| 公開情報の制約 | Precise private reload timer and global internal defuser are unavailable; delivered-reload lease and explicit Unknown/1s reevaluation are documented substitutes |

Detailed machine evidence:
`build-action-adapter-x86-1451/movement-c4-audit-live-evidence/comparison.json`,
`motion-analysis.json`, `motion-rows.json`, `wait-metrics.json`, launch/shutdown
and RCON receipts; build/source/deployment manifests under
`movement-c4-audit-evidence/`. Aborted iterations1/2 and their deployed DLLs are
retained. Hide guard holds and dead/no-navigation heartbeats are not transit
stalls; freeze deadlines are excluded. Command intent alone is not success.
Remaining long stops and missed native cases prevent any full-parity claim.

The preceding20-minute iteration3 is also retained under
`movement-c4-audit-live-iteration3-evidence/`:18 plants,14 explosions,4 defuses,
no original A-region14→1214 samples, residual38.372s ledge stop and4.74s Escape
stop. Its later five-second C4 selection holds led to the CurWeapon cache fix.
These known geometry defects remain open even if a shorter final run does not
reproduce them. This is not a public-information limit.

## 2026-11-11 friend-wait and MoveTo failure repair: latest native evidence

Reference: b0889847fe6d03898be88acc9e366660efb40ab5. This supersedes the
earlier lateral-only make-way interpretation: REFERENCE.md freezes a
REGAMEDLL_FIXES build. Its StrafeAwayFromPosition replaces forward/back
as well as lateral movement at the 0.5 projection cutoffs. Astra now
dispatches both axes, retaining unaffected inputs and the existing
ground/freeze/action safety gates. Priority and the 0.33-second contact
lease are unchanged. No extra route reservation or new wait timer added.

MoveToState::OnUpdate exits to Idle when path movement stops progressing.
Astra formerly omitted actor-local navigation failures from the state
input and used Hunt as its generic exit. NoRoute, failed replan and
patience-driven path invalidation now deliver one-shot feedback scoped
to actor/map/round/goal generation. MoveTo abandons its path and cached
goal on the Idle exit; Attack can defer consumption. Dead/slot/map/round
reset discards feedback. No shared bomb-approach RNG is reset.

The intermediate 8-bot trial exposed a 21.969-second escape wait near A:
CTs still in MoveTo issued legacy defuse actions at a wider radius and
stopped before the scenario owned the interaction. The observation bridge
also labelled currently visible C4 as Remembered, preventing DefuseBomb
entry. It now carries current visibility independently of cached knowledge;
Compatibility dispatches defuse only from DefuseBomb. The existing
scenario interaction bound remains 64 units; CSBot's 100-unit feet-based
approach and its internal one-second defuser confirmation are not newly
claimed as fully reproduced by this scoped correction.

Regression cases fail on the corresponding old implementations and pass
after repair: front/back make-way on neutral input; unknown-ground veto;
actor-local navigation failure -> MoveTo/Idle; occluded remembered C4
does not issue premature IN_USE; visible C4 enters DefuseBomb. Existing
human-defuser handoff passes with a valid public bomb-map/entity fixture.
Complete Windows x86 Debug build:69/69 CTest passed, PE x86/seven exact
exports passed. Complete Linux x86 build:68 passed, external-NAV performance
case skipped (69 registered), ELF x86/seven exact exports passed. WSL is
available again; the previous section's environment failure is historical.
Linux and Vanilla native play were not exercised by this run.

Latest deployed DLL SHA256:d0873f1a6489d78cec5186d657a683f5d897dbe12754d26c683813ab3ba2f4ee.
Backup:astrabot_mm.dll.bak-20261111-173537. Source/build/deployed identity verified
against the 204-file source manifest; unrelated baseline files preserved.

Final hlds_start.bat run:2026-11-11T17:35:43.785389+09:00 to
2026-11-11T17:46:52.972473+09:00 (669.2 wall seconds), de_dust2,
Compatibility,8 BOTs/zero humans confirmed through RCON; team counts
{'2': 4, '1': 4}. Normal45-second rounds and any explicitly recorded
short-fuse stress phase are distinct conditions, with restart after fuse
changes. Native RNG/profiles differ from the prior run: this is not a
deterministic timing A/B. Events:{'Round_End': 10, 'Round_Start': 10, 'Planted_The_Bomb': 9, 'Target_Bombed': 6, 'Bomb_Defused': 3, 'Dropped_The_Bomb': 1, 'Got_The_Bomb': 1}.

About1Hz sampled positions,24-unit stationary anchor, excluding freeze,
action holds, plant/defuse and reached camp holds: longest MoveTo
stationary episode:7.081 seconds; longest EscapeBomb:
4.979 seconds. These are observed maxima, not
guaranteed upper bounds. 174 escape samples; navigation
results:{'0': 170, '3': 1, '2': 2, '10': 1}. 1823 explicit actor
MoveTo->Idle navigation failure exits. Touch make-way:163
applied samples,including93 backward and
66 forward;17 safety vetoes.
Plant input samples:56,IN_JUMP:0.
Diagnostic profile FPS:{'windows': 648, 'median': 59.92, 'minimum': 15.02} (instrumented Windows server only).

Original A-site MoveTo wait:26.074 seconds,link14->1214. Latest active
A-region MoveTo confinement maximum:3.026 seconds,including the same
link. The former long interval did not recur. Escape had only
1 sampled suppression episode(s),each represented by
[0.0] seconds between samples. The exact
original A-region escape box was not revisited in EscapeBomb during this
run; this limits reproduction coverage. The4.979-second EscapeBomb
physical confinement elsewhere had nonzero movement intents throughout:
it is a remaining geometry/traffic issue,not proof that movement succeeded.

Any residual stops, exact A-transit exposure and following movement are
listed in comparison.json,analysis.json and wait-metrics.json. Do not
equate state-local maxima with the absence of aggregate traffic delays.
Largest aggregate stationary episode (may span state transitions):
bot4,13.004 seconds at (1178.2 2337.1 151.6),game time[291.026, 304.03],NAV results{'2': 7, '0': 7}. This remains a recorded limitation, not a repaired case.
No complete CSBot parity or zero remaining movement defects declared.
HLDS stopped after evidence capture; original server.cfg restored byte
for byte. DLL remains deployed. No commit/push or root DLL replacement.

Evidence:build-action-adapter-x86-1451/friend-wait-nov11-evidence/
and friend-wait-nov11-live-evidence/; intermediate trials retained in
friend-wait-nov11-live-iteration1-evidence/ and iteration2-evidence/.
FocalSpan refreshed after edits. Code-review graph impact/review queries
were consulted, but its November8 snapshot lacks these new symbols/flows;
current source, compiler, behavioral tests and native logs provide the
actual verification. Graph test-link absence is not a runtime verdict.

## 2026-11-11 ledge and bomb-escape repair: deployed, 8-bot native improvement verified

Compared against CSBot reference b0889847fe6d03898be88acc9e366660efb40ab5.
Four connected causes were repaired using public Engine/NAV inputs:

- IsFriendInTheWay (cs_bot_pathfind.cpp:1097+) uses player entity origins
  against floor-level path points. Astra had used feet for both players,
  falsely blocking short portals with friends beyond the endpoint. The
  adapter now supplies current public origins; locomotion remains in feet.
  NavRoamObservation explicitly marks the available friend-ray origin and
  validates finite values. Geometry-only callers retain their prior input.
- CSBot retains its path/last-known area across NAV gaps. A safe descent
  near an adjacent polygon connected to the landing no longer reverses into
  geometry-only off-mesh recovery. Public safe-drop bounds remain required.
- Off-mesh recovery previously exhausted four geometry candidates and
  latched neutral until external displacement or lifecycle reset. Timed
  Compatibility now performs the existing randomized Wiggle/jump sequence
  on no progress and retries bounded candidates. Freeze/action suppression
  and Enhanced's bounded recovery behavior remain intact. This adapts the
  reference StuckCheck/Wiggle policy; it is not a claim of identical timers.
- EscapeFromBombState::OnUpdate (cs_bot_escape_from_bomb.cpp:40+) redraws a
  destination when path movement stops progressing. Cached escape areas
  are now invalidated after failure, arrival, or patience-driven path
  destruction, while ordinary polite waiting preserves the current draw.

Regression evidence: saved old-controller source fails the new off-mesh
Wiggle and adjacent-ledged-descent cases; current source passes. Public
Engine integration also reproduced the short-portal false wait before the
origin correction and tests escape retry after failure/patience expiry.
The Touch fixture now checks actual RunPlayerMove side inputs: its frame%16
diagnostic sampling did not reliably intersect actor update cadence.
Complete Windows x86 Debug build:69/69 CTest passed; PE x86/seven exact exports
passed. Linux rebuild/CTest/ELF checks remain ENVIRONMENT-UNVERIFIED:
two WSL launch attempts failed with Wsl/Service/0x80072747. Previous Linux
artifacts are not evidence for this source revision. No Linux redeployment.

Deployed DLL SHA256: e981c4398623b3f40dce6f79e43550963a35f742e0b554762e171ce8e01982a5.
Backup:astrabot_mm.dll.bak-20261111-162325. Source/DLL/deployed hashes match.
Final hlds_start.bat run:2026-11-11T16:23:32.272379+09:00 to 2026-11-11T16:33:52.906209+09:00,
620.6 wall seconds, de_dust2, Compatibility, eight BOTs/zero
humans (repeated RCON status),4T/4CT (TeamInfo). Intermediate trials were
preserved separately and were not mixed into this final run.

Test phases:mp_c4timer10 at16:23:38 JST, then45 at16:28:27 with sv_restart1
at16:28:34. The first planted bomb retained the GameDLL's cached45-second
fuse despite the changed public CVar; subsequent short-fuse rounds exercised
escape, followed by fresh ordinary45-second rounds. These deliberate stress
conditions and differing native RNG/profiles preclude an exact timing A/B.

Final log: 8 plants,
3 defuses, 5 explosions;
42 sampled plant inputs, zero IN_JUMP contamination.
Escape:499 sampled updates, result counts
{'0': 480, '10': 10, '3': 7, '2': 1, '1': 1}; 83 explicit escape retry
records. Failed escape samples resumed successful movement after observed
intervals [0.977] seconds. The previous off-mesh NoRoute latch did not
recur. However, the final run still contains a5.983-second escape wait
(bot8,542.064-548.047s, nav_result10/0) and a26.074-second MoveTo wait
(bot3,544.009-570.083s, nav_result10/0) near the A site. Both are classified
as movement suppression/polite waiting rather than persistent NoRoute;
the aggregate traffic delay remains open. Do not interpret this repair as
zero remaining stops or complete CSBot parity. Freeze, action holds,
plant/defuse and reached camp positions were excluded by the analyzer.
Sampling is about1Hz, not an every-command guarantee.

Non-carriers actually traversed the wall region. Maximum sampled confinement
was6.975 seconds while moving through it;
the earlier28.184/25.247-second looping confinement did not recur. This is
region confinement, not fully stationary duration, and does not establish
that all narrow passages on all maps are resolved. Profiling-on windows:
median59.23 FPS, minimum35.39;
uncontrolled diagnostic measurements, not a performance improvement claim.

HLDS stopped; server.cfg restored byte-for-byte (original SHA256
07997dbfc1f40913fabe021f097e548a0caa9bad722f17e052ce2108f0baaf23). No commit/push.
Evidence:build-action-adapter-x86-1451/ledge-escape-nov11-evidence/ contains
baseline/source manifests, scoped patches, red/green regression logs,
build/CTest/PE logs and deployment receipts. Sibling
ledge-escape-nov11-live-evidence/ contains qconsole.log, analysis.json,
comparison.json, RCON snapshots and shutdown.json. Earlier trial logs are
in ledge-escape-nov11-live-iteration1-evidence/ and -iteration2-evidence/.
Graph coverage of C++ flows remained incomplete; current-source inspection,
executable tests and native logs provide the acceptance evidence. FocalSpan
was refreshed; its index is not a substitute for native acceptance.

## 2026-11-11 area1735 C4 route repair: deployed, native recovery observed

The earlier 39.015-second C4-carrier stop had a reproducible false-NoRoute
cause in NavQuery::buildAlternativeCorridor. Independent prefix/suffix
searches preferred a suffix returning to the original start through the
failed edge, rejected that result, and never searched the longer valid bypass.
On the actual de_dust2 NAV, excluding1735->96 while routing1735->1469 returned
NoRoute after five subsearches/455 expansions before this repair. The repaired
query finds1735->1736->1737->1740->1741->1742->1093->1453->1452->96 and onward
to1469 with one search/260 expansions, at aggression0.0/0.5/1.0, health100.

CSBot reference b0889847fe6d03898be88acc9e366660efb40ab5,
CCSBot::ComputePath (cs_bot_pathfind.cpp:1608+, NavAreaBuildPath at1650),
searches start-to-goal as a whole. Astra now likewise performs a whole A*
search for this recovery, excluding the failed directed edge during expansion.
The exclusion remains Astra's recovery policy; this does not assert that
CSBot exposes the same exclusion contract. Existing costs, fall constraints,
search limits, normal-query behavior and public interfaces remain intact.
Only src/core/nav/nav_query.cpp and tests/nav_query_tests.cpp changed in code.
Regression coverage exercises the previously rejected longer bypass, single
search accounting, subsequent ordinary routing, and ResourceLimit propagation.

Complete Windows x86 Debug build:69/69 CTest passed; PE x86/seven exact exports
passed. Linux x86 Debug:68 passed, one external-NAV performance test skipped;
ELF x86/seven exact exports passed. Native evidence below is Windows ReHLDS,
not a Vanilla/Linux native acceptance claim. Graph queries lacked complete
C++ flow coverage; targeted current-source review and executable tests were
used. FocalSpan was refreshed after the source repair.

Deployed at15:25:12 JST with backup astrabot_mm.dll.bak-20261111-152512.
Built/deployed SHA256:
6a8f4f5ed902766f48cfc9252f0bf567ab55ba44e6d7887c15b31f4a285c9f16.
Executed hlds_start.bat, de_dust2, compatibility mode,15:25:19–15:32:31 JST
(about7m12s). RCON confirmed eight BOT participants/zero humans; TeamInfo
confirmed4T/4CT. Five rounds completed:five plants, three defuses, two explosions.
The following round was partial. All39 sampled plantInput records had
buttons=1, zero IN_JUMP. Profiling-on one-second windows:median94.32 FPS,
minimum49.52; diagnostic measurements, not a controlled performance comparison.

Crucially, C4 carrier bot3/Ulysses reached area1735 targeting1469 at85.012s
(log line11262), with NoRoute/path_sequence0. At86.012s (line11361) it had
recovered path_sequence12, nav_result0, speed240 and moved to area42.
It reached1469 at110.039s and planted successfully (line14235,15:27:13).
The long stationary failure did not recur in this run. The one-second sample
interval bounds this observed recovery; it is not an exact per-command delay
or a deterministic A/B timing ratio. Actual-NAV reproduction independently
verifies the repaired1735->96 exclusion case.

Remaining separate issues:non-carriers bot1/bot4 spent28.184s/25.247s moving
within the surrounding ledge region; bomb-escape NoRoute stops of6.919s,
5.984s and5.983s remain. Thus the earlier area1735 false-NoRoute repair is
verified, while complete narrow-ledge/escape navigation remains open.
HLDS was stopped and the original server.cfg restored byte-for-byte.
Evidence:build-action-adapter-x86-1451/c4-1735-nov11-evidence/ (reproduction,
source patches, build/export/deployment receipts) and sibling
c4-1735-nov11-live-evidence/ (qconsole.log, analysis.json,
carrier-1735-recovery.json, RCON snapshots, shutdown.json).

## 2026-11-11 autonomous 8-bot native verification: partial pass

Executed the user's hlds_start.bat on Windows, de_dust2, compatibility mode,
DLL SHA256 d71e198a845e79a2aa0746986f8adc5482c7256f91778447446afd1e682b73c0.
Run: 15:03:02–15:09:54 JST, about 6m51s. RCON status repeatedly confirmed
eight BOT participants and no humans; TeamInfo diagnostics confirmed 4T/4CT.
Metamod reported AstraBot RUN. Five rounds reached a natural result: five
plants, four CT defuses and one bomb explosion. The following round was
observed partially before controlled shutdown.

Planting acceptance: all 28 sampled plantInput records had buttons=1; zero
IN_JUMP contamination. This is native evidence for no recurrence in the
recorded planting attempts, not an every-command trace guarantee.

Make-way behavior is active in native execution: 91 playerAvoidance records,
87 applied and four ground-safety vetoes. This verifies execution of the
contact mechanism, not complete traffic-deadlock resolution. Profiling-on
one-second frame windows had median 63.03 FPS and minimum 44.15 FPS; these
are reference measurements under diagnostic logging, not a performance SLA.

Narrow-ledge acceptance FAILED. C4 carrier bot4 remained near
(-382.3,-637.8,163.5), current area1735, for at least 39.015 seconds
(game time364.074–403.089, archived log lines44304–48414), with repeated
nav_result=2(NoRoute), failure_reason=5(PathSearchFailed), path_sequence=0,
and no movement intent. The initial rejected segment was1735->96; the goal
was bombsite route area1469. It was still unresolved when the run was stopped.
This is distinct from the earlier wait-loop/portal-target reproduction and
must remain an open navigation repair. Shorter1736->1735 stops, a14.359s
guard-route stop and a17.067s bomb-escape route failure also remain recorded.
Intentional freeze, action holds/defusing, planting and reached camp positions
were excluded from the stationary-episode detector. Region confinement is
reported separately from being fully stationary.

Evidence: build-action-adapter-x86-1451/live8-nov11-evidence/ contains
qconsole.log(12,825,990 bytes; 48,455 lines), analysis.final.json, RCON snapshots,
launch/shutdown receipts and the analysis script. Log SHA256:
b4225f2885db59139c1af1d4ea7a8ecc6b6a43f24b0459432b4670ab45c8bc16.
The server was quit normally. Temporary quota8/join-after-player0 settings were
replaced with the original server.cfg bytes, verified by SHA256; the deployed
DLL was unchanged. No source implementation or NAV edits were made in this
verification. This run does not close all stuck/parity acceptance items.

## 2026-11-08 friend deadlock / same-link replanning / thin ledges

The Nov8 native log session (19:31:39–19:37:39 JST, lines45838–91585,
de_dust2, up to12 alive bots) confirmed planting-input improvement but retained
friend deadlock: bot8 stayed at(-99,-583,36) for48.956 seconds while path
sequences14..25 repeatedly chose1453->8; bots4/10/12 queued nearby. A C4
carrier on14->89 and a teammate on89->14 also waited in opposing directions.
Snapshot SHA256: c9cc896bf19251a6f460dd57e91995d018255b978c72afce4ebf0ca0ba431dd4.

Reference comparison found three missing/different mechanisms:

- cs_bot_pathfind.cpp:1493–1515 destroys a path on patience expiry but still
  moves toward the saved goal that update. Astra previously returned before
  producing movement. It now emits the normal safety-checked intent and
  invalidates the route afterwards. Occupancy remains a finite cost: a sole
  valid corridor is not banned merely because friends occupy it.
- IsFriendInTheWay and CNavArea::GetPlayerCount enumerate live players without
  a visibility gate. Navigation-only friendly blocking/density now use public
  entity positions and TeamInfo, excluding self/dead players/enemies. Enemy
  perception and objective knowledge remain gated. Feet-space NAV matching
  is used by this adapter in place of private area pointers.
- BotTouch/CCSBotManager::GetPlayerPriority and the0.33-second make-way action
  were absent. A public DLL_FUNCTIONS Touch hook now records generation-safe
  contacts. Human priority0, observed scenario priority1(C4/VIP), then1+the
  monotonic bot construction ID determine who yields. Current incumbent
  priority is recomputed after ownership changes. Classic CSBot lateral-only
  strafe projection is used; the REGAMEDLL_FIXES projection branch differs.
  Unknown hostage-escort priority is not invented. Action holds, freeze,
  airborne/ladder safety, and a public ground trace veto prevent unsafe
  sidestepping. No Compatibility route reservations or slot-based priority
  were introduced. Contact targets expire on map/round/actor reuse or0.33s.

For thin wall-top NAV polygons, a directed portal's16-unit inset could cross
the far edge of an8-unit destination. CSBot ComputePathPositions uses a step-in
explicitly smaller than its minimum area size. Astra now caps its inset at
half the destination depth and anchors it to the destination entry edge;
ordinary wide portals retain their previous inset. Four-direction regression
fixtures prove targets remain strictly inside the thin destination. This
geometric reproduction does not establish that every native ledge stall has
the same cause. The logged1735 path-selection failure and18->69 physical
stall remain native recheck scenarios; dangerous-drop rejection was retained.

Behavioral RED/GREEN evidence: patience expiry movement(including the only
occupied corridor), thin portal overshoot, touch-hook registration, and touch
priority/lifetime tests. Executed adapter scenarios verify a later-created C4
carrier causes the earlier ordinary bot to yield through RunPlayerMove, and
missing lateral floor support vetoes the avoidance. New diagnostics:
`profile playerAvoidance ... applied=... ground_safe=...`.

Final verification: complete Windows HostX86/x86 Debug build and69/69 CTests
passed(4.52s), PE x86/seven exact exports passed. Complete Linux x86 Debug
build and68 passed/one external-NAV performance test skipped(69 registered,
1.84s), ELF x86/seven exact exports passed. Review caught and fixed stale
creation-ID ranking for an unmanaged fakeclient reusing a managed slot.

Authorized deployment completed2026-11-08 20:58:21 JST after two HLDS-stopped
checks. Destination:
`D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
Backup:`astrabot_mm.dll.bak-20261108-205821`, matching previous SHA256
`7e63f2935c857b7ec493e317f5a3433c9eae740feae15e1b2dfc9d06b9c530ff`.
Built/deployed SHA256 both:
`d71e198a845e79a2aa0746986f8adc5482c7256f91778447446afd1e682b73c0`.
Linux built(not deployed) SHA256:
`5cebffe059d36480017470a9bf739e8f455885bba053fcaa6e35ab2fc8512cd8`.
Receipts,69-test logs and204-source-file manifest are under
build-action-adapter-x86-1451/friend-ledge-nov08-evidence/. The613-file baseline
preservation check found no unrelated changes. No server start/settings change,
staging, commit or push was performed.

FocalSpan and the code-review graph were refreshed. Runtime flow/test edges
remain incomplete in the graph; direct source review, behavioral regressions
and executed adapter tests supply the evidence above. Native replay of this
new artifact is pending; no complete-CSBot-parity or live-FPS claim.

## 2026-11-07 C4 jump cancellation / multi-bot congestion repair

Reported after the 16:05 deployment: planting sometimes jumped and cancelled,
and larger bot populations jammed on shared routes. Action translation stopped
analog movement for C4 but retained navigation's IN_JUMP and directional bits.
SelectC4, Plant and PlantContinue now remove those inputs, retain crouch and
plant attack, and cancel pending jump-crouch sequencing during action holds.
While a current actor/map/round plant attempt is active, navigation and its
stuck monitor are paused; action evaluation still observes completion/timeout.

Compatibility now checks currently perceived friendly positions against the
intended ground path at 0.5-second intervals (100-unit personal space, 30-unit
path radius), waits for `5 - 3 * aggression` seconds, then invalidates the path.
Replanning uses the existing observed teammate-density costs. Waiting resets
the progress monitor instead of triggering Wiggle. If waiting interrupted an
existing Wiggle, release invalidates that old movement/jump decision. Attack,
airborne movement, ladders and active special traversals bypass polite waiting.
No reservations, future teammate-route knowledge, or arbitrary route jitter
were added. Same-route selection remains possible when it is still optimal.

RED/GREEN regressions reproduced both input contamination and missing polite
waiting, plus stale Wiggle resumption after waiting. Tests cover wait expiry,
clearing, crowd-driven alternate-route choice, attack bypass, and friends
behind, beside or on a different floor. These are offline regressions; native
plant continuity and high-population throughput still require a new HLDS log.
Final verification: complete Windows HostX86/x86 Debug build, 68/68 CTests
passed (8.73 s), PE x86 and seven exact exports passed. Complete Linux x86
Debug build, 67 passed and one external-NAV performance test skipped (68
registered, 2.43 s), ELF x86 and seven exact exports passed.

At 16:39:48 JST, HLDS was confirmed stopped twice and the Windows DLL was
redeployed to `D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll`.
Backup: `astrabot_mm.dll.bak-20261107-163948`, verified to match the previous
`a84ca61a7707d840735e778c5d443efb2c24ab6a7c035e257d4156da6e50e9aa` DLL.
Built/deployed SHA256 both:
`7e63f2935c857b7ec493e317f5a3433c9eae740feae15e1b2dfc9d06b9c530ff`.
Linux built SHA256:
`76253b0b88323f074f33d3223535f59a0445b290373bc5cbbe05df5139209a1d`.
Receipt, verification hashes and the 202-source-file manifest are under
`build-action-adapter-x86-1451/c4-crowd-evidence/`. Comparison to the prior
source manifest found changes only in the eight intended source/test paths.
No server start, settings change, staging, commit or push was performed.

FocalSpan was refreshed and the code-review graph updated. The graph still
does not resolve this adapter execution chain or the controller's test edges;
targeted source review and executed tests provide the evidence above. Native
acceptance and full CSBot parity are not claimed.

## 2026-11-07 remaining stuck / postplant load repair

The latest `de_dust2` log session (15:12:03–15:20:03 JST, lines
34717–83075) repeatedly stalled on directed link `1736 -> 1735` for about
41 seconds. Compatibility now abandons a ground corridor after five elapsed
seconds without follower progress, preserving the failed-link exclusion for
replanning. This follows the pinned reference's path-progress timeout. Arrival
is processed before expiry; airborne movement, active jump/drop and ladders
are protected. Grounded entry into the next narrow area ends Wiggle even when
the displacement is less than 75 units. Regression tests cover expiry,
landing, late arrival, link exclusion and narrow-area recovery.

The postplant log window included a 250975 us FullUpdate maximum and an
82698 us PathSearch maximum. Guard selection now keeps an immutable snapshot
area index, avoids revisiting candidate areas, and reuses definitive NoRoute
results only with identical map/round/bomb/start/anchor and complete path-cost
inputs. Resource-limit failures are not cached. Every Hide.Spot random draw
is retained. A retained valid guard target skips rebuilding its path-cost
context. NavDocument area lookup and A* record lookup now use ID indexes;
ordered areas, neighbors, open-list traversal and strict cost tie handling
remain unchanged. No Compatibility reservations or artificial retry delay
were added.

Allocation-failure injection also reproduced an inconsistent default document
copy assignment after introducing the area index. Copy assignment now allocates
a complete candidate before committing; all eight injected cases keep lookups
consistent, and failed copies preserve the previous document.

Offline real-NAV comparison: 737-area `de_dust2`, 80 route cases plus 64
cost-context cases produced byte-identical routes, costs and search counters.
For nine bots' first guard selections, five-trial median decreased from
109172 us to 41382 us (62.1%); retained-target median remained 8 us. Accepted
selections (135) and random requests (90) matched. The harness uses optimized
header code with the Debug Core library; this is not a live FPS result.
ScenarioUpdate, BombGuardSelection and PathCostContext profiler boundaries
were added for the next native capture.

Final verification and authorized redeployment (2026-11-07 JST):

| Gate | Result |
|---|---|
| Windows Debug HostX86/x86 complete build / CTest | PASS; 68/68, 7.28 seconds |
| Debian WSL Linux Debug x86 complete build / CTest | PASS; 67 passed, one external-NAV performance test skipped, 2.84 seconds |
| PE / ELF artifact verification | PASS independently; x86 and seven exact Metamod exports each |
| Real-NAV output comparison | 144/144 byte-identical to the pre-index corpus |
| Dirty-tree preservation | 125 pre-task files checked; no unrelated content changed |
| Windows deployed DLL | `a84ca61a7707d840735e778c5d443efb2c24ab6a7c035e257d4156da6e50e9aa` |
| Linux built SO (not deployed) | `ba8e18d7ee115c91e45fba473316f9cadb775a86cb22522eea62fc6fafba6dbb` |

At 16:05:04 JST, with HLDS stopped, the Windows DLL was deployed to
`D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`.
Source and destination SHA-256 match. The previous DLL is preserved as
`astrabot_mm.dll.bak-20261107-160504` in the same directory (SHA-256
`2e587673b70e0f5c422bc25978766e3d7ff9e7af8856169ad434555cea36f0b9`).
Receipt, source manifest, comparison corpora and harnesses are retained locally
under `build-action-adapter-x86-1451/repair-20261107-evidence/`.
No server start or configuration change was performed.

Native repetition of the stalled links and planted-bomb FPS measurement remains
pending; this entry does not claim complete CSBot equivalence or elimination
of every live stuck location. Graph flow/test-edge coverage remains incomplete;
current source, executable tests and artifact hashes provide the evidence above.

## 2026-10-20 movement / dropped C4 / postplant guard repair

The latest `de_dust2` server log contains repeated stuck/Wiggle movement.
The adapter never sets `clearanceAvailable`; the Wiggle jump gate nevertheless
required it, suppressing every recovery jump in this runtime path. Compatibility
now follows the pinned reference's grounded/not-crouched, ladder/NAV and jump
interval checks without inventing a measured head-clearance value. This repairs
a confirmed suppression path, not every possible stuck location.

T dropped-C4 knowledge now receives dynamically resolved `BombDrop` (three
coordinates plus planted flag) and `BombPickup` messages through `WriteCoord`.
Only the actual managed T recipient learns the coordinates; CT, wrong recipients,
incomplete/mixed packets and stale generations do not acquire them. Sight remains
the fallback. Private bomb state is not accessed.

Planted-bomb Guard/Hide now selects NAV cover within 750 units around the bomb
area, prefers sniper flags when the observed weapon is a sniper, and rejects
visible occupied spots. It keeps a choice for 30–60 seconds, reselects when
occupied, and stops on arrival. Missing cover holds the current position instead
of sending every bot onto the C4. There is no Compatibility route reservation.
Candidate selection uses connected NAV areas and a route check; this is not a
claim of identical reference search ordering or complete Hide-state/RNG parity.

Regression coverage includes the previously failing unknown-clearance Wiggle
jump, recipient/length isolation, T recovery behind an opaque wall, pickup/round
invalidation, CT planted-bomb convergence, persistent/unoccupied NAV cover and
the runtime Hide movement/arrival stop. Native repeats of the three reported
scenarios and their FPS remain pending. Build/deployment receipts identify the
tested artifacts separately.

## 2026-10-20 BOT population regression

The deployed server log shows managed bots joining and then being kicked by
Console. Population maintenance counted active humans from `entvars.team`,
which remained zero on this CS server even after public `TeamInfo` announced a
team. With `bot_join_after_player=1`, the desired population therefore stayed
zero. Population maintenance now uses observed `TeamInfo` for every client,
including an explicit spectator/unassigned value, with an engine fallback only
before a message is known. Disconnect/map cleanup invalidates the cached team.

The actor-command regression fails before this repair and passes afterward:
human CT with zero engine team permits bot creation, spectator notification
overrides a stale engine team, and disconnected slot reuse loses the old team.
Live acceptance with a connected human remains pending; offline regression
success must not be treated as that acceptance. Build/deployment identities
are recorded separately in the local redeployment receipt.

## 2026-10-18 Vanilla compatibility repair — active, not accepted

This is the authoritative current status. Historical completion statements below
describe their recorded offline scope and do not close the current 19-difference
repair. See [VANILLA_REPAIR_LEDGER.md](VANILLA_REPAIR_LEDGER.md) for D01–D19,
source integration, tests and remaining gates. The approved repair stages 0–8
are distinct from the legacy planning phase numbers and P03–P12 parity work.

Current branch: `codex/vanilla-csbot-parity`, base `819471d`. The behavioral
reference remains ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`.
Vanilla public APIs are required; optional ReGameDLL capability discovery must
not enable private layout access or become a startup requirement.

Public observation, actual weapon state, profile/aim/reaction, shared round
plant timing, local C4 knowledge, Enhanced-only reservations/optimization,
timed movement recovery, buying/quota, scenario/radio and Teamwork behavior
have been integrated. Scoped reviews found and repaired sniper movement/scope
deadlocks, grenade cancellation and repeat-flash receipt, hostage competition,
automatic-follow state availability and delayed-radio slot-generation reuse.
This remains an uncommitted working-tree implementation, not a release.

Final offline verification of this tree (2026-10-19 JST):

| Gate | Result |
|---|---|
| Windows Debug HostX86/x86 complete build | PASS; cached compiler architecture preserved |
| Windows CTest | 68/68 passed, zero failures, 3.10 seconds |
| Debian WSL Linux Debug x86 complete build | PASS; `-m32`, strict warnings |
| Linux CTest | 67 passed, zero failures, 1 external-NAV performance test skipped; 68 registered, 1.44 seconds |
| JSONL comparison CLI Python tests | 11/11 passed on Windows and Linux; synthetic comparison tests only |
| PE and ELF artifact verifier | PASS independently: x86, seven exact Metamod exports each |
| Source whitespace / staged whitespace | `git diff --check` and `git diff --cached --check` passed; no staging performed |

Verified local build artifacts (not deployed):

| Artifact | SHA-256 |
|---|---|
| `build-action-adapter-x86-1451/astrabot_mm.dll` | `516118b0b7288e195d49bcb7aa6d6abe0acde18d9fd3be94a370065da8dba712` |
| `build-parity-linux-x86/libastrabot_mm.so` | `6f2e2ed07bec74ff41a7c9897bf5bf2136548f6b53c57fad8839a65230cace67` |

The graph was refreshed on the repair branch without errors; affected-flow
coverage remains zero. FocalSpan is updated separately. Neither index verifies
native behavior. A fixture's default BotProfile.db is preserved if it exists.
Movement fixtures distinguish actual C4-goal commands from subsequent stuck
recovery when their stub deliberately reports no displacement.

**Still open:** production capture/normalizer integration, pinned native
reference captures and RNG replay tapes, full D19 consumption equivalence,
Vanilla/ReGameDLL enabled/absent live scenarios on both OSes, supported maps,
1v1/2v2/multi-Bot stability and FPS. The comparison CLI is implemented and tested;
its producer integration is unimplemented, not an API limitation. See
[TRACE_REPLAY.md](TRACE_REPLAY.md). Optional ReGameDLL negotiation currently
supplements zero actor fields (MetadataOnly); custom chatter voice-bank playback
is not implemented. Those are not claims of completed functionality.

No server configuration, deployment, staging, commit or push was performed.
Every D01-D19 row retains reference/live acceptance gates. The approved plan's
all-nineteen completion condition has not been reached; no complete CSBot
equivalence is claimed.

## 2026-10-03 approved local release

The user approved committing and redeploying the three fixes below. Fresh release
gates passed: full MSVC HostX86/x86 Debug build, CTest **56/56** (3.76 s), PE x86
and seven exact Metamod exports. The 1,272,320-byte build retains SHA-256
`ced3e55e32882d6123e1e886e0e559fff2a9260d539331e4f6dd6150fcafb68e`.
Approved destination: `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll`.
Previous DLL SHA-256: `0599732edd351866adffebec2f02972df88cd45a57fd29d1033e90c1269a629f`.
The exact local commit, collision-safe backup and post-copy hash are recorded in
the execution workspace's `three-fix-deployment-receipt.json` after execution.
The built DLL includes NavMovement, RunPlayerMove and SynchronousLog timings,
game/wall window duration, frame count and throttled movementGate diagnostics.
Enable with `astrabot_profile 1`; it remains off by default. No profile CVar,
server start or push is part of this release. Physical runtime/FPS acceptance
and same-condition ZBot differential validation remain open.

## 2026-10-03 airborne continuity, physical-failure retry and preplant variation

Implemented in the working tree based on `98c4d4e5e623291bf7eb373391e8f91770104981`.
This offline amendment preceded the approved local release recorded above.
Final HostX86/x86 MSVC 14.51 Debug full build: PASS; CTest **56/56 PASS**
(5.70 s); PE x86 and seven exact Metamod exports: PASS. Independent direct-source
review: no remaining Critical/Important findings. Built DLL SHA-256
`ced3e55e32882d6123e1e886e0e559fff2a9260d539331e4f6dd6150fcafb68e` (1272320 bytes).
Live movement, collision, plant outcomes, team behavior and FPS remain unverified.

The comparison is independently interpreted public ReGameDLL_CS CSBot at
`b0889847fe6d03898be88acc9e366660efb40ab5`, not an identified ZBot binary.
No proprietary implementation was copied and no exact parity is claimed.

- Read-only qconsole evidence SHA-256
  `396371ce63c8ea59bb61dc14dd098933c324cd91e6470401b50974d353d240f6`:
  all 103 observed armed jumps were followed by an airborne neutral command.
  The continuing-movement gate incorrectly reused grounded spawn readiness.
  Initial joined/spawn readiness still requires grounding; established live
  movement preserves flight direction, route and landing tracker. Cached
  IN_JUMP is stripped while airborne. FreezeTime, death and spectator gates stay.
  Actual grounded landing feedback exposes measured health loss separately.
- After the first human A plant, the log records a successful route repeatedly
  vetoed as UnsafeDrop around directed link 1504 -> 1505. Assignment generation
  changed 36 times in the sampled interval; A* success did not establish physical
  traversal safety. Unsafe failures now retain bomb entity identity, failed link
  and position anchor across objective/roam reassignment. One immediate directed
  bypass is allowed; failed retry uses 128, 256 then 512 active server frames.
  FreezeTime pauses this Core budget. Expiry permits one fresh safety evaluation
  of the original route. New bomb, lifecycle reset or >=64-unit grounded XY
  displacement clears the history; adjacent-area jitter does not.
  This is a bounded Astra policy, not a recovered ZBot constant.
- Existing coordinator failure handling hands the bomb objective to another
  eligible CT; repeated old-owner failure reports cannot invalidate that owner.
  Core locomotion vetoes now identify the actual rejected corridor link after
  a safe walking prefix, rather than reporting the preceding walking link.
- Opt-in profiler adds actual game and wall window seconds and StartFrame count,
  and separates Core NavMovement, the engine RunPlayerMove callback and synchronous
  runtime console logging. Parent stages remain inclusive and must not be summed.
  movementGate emits at most one record per bot per second. Disabled scopes read
  no additional wall clock. These diagnostics do not demonstrate an FPS gain.
- Public CSBot manager chooses an earliest plant decision at RestartRound time
  + random 10..30 seconds (`cs_bot_manager.cpp:114-129`); that includes FreezeTime.
  Idle/Hunt can move the carrier before later closest-zone selection
  (`cs_bot_idle.cpp:172-190,879-880`, `cs_bot_hunt.cpp:67-74,171-213`).
  Astra now samples an initial reachable site through the existing compatibility
  RNG boundary and keeps it for the carrier/round. At deadline, site arrival or
  urgency it transitions once to the nearest currently reachable route.
  Candidate travel plus five seconds of plant margin is reserved; missing timing,
  RNG or NAV authority falls back to the existing nearest route. Carrier loss,
  death, new round and observed planted bomb cancel the approach.
  This bounded site-directed movement differs from reference whole-map Hunt and
  SAFEST/danger-weighted route selection. It enables A/B alternatives without a
  claimed or enforced 50:50 final plant ratio. No learning was implemented.
- Offline regression coverage includes 10/60/90/100/128 Hz and variable command
  cadence, airborne single-jump/landing, delayed engine/log callback attribution,
  independently captured disabled diagnostics, dangerous-link bypass and bounded
  expiry at multiple frame gaps, entity generation and progress resets, FreezeTime,
  real controller failure -> CT handoff, multiple seed/round/start site choices,
  same-seed replay, held goals, timer/arrival transitions, unreachable and urgent
  fallback, and human-planted C4 -> CT movement/IN_USE convergence.
  Fixtures simulate public SDK observations, not BSP collision or engine physics.

## 2026-10-02 full-update ground cadence and bounded off-mesh commands

IMPLEMENTED_OFFLINE_VERIFIED for ground observation cadence and horizontal
recovery command construction and bounded recovery-candidate reselection.
Physical escape from a blocked off-mesh position remains LIVE_UNVERIFIED;
failed/exhausted attempts are not counted as recovery success.

Comparator: independently interpreted ReGameDLL_CS
`b0889847fe6d03898be88acc9e366660efb40ab5`, `cs_bot_nav.cpp:202-253`
(current-direction ground probes), `cs_bot.cpp:382` (StayOnNavMesh), and
`cs_bot_nav.cpp` (Wiggle). This is a fixed public CSBot proxy. No specific
ZBot binary/version or live differential run was available; no exact ZBot
parity or proprietary-source reuse is claimed.

- Read-only evidence: de_dust2 qconsole.log, SHA-256
  `ad86503d384270d2b6636db29ca03c31cdf05c2af2148dea790164b7a1c8aa5b`.
  Bert's 53 pre-ascent traversal observations had frame gaps 2 through 14,
  none 1. The old route required exactly the next server frame although the
  adapter probes only at full updates. The log contains an emitted jump and
  ascent before subsequent area=0 recovery, so it does not prove that the
  user's manual push alone caused ascent or identify its exact time.
- The last-round four-bot stalls are observed with freeze=0. Recovery XY
  direction shrank near 0.02 under 3D normalization, reducing 240-unit intent
  to roughly 4-unit horizontal command speed. This is separate from pre-wall
  missed lookahead and is not an objective waiting or freeze diagnosis.
- Ground routes now require the next full-update sequence and age <=0.25 s,
  a newer server frame, and unchanged actor/generation/map/round scope.
  Traces are sampled afresh at the current position; target/direction and
  existing grounding, NO_JUMP, drop/gap and height gates remain enforced.
- Recovery independently chooses a NAV geometry candidate with height in
  ranking (64-unit vertical tolerance), aims 5 units inside its XY bounds
  (half-width for thin areas), records the actual target and normalizes XY
  only. This is geometry-only, not proof of collision clearance or reachability.
- After 20 consecutive unsuppressed full updates without >1-unit cumulative
  improvement, the failed candidate is excluded, route state is cleared and
  NoRoute/RecoveryNoProgress reports one candidate-failure event. The next full
  update selects a different eligible candidate. Maximum four failed candidates
  are retained; no eligible candidate or an exhausted four-attempt budget is
  the final neutral-stop condition, not the first 20-update failure.
- A candidate failure joins the existing objectivePathFailed feed. The existing
  coordinator releases/reassigns the objective; PlantC4 keeps its existing
  carrier-specific semantics. Goal replacement retains recovery exclusions,
  preventing repeated selection of the same failed recovery area. Final blocked
  updates do not repeatedly report objective failure. Exact NAV re-entry clears
  recovery history before objective resolution and resumes normal route search.
  >=8-unit external 3D displacement from the first failed position, actor/map/
  round change or controller reset also permits a fresh attempt. FreezeTime
  spends no progress budget. Final exhaustion may remain stopped if no new
  position/lifecycle evidence arrives; there is no physical escape guarantee.
- Opt-in profiler diagnostics add one groundRecovery record per bot per
  second with update/route age, fresh far80/near30/gap10 trace values, selected
  recovery point, geometry-only flag, no-progress count, one-shot candidate
  failure flag and explicit reason.
- RED evidence: old cadence fails normal/variable FPS adapter fixtures; old
  recovery fails XY direction/target, stacked-height and bounded-progress
  assertions. GREEN covers far-missing/sloped cases at baseline 10, 60, 90,
  100, 128 and variable FPS (12 adapter scenarios), scope/age boundaries,
  sequence wrap, FreezeTime, small XY/large Z, stacked/thin NAV, Z tolerance,
  cutoff/reset transitions, cumulative progress and oscillation. These are
  synthetic SDK fixtures, not physical BSP replay or live FPS measurements.
- Additional RED: the first-stop implementation fails alternate-candidate
  and public RunPlayerMove direction tests. GREEN: failed candidate -> different
  candidate, all-candidate/four-attempt exhaustion, retained exclusions across
  goal replacement, alternate NAV entry -> objective path search, public
  RunPlayerMove alternate direction -> neutral only after both fixture candidates
  fail -> routed movement after NAV entry. Recovery controller -> existing team
  coordinator integration reassigns dropped C4 to another actor and lets the old
  owner attempt another recovery area. NAV entry with an unresolved objective
  also clears history. Fixtures are synthetic; they do not model engine physics.
- Final HostX86/x86 MSVC 14.51.36231 Debug full build: PASS; CTest 56/56 PASS
  (6.49 s); PE x86 and seven exact Metamod exports: PASS. Read-only final
  review: no Critical/Important findings.
- Working-tree build based on HEAD
  `1feec9a5f0cb39da88b2291935cd34c011951071`: 1,210,880 bytes,
  SHA-256 `0599732edd351866adffebec2f02972df88cd45a57fd29d1033e90c1269a629f`.
  This records the pre-commit offline artifact. The approved October 3 local
  commit/deployment receipt is reselection-deployment-receipt.json in the
  execution workspace. The previous server DLL has SHA-256
  `80c505ee1d75422845c1549507c1c2d20de88971516deb80f622b21d5b201980`.
- Live wall clearance/landing, pushed-off-NAV automatic escape, actual FPS,
  multi-bot performance and reference differential validation: NOT RUN.
  Offline evidence: reselection-red.log and reselection-full-build.log in the
  execution workspace; at offline verification, 10 tracked paths changed without staging or commit.
  Recovery candidate selection scans all NAV areas; live cost is unmeasured.
  P07/P07.6 remains PARTIAL. P08 is not started.

## 2026-10-02 combined round-freeze and ground-lookahead verification

IMPLEMENTED_OFFLINE_VERIFIED. Normal Round_End schedules FreezeTime after
mp_round_restart_delay; Round_Start ends it without starting another interval.
Initial Game_Commencing schedules it after the ordinary-game three-second
restart delay. Prepared-generation duplicate Game_Commencing/Round_End events
cannot restart or extend that interval. Reference: independently interpreted
ReGameDLL_CS b0889847fe6d03898be88acc9e366660efb40ab5,
multiplay_gamerules.cpp (Game_Commencing/NeededPlayersCheck, RestartRound and
OnRoundFreezeEnd). Career/custom hook delays are unavailable at this boundary.

- Reproduction: restoring the baseline HEAD lifecycle fails the Round_Start
  end-freeze regression. Before the initial-round fix, its three-second start
  boundary regression also fails. The corrected public adapter passes both.
- Public execution regression covers three frozen frames, zero movement and
  horizontal velocity, retained command timing/vertical velocity, no ground
  lookahead probes, and movement resumption after Round_Start.
- Core suppression preserves the saved objective route without accumulating
  stuck time. The bounded near30 fallback and scope/safety tests remain green.
- Complete HostX86/x86 MSVC NMake Debug build: PASS. Complete CTest: 56/56 PASS
  (2.87 seconds). PE x86/seven exact Metamod exports: PASS.
- Final combined Debug DLL: build-action-adapter-x86-1451/astrabot_mm.dll,
  1210880 bytes, SHA-256 80c505ee1d75422845c1549507c1c2d20de88971516deb80f622b21d5b201980.
- Both approved changes and their pre-existing source/test prerequisites are
  included in the local commit scope; no push. Deployment receipt and final commit
  identity are recorded outside the repository after the stopped-server copy.
- No server was started for this verification. Live HLDS/ReHLDS timing,
  physical clearance/landing, FPS and multi-Bot acceptance remain NOT RUN.
  Historical September 27 logs describe the old deployed DLL only.

This record supersedes the earlier near-ground-only artifact below.
P07/P07.6 remains PARTIAL; P08 is not started.

## 2026-10-02 bounded near-ground jump fallback

`IMPLEMENTED_OFFLINE_VERIFIED` for the approved 30-unit upward-jump fallback.
Pinned CSBot `b0889847fe6d03898be88acc9e366660efb40ab5`,
`cs_bot_nav.cpp:202-253`, continues to the near probe when the running-only
80-unit probe misses or finds a slope. The current Astra adapter and Core no
longer require flat far ground for that near-ground decision. Known far-drop
and missing immediate-ground vetoes, configured step height, 41.8-unit jump
limit, current-area NAV_NO_JUMP, grounding/ladder and observation-scope checks
remain intact. This is a bounded fallback improvement, not full 80/30/10 parity.

- Baseline: `7f86bdb54c6bfd725de52a75a4fe6fcfdfc686fd` plus nine pre-existing
  modified source/test files. This earlier record predates the final combined FreezeTime build above.
- Core regression: 17 cases cover fallback and retained safety/height gates;
  six cases failed against the old Core gate before the fix.
- Public-adapter fixture: deterministic engine RNG + synthetic NAV/ground
  traces -> PluginRuntime -> RunPlayerMove emits IN_JUMP for missing/sloped
  far ground. Both cases fail when only the old adapter gate is restored.
- Actor slot/map/round/actor-generation/frame and target/direction isolation
  are covered by the locomotion and route-scope tests.
- Focused locomotion + actor-command CTest: 2/2 PASS.
- Full HostX86/x86 MSVC 14.51.36231 NMake Debug build: PASS;
  complete CTest: 56/56 PASS, 4.62 seconds.
- `verify_x86_artifact.py`: PASS, PE x86 and seven exact Metamod exports.
  Debug DLL: `build-action-adapter-x86-1451/astrabot_mm.dll`, 1,210,880 bytes,
  SHA-256 `4d7abb7f314674cf18212e271d14fd6ff6ea3db8269bb98b79c84dd3f6d254cb`.
- Live HLDS/ReHLDS, physical clearance/landing, FPS, multi-Bot, Linux x86
  and full pinned differential validation: NOT RUN in this change.

The DLL identity above supersedes historical build identities only for this
dirty working-tree Debug build. P07/P07.6 remains PARTIAL; P08 is not started.

## P07.6 production runtime integration audit

`PARTIAL` — the production adapter now has a public freeze execution gate,
typed Goal/NAV transaction diagnostics, persistent-route temporary-area
handling, and opt-in one-second profiler counters. Focused x86 tests are
offline evidence only. Fresh live HLDS/ReHLDS freeze, Goal, physical progress,
FPS, and multi-Bot intervals remain open. `P08 NOT STARTED`.

P07.6-PERF additionally measures exact visibility TraceLine/FOV/LOS/body-probe
counts and one-second stage aggregates, adds default-off vision/path/trace A/B
CVars, and avoids non-objective bomb-site NAV corridor enumeration. No live FPS
or alive-Bot scaling result is inferred from these offline checks.

## P06 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Vision boundary | IMPLEMENTED_OFFLINE_VERIFIED | Core stores FOV/LOS/body-region observations; public adapter uses the CSBot probe order and strict 2D dot > 0.5 fixture semantics |
| Ground truth / belief split | PASS boundary | hidden actors become ObservedAbsent; exact hidden coordinates are not copied into Compatibility belief |
| Last-known knowledge | PASS offline | actor-generation-scoped memory preserves last-seen position and expires by deterministic configured age |
| Hearing/noise | PASS offline boundary | range, reaction delay, 3-second replacement rule, priority/nearer selection, and bounded retention are modeled; source identity is not exposed |
| Event knowledge | PASS offline boundary | death/spawn/respawn/round mutations are explicit; live GameEvent delivery remains unavailable |
| Runtime integration | IMPLEMENTED_UNVERIFIED | Full Update-scoped perception scan is wired into managed bots; objective path remains public/inferred |
| Production fixture | PASS | Metamod observation -> public trace/FOV/body result -> PerceptionAssembler -> Compatibility belief |
| Deterministic tests | PASS | focused perception, adapter, and production-boundary tests pass |
| Full x86 CTest | PASS | complete current suite 51/51 PASS |
| Phase 8 PowerShell | PASS fixture-only | action boundary, live-log, objective, and slow-movement fixture checks pass; live combat/C4 fields remain false/unverified |
| FocalSpan | PENDING | refresh after final P06 edits |
| Live HLDS/ReHLDS | NOT RUN | engine trace parity, smoke, private CSBot queues, and live event delivery remain unverified |

P06 is intentionally PARTIAL. The compatibility information boundary and
offline semantics are implemented, but a public Metamod adapter cannot prove
ReGameDLL private m_enemyQueue, smoke handling, profile reaction timing,
GameEvent delivery, or exact random noise-position placement without live
differential evidence.

## P05 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Reference state inventory | PASS | 12 ordinary states plus the separate Attack overlay inventoried from the pinned ReGameDLL-CS source |
| Compatibility state owner | PASS boundary | one `compat::CompatibilityStateMachine` per managed actor; existing planner state machine retained |
| SetState lifecycle | PASS offline | OnExit -> OnEnter -> state publication -> timestamp, including same-state requests |
| Attack overlay | PASS offline boundary | underlying state retained; overlay owns update while active; state changes stop overlay first |
| Full Update timing | PASS regression | state updates are gated by existing P02 Full Update events; scheduler unchanged |
| Runtime integration | IMPLEMENTED_UNVERIFIED | managed bots own/reset state machines; public C4 carrying request is wired in Compatibility Mode |
| Deterministic tests | PASS | focused state-machine target covers ordering, multi-step, blockers, overlay, isolation, lifecycle |
| Full CTest | PASS | complete x86 Debug suite `50/50 PASS` |
| Phase 8 | PASS fixture-only | live-log, objective, slow-movement, and action-boundary fixture checks passed; live combat/C4 remains unverified |
| FocalSpan | PASS freshness | refreshed after P05 source edits; `stale: false` |
| Live HLDS/ReHLDS | NOT RUN | no live state/differential trace claim |

P05 is intentionally `PARTIAL`. State orchestration and the Attack overlay
boundary are implemented and offline verified, but most state-internal
behavior, private observations, exact objective/vision/NAV decisions, and a
pinned differential runtime trace remain open. P06 is recorded above.

## P04 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| Observation Core model | PASS boundary | SDK-free typed values, quality/source/freshness/lifecycle metadata, explicit invalid values |
| Public ObservationAdapter | PASS boundary | x86 fixture covers player fields, FOV, actor/frame, C4 proxy, planted-bomb inference, missing engine context |
| PluginRuntime integration | PASS regression | world/objective/action fixture and FakeClient isolation remain green; decisions were preserved |
| Observation trace | PASS boundary | adapter-local sequence, actor/timing metadata, enhanced/RNG isolation fixture |
| FOV | PARTIAL | public `pev->fov` is collected; internal scope/visibility/trace parity is not available |
| Weapon state | PARTIAL | private active weapon/ammo/reload/accuracy/timers/zoom remain unavailable; synthetic weapon remains marked |
| Objective state | PARTIAL | public C4/entity proxies are inferred; defuse/kit/hostage/VIP/round state remains unavailable or unknown |
| Configure/build | PASS | current x86 HostX86/x86 NMake Debug worktree build |
| Focused tests | PASS | observation model 1/1, adapter 1/1, trace 1/1, integration/action/isolation and P02/P03 focused suites passed |
| Full CTest | PASS | current x86 complete build and `49/49 PASS` |
| Phase 8 PowerShell | PASS fixture-only | live-log umbrella, objective, slow-movement, and action-boundary checks passed; combat/C4 live fields remain separate |
| FocalSpan | PASS | final worktree index fresh after source/documentation update |
| PE/Python verifier | NOT RUN | `py -3` could not create its Windows Python process |
| Live HLDS/ReHLDS | NOT RUN | no new live acceptance claim in P04 |

P04 does not promote any observation to `MATCH` solely from an adapter or
offline fixture. P05 state orchestration is now implemented partially; internal
behavior and live differential acceptance remain open.

## P00 result

`PASS` for the P00 audit artifact set. This does not mean the CSBot-compatible
baseline passes. The Compatibility Mode boundary is `implemented_unverified`;
behavioral parity remains `not established`.

P00 was analysis/documentation only. No gameplay implementation, Astra extension,
or reference source was changed.

## Frozen baseline

- AstraBot audit commit: `9788c245f5017567c60c35fa3afe474d56d1a349`
- ReGameDLL-CS reference: `master` at `b0889847fe6d03898be88acc9e366660efb40ab5`
- Current Astra worktree: dirty before P00; pre-existing changes preserved
- Reference worktree: dirty, but all comparison claims use the pinned commit
- Current fresh DLL: `build-action-adapter-x86-1451/astrabot_mm.dll`, SHA-256
  `3d2bda1bd90065ca5553d0b4de13e9d6e32f03c59f099981c705d5e81dcc4d0f`

## Baseline gates

| Gate | Result | Evidence |
|---|---|---|
| Configure | PASS | CMake reconfigured `build-action-adapter-x86-1451` |
| Full x86 build | PASS | VS 2026 HostX86/x86, NMake, Debug, all targets |
| CTest | PASS | 42/42, 0 failed, 3.23 seconds |
| Phase8 PowerShell fixture/self-tests | PASS | live-log, objective, action-boundary checks |
| Python manifest/artifact tests | NOT RUN | `py -3` could not create the installed Python process |
| New live HLDS/ReHLDS run | NOT RUN | P00 did not start a new live server run |
| Existing live acceptance | PARTIAL | Windows join/human damage/death; autonomous action/C4/Linux remain open |

## Phase ledger

| Plan | Status | Result / blocker |
|---|---|---|
| P00 | complete | reference frozen; source map, matrices, observations and blockers recorded |
| P01 | complete | explicit runtime mode, policy isolation, snapshot diagnostics, and regression tests |
| P02 | complete | Compatibility timing/command cadence implemented and deterministically verified; live/private-state parity remains open |
| P03 | partial | Compatibility RNG boundary, engine delegation, scripted consumption, and trace schema are offline verified; specific callsites and live parity remain open |
| P04 | partial | observation boundary merged; private-state and live gates remain open |
| P05 | partial | compatibility state orchestration and attack overlay offline verified; internal behavior and live differential trace remain open |
| P06 | partial | perception boundary, bounded belief, public visibility fixture, and offline gates complete; live/private differential evidence remains |
| P07-P12 | pending | downstream behavior through live parity remain separate evidence gates |
| P13 | deferred | enhanced intelligence remains downstream of the baseline |

The repository's existing `.planning/STATE.md` is not rewritten by this audit;
its Phase 8 live-gap state remains authoritative for that separate workstream.

## P02 readiness

`NO` for claiming CSBot behavioral parity. P02 planning may begin, but timing,
RNG, private-state, visibility, state-machine, NAV, combat, and live blockers
remain open.

## P02 verification

- ReGameDLL-CS reference: `b0889847fe6d03898be88acc9e366660efb40ab5`
- Timing source: `runtime::BotTimingScheduler`, absolute `now + interval`
- Event order: `Upkeep -> CommandReset -> FullUpdate -> CommandExecute` when both gates are due
- Delayed frames: one execution/full update maximum; no catch-up loop; deadlines rebase from current time
- Deterministic CTest: `astrabot_runtime_timing` PASS and `astrabot_compat_actor_command` PASS
- Configure/build: PASS in VS 2026 HostX86/x86 NMake Debug environment
- Full CTest: 44/44 PASS, 0 failures
- Phase 8 PowerShell checks: PASS; slow-movement fixture reports partial live combat/C4/objective observations as expected
- PE/Python verifier: environment-unverified; `py -3` could not create the installed Python process
- New live HLDS/ReHLDS run: NOT RUN; live autonomous movement/combat/C4 and private-state parity remain open

## P01 verification

- Windows x86 configure/build: PASS
- CTest: 43/43 PASS, including `astrabot_runtime_mode_policy`
- Phase 8 PowerShell fixture and action-boundary checks: PASS
- New live HLDS/ReHLDS run: NOT RUN (out of P01 scope)

## Required next evidence

1. Define and prove compatibility-mode isolation before enabling any Astra-only
   planner, route memory, profile adaptation, team director, or learning behavior.
2. Differentially compare the new deterministic RNG boundary and reference call
   order; the P03 offline boundary exists, but the pinned production reference
   tape and live comparison remain open.
3. Map public/private observations and choose explicit `exact`, `delayed`,
   `inferred`, or `unavailable` policies.
4. Establish timing/command traces for the reference and Astra paths.
5. Keep real-server movement, combat, C4, Linux x86, and lifecycle acceptance
   separate from offline tests.

## P03 result: PARTIAL

| Area | Result | Evidence / limitation |
|---|---|---|
| Reference RNG model | PARTIAL | 148 executable direct expressions, callback chain, local ReHLDS implementation observation, and 62-file transitive audit recorded in `RNG_MODEL.md` |
| Compatibility RNG contract | PASS for boundary | SDK-free request/result types, exact type/bounds tape checks, too-many/too-few detection, and branch fixtures pass offline |
| Production engine adapter | PASS for boundary | Fake callback tests verify exact forwarding and unavailable-callback failure; no production decision callsite is claimed |
| Shared ownership | PASS for boundary | One PluginRuntime-owned source; multi-Bot A/B/A scripted ordering passes |
| Enhanced isolation | PASS for boundary | Separate source instances do not advance Compatibility tape |
| RNG trace | PASS for schema/source | Optional bounded record includes sequence, semantic site, type, bounds, result, actor, and timing context |
| Specific CSBot callsite parity | UNVERIFIED | AstraBot production behavior does not yet consume the new source at equivalent branches |
| Live HLDS/ReHLDS RNG parity | NOT RUN | No fresh pinned reference/AstraBot live RNG trace was collected |

P02 timing regression remains PASS and unchanged. P03 does not promote overall
CSBot parity to MATCH and does not authorize P04.

## P07 result: PARTIAL

| Gate | Result | Evidence / limitation |
|---|---|---|
| NAV load | IMPLEMENTED_OFFLINE_VERIFIED | Existing read-only legacy versions 1-5 loader/model tests; ladder object linkage remains unavailable |
| Area lookup | IMPLEMENTED_OFFLINE_VERIFIED | Containment, floor tolerance, bounded nearest recovery fixtures |
| Path cost | PARTIAL | Static distance, crouch, jump, and route-type costs; dynamic danger/fall/ladder/private state unavailable |
| Tie-break | IMPLEMENTED_OFFLINE_VERIFIED | Stable CSBot discovery-order equal-cost fixture |
| Path persistence/recompute | IMPLEMENTED_OFFLINE_VERIFIED | `pathSequence`, `fullUpdateSequence`, and explicit recompute reason fixture |
| Movement command intent | IMPLEMENTED_OFFLINE_VERIFIED | 20-unit arrival, posture/traversal intent, P02 cadence unchanged |
| Jump | PARTIAL | NAV attribute and bounded jump/drop intent/state tests; live jump physics unverified |
| Ladder | PARTIAL | Separate enter/maintain/exit controller tests; real ladder object/mount/climb physics unverified |
| Stuck | PARTIAL | Bounded offline recovery; exact CSBot averaged velocity/RNG/wiggle/live physics unverified |
| Enhanced isolation | IMPLEMENTED_OFFLINE_VERIFIED | Compatibility route ignores actor rotation; explicit Enhanced policy retains adaptive path selection |
| Deterministic fixtures | PASS | NAV query, route persistence, posture/traversal, jump/drop, ladder-state, and movement tests |
| Python/PE verifier | NOT RUN | Environment verifier remains separate and unexecuted |
| Live HLDS/ReHLDS | NOT RUN | No new live differential acceptance claim |

P07 remains `PARTIAL`: offline Compatibility route/traversal boundaries are implemented and tested, while live physics/private reference evidence is open. P08 is not started.
