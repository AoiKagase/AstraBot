# Agent Instructions

## FocalSpan and completion workflow

FocalSpan is part of the standard workflow for every implementation or source
change in this repository. Do not silently skip it.

Before editing:

1. Confirm that the working directory is the AstraBot repository root.
2. Run `focalspan status --json`.
3. If the index is stale or not ready, run `focalspan update --root .` and
   resolve any error before proceeding.
4. Query the relevant architecture, policy, and existing implementation with
   `focalspan -- "<question>"` before making design or code changes.

After implementation and verification:

1. Run the tests and builds appropriate to the change, and inspect the final
   diff.
2. Run `focalspan update --root .` so the index reflects the implemented
   state. Use a follow-up FocalSpan query when needed to verify the changed
   contract or integration points.
3. Stage only the intended repository changes and run
   `git diff --cached --check`.
4. Commit the implementation and its tests/documentation. A change is not
   complete until the commit succeeds.
5. Verify the commit with `git log -1 --oneline` and `git status --short`, then
   report the commit hash, verification results, and any remaining acceptance
   work.

## Finish gate and post-Finish validation

Treat `Finish` as an explicit phase state, not as a synonym for every possible
platform or live-environment check.

- Linux builds and real-device/live-server validation must not be started
  before `Finish` has been explicitly confirmed for the phase.
- Determine and record `Finish` from the phase's implementation, applicable
  portable/target verification, and required documentation evidence first.
- After `Finish` is confirmed, run the Linux build and real-device/live-server
  checks as post-Finish validation, and report their results separately.
- If post-Finish validation fails, record the follow-up or reopened work
  explicitly; do not silently present the phase as fully accepted.

If FocalSpan is unavailable, cannot update, or cannot provide the required
context, report the blocker and do not claim the implementation is complete
without explicit user approval for an exception. Preserve unrelated user
changes. Do not stage `.focalspan/` or `.focalspan.json` unless the user
explicitly requests committing the local FocalSpan index/configuration.
