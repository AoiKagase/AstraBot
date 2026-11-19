# P11 differential trace comparison

`tools/compare_parity_traces.py REFERENCE.jsonl CANDIDATE.jsonl` compares fixed
captures and prints a JSON report. Exit codes: **0** equal, **1** differences,
**2** malformed input or invalid options. Python standard library only.

## Pinned offline hearing replay

On Linux x86, use a current `astrabot_core` build and a local ReGameDLL-CS
checkout containing the pinned commit. The runner builds both drivers and
writes raw and normalized traces, a build and input manifest, and a comparison
report in a new output directory.

```sh
rtk proxy cmake --build build/linux-x86-debug --target astrabot_core
rtk proxy python3 tools/replay_csbot_noise.py \
  --reference ../ReGameDLL_CS \
  --core-library build/linux-x86-debug/libastrabot_core.a \
  --output /tmp/astrabot-csbot-hearing
```

The runner reads native function bodies and two event-handler fragments directly
from Git at the pin. It compiles them with `REGAMEDLL_FIXES` and a small host shim
that supplies a stationary observer, a single flat NAV area, preclassified
sounds, explicit visibility, and scripted RNG values. The Astra driver calls
its production perception assembler, NAV projection, investigation planner and
look planner. Each driver emits the delivered sound inputs, memory snapshots,
ordered float requests, investigation decisions, noise consumption, and startle
look decisions with inhibition deadlines. The clock is controlled by the
fixture. State-machine, command and path fields outside the measured callbacks
are unknown.

This fixture covers hearing and composed startle at time zero, the three-second overwrite boundary,
retention past twenty seconds, quarter-second investigation cadence, and
startle at 1,000 units with obscured and out-of-range controls. It does not run
the GameDLL, engine event classification, native NAV search, or an HLDS match.
The startle excerpt measures the resulting look and inhibition after the
branch returns, and captures RNG calls at the shared callback boundary.
The `Z` rows feed a shot through the normal intake before checking startle at
time zero, at 0.5 seconds, and just after the cutoff. The startle checks use
independent inhibition state to expose the freshness boundary. The standalone
`L` rows provide range, visibility, and inhibition controls. Starting a map in
ReHLDS sets server time to 1.0 seconds, so the time-zero case uses the controlled
clock in this fixture.

## Windows x86 HLDS capture

Run the pinned `b088984` Win32 GameDLL in two local HLDS sessions with the same
`de_dust2` BSP, NAV, and bot profiles. Run the native bots on one side and
AstraBot in Compatibility mode on the other. The reference capture adds console
records to the audible-event classifier and successful multi-area path builder.
Both sides sample `CmdStart` before the capture fixture's input overrides.
On AstraBot, `astrabot_profile 1` enables `profile nativeWeaponSound` and the
`goalAssignmentCorrelation` and `motionCommand` diagnostics.

A controlled AK-47 shot in both sessions produced the same weapon playback
event and classified as `HIGH` with range 2,000. Additional captures matched
AK-47 reload (`LOW`, 1,100), AWP fire (`HIGH`, 99,999), and TMP fire (`HIGH`,
1,000). These records measure classification before AstraBot's per-observer
delivery. At the start of free movement,
the CT bot used area 5 on both sides. The native bot selected area 21 via
1522, and AstraBot selected area 874 via 60. The two runs chose different
destinations and entered Hunt at different times. This capture records those
differences and their command effects.

The fixed plant probe starts the CT at `(-1361.3, 2447.0, 37.9)` after the
normal `Planted_The_Bomb` event. Both captures show a path toward NAV area
293 beginning with `157 -> 54`. Their timing and command axes differ, and
subsequent goals diverge. The probe compares a shared route segment and
observed dispatch. Complete route, movement-command and live RNG equality
remain open.

## Windows x86 Hunt goal selection

Two isolated `de_dust2` HLDS runs used the same map, profiles, and pinned
Win32 GameDLL. The reference probe records the Hunt goal draw and the path
retry draw. An AstraBot test build records the same call sites before and
after the change. Each engine uses its own RNG stream.

For the first CT Hunt update, the old AstraBot build drew the retry delay
before the goal and dispatched forward input in the same update. The fixed
build drew the goal before the delay, like CSBot, and dispatched zero movement
while the new corridor was prepared. During the cooldown it selected a goal
without drawing another delay or searching. The deterministic controller test
also covers the exact cooldown boundary and a path invalidated before retry.

