# P11 differential trace comparison

`tools/compare_parity_traces.py REFERENCE.jsonl CANDIDATE.jsonl` compares fixed
captures and prints a JSON report. Exit codes: **0** equal, **1** differences,
**2** malformed input or invalid options. Python standard library only.

## Current integration status: open

The semantic record classes come from `docs/TRACE_SCHEMA.md` and
`docs/parity/TRACE_SCHEMA.md`. Existing RNG, state, observation and timing sinks
use independent sequences and do not emit one common capture envelope. The
canonical envelope below is the comparator's input contract; **production
collector/normalizer wiring for both pinned CSBot and AstraBot is unimplemented**.
No native reference capture, production RNG replay tape, or live scenario
acceptance is supplied by this change. Synthetic unit tests exercise comparison
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
malformed inputs and CLI exit codes. Native capture, producer integration,
reference RNG replay and live behavioral acceptance remain open work.
