# P03 — RNG and Deterministic Replay Foundation

## Goal

Make compatibility randomness observable and replayable so two implementations can be compared without divergence caused merely by different random streams.

## Why this precedes behavior tuning

CSBot uses randomness in combat, hiding, morale, strategy, timing and initialization. Equivalent probability distributions are insufficient for deterministic differential comparison. Call order matters.

## Tasks

1. Inventory every RNG call reachable from compatibility behavior in current AstraBot.
2. Inventory behaviorally relevant RNG calls in the pinned CSBot reference. Group by semantic call site; do not copy implementation bodies.
3. Introduce/reuse one compatibility RNG abstraction that supports:
   - normal engine-compatible/random operation;
   - record mode;
   - replay/tape mode;
   - a stable semantic call-site id;
   - typed/ranged values.
4. Ensure enhanced-only code does not consume compatibility RNG in compatibility mode.
5. Decide and document how ReGameDLL/GoldSrc random semantics will be matched. If exact generator state cannot be shared externally, the required minimum is a random-tape oracle that feeds identical results to both sides for differential tests.
6. Add trace records for RNG sequence number, call-site id, requested range/type and produced value.
7. Add tests that prove:
   - same tape -> same compatibility decisions;
   - one extra random call is detected as a sequence mismatch;
   - enhanced mode activity cannot shift compatibility-mode RNG sequence.
8. Do not replace unrelated non-compatibility random utilities unless necessary.

## Acceptance criteria

- compatibility RNG is centralized/traceable enough for differential fixtures;
- call sequence mismatches are visible rather than silently tolerated;
- deterministic replay test passes across repeated runs;
- no behavior is changed merely to make RNG easier to test;
- build/smoke gate passes.

## Commit

Suggested message:

`test(parity): add deterministic RNG tape for compatibility behavior`

Update STATUS/matrix, mark P03 complete, set P04 next, commit, stop.