A separate control sent the reference's measured NAV start 5 and goal 842 to
AstraBot's Safest search using the captured `de_dust2.nav` file. All 13 corridor
areas matched the reference run after removing its duplicated endpoint area.
A separate live probe captured `StatusIcon buyzone`: AstraBot briefly left Buy
before its first zone notification during the scheduled restart. The fix holds
Buy while the notification is missing and the restart is pending. It uses any
known zone result and still requires an observed buy zone for purchase commands.
In the follow-up live run the first Hunt decision moved from before
`Round_Start` to after it. Full-match RNG and movement-command equality
remain open.

## Linux x86 Hunt ground routes

The controlled ReHLDS replay used `de_dust2`, identical CT spawn coordinates,
and a small separate NAV fixture in each isolated instance. Diagnostic builds
used the pinned CSBot GameDLL and kept each Hunt random call while injecting
the selected index. The fixture fixes the requested area for each case.

| Case | Requested area | Ground route and endpoint on both builds |
| --- | --- | --- |
| Same area | 5 | Area 5, `(437.5, 2412.5, -128)` |
| Disconnected goal | 7 | Areas 5 → 6, `(437.5, 2200, -128)` |

After selecting area 5, Hunt selected area 7 on the next update while
`ComputePath` was throttled. Both bots retained and followed the existing
same-area route after the second selection. In the disconnected case, ground
A* retained the closest reachable area and aligned the endpoint with the
last portal. Both fixed scenarios produced movement on the retained path.
The replay covers ground routes. Hunt ladder reachability and world-height
sampling for a projected endpoint remain outside this replay.

## Windows x86 round opening Buy

The isolated `de_dust2` replay compared two $800 bots against Win32
ReGameDLL-CS `b0889847fe6d03898be88acc9e366660efb40ab5`. AstraBot had previously
entered Hunt before the BuyZone HUD update arrived. The reference stays in
Buy for its initial two-second delay, then attempts purchases at intervals
strictly greater than 0.2 seconds. Each stock primary attempt draws sniper
eligibility before the team stock index, even when all the weapons are too
expensive. With no primary, the seventh attempt leads to equipment purchases
in the same update. A finished bot stays in Buy until freeze ends.

The replay recorded the kit, grenade, and pistol preference draws for CT,
and the grenade and pistol preference draws for T. Each bot then consumed
seven ordered sniper and stock-index draw pairs. The controller regression
also fixes the RNG values and checks the resulting command order. The live
servers drew from independent engine streams.

| Freeze | Reference CT / T first Hunt after Round_Start | AstraBot CT / T |
| --- | --- | --- |
| 0 s | 3.80 / 3.81 s | 3.83 / 3.85 s |
| 5 s | 0.12 / 0.13 s | 0.15 / 0.15 s |

## Full runtime replay remains open

The semantic record classes come from `docs/TRACE_SCHEMA.md` and
`docs/parity/TRACE_SCHEMA.md`. Existing RNG, state, observation and timing sinks
use independent sequences and do not emit one common capture envelope. The
canonical envelope below is the comparator's input contract. A production
collector, full native binary capture, production RNG replay tape, and live
scenario acceptance remain open. Synthetic unit tests exercise comparison
only and are never golden measured traces. `live_parity_accepted` is always
false: even an equal file pair requires independent capture provenance and
scenario acceptance before closing parity.

## Canonical JSONL envelope, version 1

First line is one metadata object with these mandatory fields:

```json
{"kind":"metadata","schema":"astrabot-parity-v1","capture_kind":"fixture","reference_pin":"b0889847fe6d03898be88acc9e366660efb40ab5","map":"de_fixture","scenario":"synthetic format example only","profiles":{"1:1":{"name":"fixture","skill":50}},"cvars":{"bot_difficulty":2},"rng_seed":7,"identity_settings":{"actors":"slot:generation","nav":"map-native-area-id"},"coverage":["command"]}
```

