# P11 — Differential Oracle, Golden Traces and Replay

## Goal

Replace subjective parity assessment with a repeatable machine comparison between the pinned ReGameDLL CSBot and Astra compatibility mode.

## Principle

Do not require internal class names to match. Compare the observable contract and normalized semantic trace.

## Tasks

1. Finalize `docs/parity/TRACE_SCHEMA.md`.
2. Create a trace writer for Astra compatibility mode with deterministic entity ids and ordering.
3. Create a **separate reference instrumentation path** for the pinned ReGameDLL tree used only for generating oracle traces. Keep reference modifications outside Astra production code. Store patch/script instructions if useful, not copied reference implementation.
4. Capture at minimum:
   - command/full-think ticks;
   - observations that drive decisions;
   - RNG sequence;
   - task/state transitions;
   - enemy/goal/path selection;
   - final movement/buttons/view command.
5. Build a comparator that classifies mismatch by domain rather than dumping a giant textual diff.
6. Support exact fields and narrowly documented float epsilon fields.
7. Add golden fixtures for small synthetic scenarios before long live traces.
8. Use random-tape mode to remove generator divergence while validating call order/branch semantics.
9. Once semantic parity is good, optionally compare normal random operation statistically; do not use statistics as a substitute for deterministic branch tests.
10. Integrate existing Astra replay/differential infrastructure instead of duplicating it if suitable.
11. Add a one-command or clearly documented sequence to:
   - generate/reference trace;
   - run Astra replay;
   - compare;
   - emit a compact mismatch report.
12. Do not store enormous live traces in Git unless intentionally curated. Keep minimal golden fixtures in-tree and document local trace storage.

## Mismatch priority

Fix in this order:

1. scheduling/tick mismatch;
2. observation mismatch;
3. RNG sequence mismatch;
4. state/task transition mismatch;
5. path/goal mismatch;
6. combat/action mismatch;
7. final command float noise.

Never patch a later layer to hide an earlier mismatch.

## Acceptance criteria

- a reproducible ReGameDLL oracle trace can be generated from the pinned reference;
- Astra trace can be generated/replayed for the same fixture;
- comparator produces domain-specific mismatches;
- at least one multi-step state/navigation/combat fixture reaches zero meaningful diff;
- existing parity matrix rows can link to trace fixture names;
- build/smoke gate passes.

## Commit

Suggested message:

`test(parity): add CSBot differential trace oracle and comparator`

Update STATUS/matrix, mark P11 complete, set P12 next, commit, stop.
