# CSBot RNG Consumption Parity Design

**Date:** 2026-09-19
**Phase:** P03 only
**Status:** Approved in conversation; implementation pending written-spec review
**Baseline:** 71cf00aaa9858855f0ebcff1df6544eaab3c7d4b
**Reference:** ReGameDLL-CS b0889847fe6d03898be88acc9e366660efb40ab5

## Goal

Establish an observable Compatibility RNG boundary for AstraBot that preserves
the reference RNG source, argument type, bounds, branch position, and shared
consumption order, while making deterministic call-sequence tests and bounded
parity traces possible.

P03 does not attempt to reproduce the complete CSBot behavior tree, implement
private GameDLL state, alter combat/navigation algorithms, or claim live
HLDS/ReHLDS parity.

## Evidence and constraints

- The pinned ReGameDLL-CS bot-related source contains 148 executable
  RANDOM_* expressions in 24 files: 111 RANDOM_FLOAT and 37
  RANDOM_LONG. Two additional RANDOM_LONG matches are preprocessor guards,
  not calls.
- regamedll/dlls/enginecallback.h maps RANDOM_LONG and RANDOM_FLOAT to
  g_engfuncs.pfnRandomLong and g_engfuncs.pfnRandomFloat.
- regamedll/engine/eiface.h defines the public engine callback signatures as
  inclusive integer bounds and two float arguments; ReGameDLL itself does not
  own a per-bot RNG for these calls.
- The local ReHLDS source additionally shows a process-wide engine generator
  with engine-seeded state and float [low, high) / integer inclusive
  semantics. This is recorded as the local ReHLDS implementation observation,
  not as proof of every possible HLDS runtime.
- AstraBot currently has no Compatibility RNG implementation or direct
  Compatibility RNG callsites. Its existing actor-derived navigation choice is
  a known behavioral difference and must not be silently converted in P03.
- P02's BotTimingScheduler cadence, deadline rebasing, no-catch-up behavior,
  command persistence, and msec conversion are frozen and must remain
  unchanged.
- Existing dirty worktree files are outside this design and must remain
  untouched and unstaged.

## Design

### 1. SDK-free Compatibility RNG contract

Add a small contract under include/astrabot/compat/ with value-only types:

- RandomType: Float or Long.
- RandomCallSite: a semantic identifier string owned by the caller; source
  line numbers are optional documentation only.
- RandomActor: stable slot/generation identity, with an explicit unknown
  value for manager-level calls.
- RandomTimingContext: command, upkeep, and full-update sequence values;
  zero means the caller has not associated a scheduler event.
- RandomRequest: callsite, type-specific bounds, actor, and timing context.
- RandomTraceRecord: global sequence, request metadata, and returned value.

The public source contract exposes separate nextFloat and nextLong operations so
callers cannot silently convert one distribution to the other or normalize
bounds. Callers provide a semantic ID at the exact branch where the reference
consumes randomness. No constructor, default member initializer, temporary, or
helper argument may evaluate a random request eagerly.

The contract does not include edict_t, Vector, enginefuncs_t, GameDLL private
classes, or a seed setter.

### 2. Production adapter and ownership

Implement the production adapter in the Metamod layer using injected function
pointers matching the public engine callback signatures. It calls
pfnRandomFloat and pfnRandomLong exactly once per request and forwards the
original bounds unchanged.

PluginRuntime owns one Compatibility source shared by all managed bots and
manager-level decisions. It does not create one tape, seed, or generator per
bot. This preserves the reference global-stream ordering when Bot A and Bot B
are processed in one server-frame order.

The adapter does not contain std::mt19937, rand, xorshift, PCG, custom seed
policy, or a copied engine algorithm. If an engine callback is unavailable, the
adapter fails closed and exposes an invalid/unavailable result to its caller.

### 3. Scripted/tape source

Implement a deterministic test source with a single ordered tape shared by its
consumer. Each tape entry records:

- expected type;
- exact lower and upper bounds;
- returned float or integer value;
- optional expected semantic callsite.

Consumption fails when the caller requests more entries than the tape holds,
when type or bounds differ, or when an optional callsite assertion differs.
verifyComplete() fails when entries remain at fixture end. The source records
all accepted and rejected requests so tests can inspect exact order without
using a generator algorithm.

Enhanced tests use a separate source instance/tape. Enhanced calls therefore
cannot advance the Compatibility tape. P03 does not add seeded personalities,
learning randomness, replay RNG, or another enhanced generator.

### 4. Trace integration

Add an optional trace sink to the RNG boundary. When enabled, each accepted
Compatibility request emits one bounded record containing:

~~~text
sequence
semantic callsite ID
type
min
max
result
bot/entity identity
command sequence
upkeep sequence
full-update sequence
~~~

The sequence counter belongs to the shared Compatibility source, not to a Bot
instance. Trace-disabled production operation performs no unbounded logging.
The scheduler itself is not changed; timing context is supplied by the caller
around existing scheduler events. This keeps P02 cadence and msec semantics
out of the RNG implementation.