`capture_kind` is `fixture` or `live`, declared by the caller; it does not prove
native provenance. Both files contain the same comparison configuration:
full pinned reference SHA, map/scenario, resolved profiles (including inherited
values), all decision-relevant CVars, RNG seed and actor/NAV identity mapping.
Record build hashes, environment, capture commands and provenance in a separate
capture manifest: reference/candidate executable hashes necessarily differ.
Every metadata field, including extensions, compares exactly; do not silently
remove differing settings in a normalizer. `coverage` lists the distinct record
kinds actually present. Partial captures may compare equal, but the report's
`complete_record_classes` remains false unless all six classes are present.

Every subsequent line has this envelope (also a synthetic example):

```json
{"id":"1:1/1/command/0","kind":"command","sequence":1,"timestamp":1.0,"actor":"1:1","frame":{"map_generation":1,"round_generation":1,"tick":1},"data":{"forwardmove":0.0,"sidemove":0.0,"upmove":0.0,"buttons":0,"view":{"pitch":0.0,"yaw":0.0,"roll":0.0},"msec":33,"action":null}}
```

`id` is a unique semantic alignment key, assigned independently of output line
order or producer sequence: actor/frame/kind/local callback identity or ordinal
within that semantic scope is suitable. A normalizer must preserve extra
callbacks and order; it must not derive IDs from whole-file line numbers or
reassign IDs to hide missing events. `sequence` is a strictly increasing global
capture sequence; local sink sequences remain additional data. `timestamp` is
nonnegative finite game time in seconds. `actor` is `slot:generation`, or null
for genuinely global records. Frame generations are positive; tick is
nonnegative. Pointers/process addresses must never become identities.

Required `data` fields by class (additional fields also compare):

| Kind | Required fields |
|---|---|
| think | command_tick, full_think_tick, task, state, attack_overlay, enemy, nav_area, route |
| observation | self, weapon, objective, visible_enemies, noise, remembered_contacts |
| rng | call_sequence, call_site, range, type, value |
| transition | old_state, new_state, task_before, task_after, reason, side_effects |
| command | forwardmove, sidemove, upmove, buttons, view, msec, action |
| path | source_area, target_area, target_position, route_type, areas, transitions, cost |

Use `self` for origin/velocity/view/FOV/posture/life/health/armor/team; `weapon`
for active weapon/ammo/reload state; `objective` for bomb/hostage/zone state.
Retain known/unknown, freshness, quality and observation-source classifications.
Unknown values are explicit null or an observation object, never fabricated
zero/false. Position/vector objects use x/y/z; view objects use pitch/yaw/roll.
Arrays preserve observable order. Normalize equivalent public semantics only;
document mappings for task/state/target/reason IDs instead of dropping fields.

## Exact comparison and justified precision

Default comparison is exact, including timestamps, sequence, identity,
configuration, state/target/path, buttons, movement, command actions and RNG.
Missing/extra IDs are reported separately; common-ID order detects reordering.
Each common event's complete envelope and nested payload are compared, so
changed array order, missing fields and changed timing cannot be hidden by ID
alignment. Duplicate JSON keys/IDs, non-finite numbers, invalid sequences and
empty captures are errors. Numeric representation types compare exactly unless
an explicit scalar precision override applies.

An absolute epsilon requires an explicit matching evidence explanation:

```text
python tools/compare_parity_traces.py reference.jsonl candidate.jsonl --precision /data/view/yaw=0.0001 --precision-reason "/data/view/yaw=Capture manifest section 4 measured serializer rounding bound"
```

This is an option example, **not an approved measured tolerance**. The report
records every epsilon and reason. Supported scalar paths: `/timestamp`,
`/data/cost`, `/data/forwardmove`, `/data/sidemove`, `/data/upmove`,
`/data/view/{pitch,yaw,roll}`, `/data/target_position/{x,y,z}`,
`/data/self/{origin,velocity}/{x,y,z}`, `/data/self/view/{pitch,yaw,roll}`,
and `/data/self/fov`. Supply each concrete path separately. No relative epsilon,
wildcards, rounding, resampling, angle wrapping or tolerances for configuration,
IDs, sequence, RNG, buttons, msec or command order. Reasons are recorded claims;
the reviewer must verify their linked measurement evidence.

## Offline comparator verification

```text
python -m unittest discover -s tests -p test_compare_parity_traces.py -v
```

The tests generate temporary synthetic fixtures covering equal comparison,
missing/extra/reorder, configuration and identity mismatches, explicit precision,
malformed inputs and CLI exit codes. Run the pinned fixture separately to
compare actual native function results with the current Astra build.