Update docs/parity/TRACE_SCHEMA.md with the rng record contract and its
limits. A synthetic RNG trace remains offline evidence and cannot close live
parity.

### 5. Reference inventory

Create docs/parity/RNG_MODEL.md as a reviewable, machine-readable-oriented
Markdown table. Each direct reference expression receives a semantic ID based
on subsystem, behavior, and purpose rather than a line number. Each row
records:

~~~text
semantic ID
reference source/function
line (auxiliary)
type
bounds
condition
purpose
scope
behavior influence
classification
order constraints
AstraBot equivalent
status
~~~

The inventory includes manager, profile selection, navigation/path selection,
combat/weapon/aim, objective, chatter/radio, hiding, event/listen/vision,
helper, and state callsites. It explicitly records UNKNOWN entries where
reachability or influence cannot be established. The audit also records that
the bot paths do not directly call UTIL_SharedRandom*; unrelated engine-wide
random users remain outside the P03 Compatibility call inventory unless a
transitive CSBot path is proven.

Categories are limited to:

~~~text
COMPAT_BEHAVIOR_CRITICAL
COMPAT_MANAGER_CRITICAL
COMPAT_NAV_CRITICAL
COSMETIC_ONLY
CHATTER_ONLY
DEBUG_ONLY
ENHANCED_ONLY
UNKNOWN
~~~

No row is promoted to MATCH merely because a source adapter exists. A row is
only a verified candidate when branch, type, bounds, and call order are proven
by an AstraBot fixture or runtime evidence.

### 6. Behavioral fixture

Add a deterministic reference-shaped aim-offset fixture to the RNG tests. It
will model the observed cs_bot_weapon.cpp multi-call shape without changing
the production CombatController:

~~~text
FLOAT(-error, error) -> aim X
FLOAT(-error, error) -> aim Y
FLOAT(-error, error) -> aim Z
FLOAT(0.25, upper timestamp) -> next aim timestamp
~~~

The fixture asserts input state, semantic callsite IDs, exact bounds, tape
order, and the resulting decision values. Its result is a call-consumption
fixture, not a claim that AstraBot's current combat behavior already matches
CSBot.

## Test contract

Add a focused CTest executable covering:

1. float bound forwarding;
2. integer bound forwarding;
3. exact mixed FLOAT/FLOAT/LONG/FLOAT tape order;
4. too-many-call failure;
5. too-few-call detection;
6. false-branch non-consumption;
7. true-branch one-call consumption;
8. multi-call behavioral fixture order and bounds;
9. shared multi-Bot ordering through one source;
10. Enhanced source isolation from Compatibility position;
11. Compatibility/Enhanced baseline equivalence when extensions are disabled;
12. unchanged P02 timing tests;
13. production adapter callback forwarding using fake engine functions;
14. a lightweight source check preventing new direct random bypasses in the
    Compatibility implementation.

The test executable must fail through its normal exit status for tape
exhaustion, mismatch, or incomplete consumption. Existing tests must not be
deleted, disabled, or skipped.

## Documentation/status updates

Update only the P03-relevant parity documents:

- docs/parity/RNG_MODEL.md — new reference inventory and AstraBot mapping;
- docs/parity/TRACE_SCHEMA.md — RNG trace record;
- docs/parity/SOURCE_MAP.md — Compatibility RNG and production boundary;
- docs/parity/PARITY_MATRIX.md — boundary/callsite statuses with evidence;
- docs/parity/KNOWN_DEVIATIONS.md — global engine ownership, unmapped
  callsites, and live acceptance gap;
- docs/parity/STATUS.md — P03 result and validation evidence;
- docs/parity/TIMING_MODEL.md only if a cross-reference is needed; no timing
  behavior or scheduler wording may change.

The final result is expected to be PARTIAL unless all claimed callsites have
branch/type/bounds/order evidence. Offline CTest, FocalSpan, and a synthetic
fixture do not become live HLDS/ReHLDS acceptance.

## Out of scope

- P04 or any later phase;
- private-state observation or CSBot state-machine reconstruction;
- combat, aim, weapon, navigation, objective, chatter, or probability tuning;
- replacing the reference global stream with per-Bot streams;
- adding a custom seed policy or random algorithm;
- permanent ReGameDLL instrumentation;
- live HLDS/ReHLDS execution unless separately available and explicitly within
  the P03 validation gate;
- unrelated refactoring or changes to existing dirty files.

## Acceptance gates

P03 is complete only after the implementation is validated with a fresh:

1. x86 configure;
2. complete Debug build in the matching VS HostX86/x86 NMake environment;
3. focused RNG CTest;
4. full CTest;
5. Phase 8 PowerShell checks;
6. focalspan update --root ., fresh status, and a follow-up query;
7. staged path/diff review and git diff --cached --check.

Python/PE verification and live HLDS/ReHLDS acceptance remain separately
reported when the environment cannot execute them.
